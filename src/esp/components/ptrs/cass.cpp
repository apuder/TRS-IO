

#include "cass.h"
#include "spi.h"
#include "event.h"
#include "hal/timer_types.h"
#include "driver/timer.h"
#include "esp_log.h"
#include <cstring>

#define TAG "CASS"

#define DMA_BUFFER_COUNT 2
#define DMA_BUFFER_SIZE_IN_BYTES (DMA_BUFFER_SIZE * sizeof(uint16_t))

#define TIMER_DIVIDER 16
#define TIMER_SCALE   (TIMER_BASE_CLK / TIMER_DIVIDER)
#define TIMER_ALARM   (TIMER_SCALE / I2S_SAMPLING_FREQ)

static TaskHandle_t s_task_handle;

static float cassette_avg;
static float cassette_env;
static float signal_center;
static int cassette_noisefloor;



TRSSamplesGenerator::TRSSamplesGenerator()
{
  curr_read = 0;
  curr_read_cnt = 0;
  curr_write = 0;
  curr_write_cnt = 0;
}

void TRSSamplesGenerator::lock()
{
  portDISABLE_INTERRUPTS();
}

void TRSSamplesGenerator::unlock()
{
  portENABLE_INTERRUPTS();
}

bool TRSSamplesGenerator::detectPulse(uint16_t sample, int cassette_noisefloor)
{
  samples[sample_idx++] = sample;
  if (sample_idx == NUM_PULSE_SAMPLES) {
    sample_idx = 0;
  }
  uint16_t min_sample = 0xffff;
  uint16_t max_sample = 0;
  for (int i = 0; i < NUM_PULSE_SAMPLES; i++) {
    uint16_t curr_sample = samples[i];
    if (curr_sample > max_sample) {
      max_sample = curr_sample;
    }
    if (curr_sample < min_sample) {
      min_sample = curr_sample;
    }
  }
  return (max_sample > signal_center + (cassette_noisefloor)) &&
         (min_sample < signal_center - (cassette_noisefloor));
}

void TRSSamplesGenerator::putSample(uint16_t sample, int cassette_noisefloor)
{
  curr_write <<= 1;
  if (detectPulse(sample, cassette_noisefloor)) {
    curr_write |= 1;
  }
  curr_write_cnt++;
  if (curr_write_cnt != 8) {
    return;
  }
  lock();
  curr_write_cnt = 0;
  *sound_ring_write_ptr++ = curr_write;
  if (sound_ring_write_ptr == sound_ring_end) {
    sound_ring_write_ptr = sound_ring;
  }
  if (sound_ring_write_ptr == sound_ring_read_ptr) {
    // Overflow
    sound_ring_read_ptr++;
    if (sound_ring_read_ptr == sound_ring_end) {
      sound_ring_read_ptr = sound_ring;
    }
  }
  unlock();
}

int TRSSamplesGenerator::getSample() {
  int sample = 0;
  static int last_sample = 0;
  
  if (curr_read_cnt == 8) {
    lock();
    if (sound_ring_read_ptr != sound_ring_write_ptr) {
      curr_read_cnt = 0;
      curr_read = *sound_ring_read_ptr++;
      if (sound_ring_read_ptr == sound_ring_end) {
        sound_ring_read_ptr = sound_ring;
      }
    } else {
      // Buffer is empty
      unlock();
      return last_sample;
    }
    unlock();
  }

  sample = (curr_read & 0x80) != 0;
  curr_read <<= 1;
  curr_read_cnt++;

  last_sample = sample;

  return sample;
}

static TRSSamplesGenerator* trsSamplesGenerator = NULL;


static void i2sRead(uint16_t* buf, int buf_size)
{
  ESP_ERROR_CHECK(i2s_adc_enable(I2S_NUM_0));

  while (!evt_check(EVT_I2S_STOP)) {
    size_t br;
    ESP_ERROR_CHECK(i2s_read(I2S_NUM_0, (void*) buf, buf_size, &br, portMAX_DELAY));

    for(int i = 0; i < br / 2; i++) {
      uint16_t sample = buf[i];
      sample >>= 4;
      trsSamplesGenerator->putSample(sample, cassette_noisefloor);
      /* Attempt to learn the correct noise cutoff adaptively.
       * This code is just a hack; it would be nice to know a
       * real signal-processing algorithm for this application
       */
      int cabs = abs(sample - signal_center);
      if (cabs > 1) {
        cassette_avg = (99 * cassette_avg + cabs) / 100;
      }
      if (cabs > cassette_env) {
        cassette_env = (cassette_env + 9 * cabs) / 10;
      } else if (cabs > 10) {
        cassette_env = (99 * cassette_env + cabs) / 100;
      }
      cassette_noisefloor = (cassette_avg + cassette_env) / 2;
    }
  }

  ESP_ERROR_CHECK(i2s_adc_disable(I2S_NUM_0));
}


static void determine_signal_center(uint16_t* buf, int buf_size)
{
  int center_cnt = 0;
  size_t br;

  ESP_ERROR_CHECK(i2s_adc_enable(I2S_NUM_0));

  while (center_cnt < I2S_SAMPLING_FREQ) {
    size_t br;
    ESP_ERROR_CHECK(i2s_read(I2S_NUM_0, (void*) buf, buf_size, &br, portMAX_DELAY));
    for(int i = 0; i < br / 2; i++) {
      uint16_t sample = buf[i];
      sample >>= 4;
      signal_center = (signal_center * 99 + sample) / 100.0;
      center_cnt++;
    }
  }

  ESP_ERROR_CHECK(i2s_adc_disable(I2S_NUM_0));
  ESP_LOGI(TAG, "Signal center: %f", signal_center);
}


static void i2sTask(void * arg)
{
  uint16_t* buf = (uint16_t*) malloc(DMA_BUFFER_SIZE_IN_BYTES * DMA_BUFFER_COUNT);
  determine_signal_center(buf, DMA_BUFFER_SIZE_IN_BYTES * DMA_BUFFER_COUNT);

  while (true) {
    evt_wait(EVT_I2S_START);
    i2sRead(buf, DMA_BUFFER_SIZE_IN_BYTES * DMA_BUFFER_COUNT);
  }
}

static bool IRAM_ATTR timer_isr(void *args)
{
  BaseType_t mustYield = pdFALSE;
  vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
  return (mustYield == pdTRUE);
}

static void init_timer()
{
  const timer_group_t group = TIMER_GROUP_0;
  const timer_idx_t timer = TIMER_0;

  timer_config_t config;
  memset(&config, 0, sizeof(timer_config_t));
  config.divider = TIMER_DIVIDER;
  config.counter_dir = TIMER_COUNT_UP;
  config.counter_en = TIMER_PAUSE;
  config.alarm_en = TIMER_ALARM_EN;
  config.auto_reload = TIMER_AUTORELOAD_EN;

  timer_init(group, timer, &config);
  timer_set_counter_value(group, timer, 0);
  timer_set_alarm_value(group, timer, TIMER_ALARM);
  timer_enable_intr(group, timer);
  timer_isr_callback_add(group, timer, timer_isr, NULL, 0);
  timer_start(group, timer);
}

static void deinit_timer()
{
  const timer_group_t group = TIMER_GROUP_0;
  const timer_idx_t timer = TIMER_0;

  timer_pause(group, timer);
  timer_isr_callback_remove(group, timer);
  timer_deinit(group, timer);
}

static void cassTask(void* arg)
{
  uint8_t last_sample = 0;

  s_task_handle = xTaskGetCurrentTaskHandle();

  while(true) {
    evt_wait(EVT_CASS_MOTOR_ON);
    ESP_LOGI(TAG, "Cassette motor on");

    if (trsSamplesGenerator != NULL) {
      delete trsSamplesGenerator;
    }
    trsSamplesGenerator = new TRSSamplesGenerator();

    cassette_avg = NOISE_FLOOR;
    cassette_env = signal_center;
    cassette_noisefloor = NOISE_FLOOR;
    
    evt_signal(EVT_I2S_START);

    init_timer();

    while(!evt_check(EVT_CASS_MOTOR_OFF)) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      uint8_t new_sample = trsSamplesGenerator->getSample();
      if (new_sample != last_sample) {
        if (new_sample != 0) {
          spi_set_cass_in();
        }
        last_sample = new_sample;
      }
    }
    deinit_timer();
    evt_signal(EVT_I2S_STOP);
    ESP_LOGI(TAG, "Cassette motor off");
  }
}


#if 0

#define COUNTER_250US 5
#define COUNTER_1MS   25

static uint8_t getNextCASByte()
{
  return 0;
}

static void sendCAS(void* arg)
{
  int timer_cnt = 0;
  int bit_cnt = 0;
  uint8_t current_byte;

  s_task_handle = xTaskGetCurrentTaskHandle();

  while(true) {
    evt_wait(EVT_CASS_MOTOR_ON);

    init_timer();

    timer_cnt = 0;
    bit_cnt = 0;

    ESP_LOGI(TAG, "Start sending CAS data");

    while(!evt_check(EVT_CASS_MOTOR_OFF)) {
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
      if (bit_cnt == 0) {
        current_byte = getNextCASByte();
        bit_cnt = 8;
      }
      switch (timer_cnt) {
        case 0:
          spi_set_cass_in();
          timer_cnt++;
          break;
        case COUNTER_1MS:
          if (current_byte & 0x80) {
            spi_set_cass_in();
          }
          timer_cnt++;
          break;
        case 2 * COUNTER_1MS:
          current_byte <<= 1;
          bit_cnt--;
          timer_cnt = 0;
          break;
        default:
          timer_cnt++;
          break;
      }
    }
    ESP_LOGI(TAG, "Stop sending CAS data");
    evt_signal(EVT_I2S_STOP);

    deinit_timer();
  }
}
#endif

static QueueHandle_t evt_que;

static void i2s_event(void *args)
{
  i2s_event_t evt;
  uint32_t cnt = 0;
  while (true) {
    xQueueReceive(evt_que, &evt, portMAX_DELAY);
    if (evt.type == I2S_EVENT_RX_Q_OVF) {
      ESP_LOGW(TAG,"I2S queue overflow, data dropped [%d]", cnt++);
    }
  }
  vTaskDelete(NULL);
}

void init_cass()
{
  esp_err_t ret;
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t) (I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate          = I2S_SAMPLING_FREQ,
    .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format       = I2S_CHANNEL_FMT_ONLY_RIGHT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags     = 0,
    .dma_buf_count        = DMA_BUFFER_COUNT,
    .dma_buf_len          = DMA_BUFFER_SIZE * sizeof(uint16_t),
    .use_apll             = false,
    .tx_desc_auto_clear   = 0,
    .fixed_mclk           = 0
  };

  ret = i2s_driver_install(I2S_NUM_0, &i2s_config, 10, &evt_que);// NULL);
  ESP_ERROR_CHECK(ret);
  xTaskCreatePinnedToCore(i2s_event, "i2s_watch", 1024, NULL, tskIDLE_PRIORITY + 5, NULL, 1);
  ret = i2s_set_adc_mode(ADC_UNIT_1, ADC1_CHANNEL_0);
  ESP_ERROR_CHECK(ret);

  i2s_set_clk(I2S_NUM_0, I2S_SAMPLING_FREQ,
        I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);

  ret = i2s_start(I2S_NUM_0);
  ESP_ERROR_CHECK(ret);

  xTaskCreatePinnedToCore(cassTask, "cass", 3000, NULL, configMAX_PRIORITIES - 1, NULL, 1);
  xTaskCreatePinnedToCore(i2sTask, "i2s", 3000, NULL, configMAX_PRIORITIES - 2, NULL, 1);
}
