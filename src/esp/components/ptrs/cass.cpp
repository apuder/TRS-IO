

#include "cass.h"
#include "spi.h"
#include "event.h"
#include "esp_adc/adc_continuous.h"
#include "driver/gptimer.h"
#include "esp_log.h"
#include <cstring>

#define TAG "CASS"

// The cassette input is sampled by the ADC (ADC1 channel 0, GPIO36) in
// continuous mode, which uses I2S0 on the ESP32 as the I2S driver's ADC mode
// did before ESP-IDF 5. Each conversion result is 2 bytes.
#define DMA_BUFFER_COUNT 2
#define DMA_BUFFER_SIZE_IN_BYTES (DMA_BUFFER_SIZE * sizeof(adc_digi_output_data_t))

// Plays back the samples at the rate they were taken
#define TIMER_RESOLUTION_HZ 1000000
#define TIMER_ALARM   (TIMER_RESOLUTION_HZ / I2S_SAMPLING_FREQ)

static TaskHandle_t s_task_handle;
static adc_continuous_handle_t s_adc;
static gptimer_handle_t s_timer;
static volatile uint32_t s_adc_overflows;

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


// Conversion results read into buf; returns how many
static uint32_t adc_read(adc_digi_output_data_t* buf, int buf_size)
{
  uint32_t br = 0;
  esp_err_t err = adc_continuous_read(s_adc, (uint8_t*) buf, buf_size, &br, 100);
  if (err == ESP_ERR_TIMEOUT) {
    return 0;
  }
  ESP_ERROR_CHECK(err);
  return br / sizeof(adc_digi_output_data_t);
}

static void i2sRead(adc_digi_output_data_t* buf, int buf_size)
{
  uint32_t overflows = s_adc_overflows;

  ESP_ERROR_CHECK(adc_continuous_start(s_adc));

  while (!evt_check(EVT_I2S_STOP)) {
    const uint32_t n = adc_read(buf, buf_size);
    if (s_adc_overflows != overflows) {
      overflows = s_adc_overflows;
      ESP_LOGW(TAG, "ADC buffer overflow, data dropped [%u]", (unsigned) overflows);
    }

    for(int i = 0; i < n; i++) {
      // 8 bit samples, as before
      uint16_t sample = buf[i].type1.data >> 4;
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

  ESP_ERROR_CHECK(adc_continuous_stop(s_adc));
}


static void determine_signal_center(adc_digi_output_data_t* buf, int buf_size)
{
  int center_cnt = 0;

  ESP_ERROR_CHECK(adc_continuous_start(s_adc));

  while (center_cnt < I2S_SAMPLING_FREQ) {
    const uint32_t n = adc_read(buf, buf_size);
    for(int i = 0; i < n; i++) {
      uint16_t sample = buf[i].type1.data >> 4;
      signal_center = (signal_center * 99 + sample) / 100.0;
      center_cnt++;
    }
  }

  ESP_ERROR_CHECK(adc_continuous_stop(s_adc));
  ESP_LOGI(TAG, "Signal center: %f", signal_center);
}


static void i2sTask(void * arg)
{
  adc_digi_output_data_t* buf = (adc_digi_output_data_t*) malloc(DMA_BUFFER_SIZE_IN_BYTES * DMA_BUFFER_COUNT);
  determine_signal_center(buf, DMA_BUFFER_SIZE_IN_BYTES * DMA_BUFFER_COUNT);

  while (true) {
    evt_wait(EVT_I2S_START);
    i2sRead(buf, DMA_BUFFER_SIZE_IN_BYTES * DMA_BUFFER_COUNT);
  }
}

static bool IRAM_ATTR timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* args)
{
  BaseType_t mustYield = pdFALSE;
  vTaskNotifyGiveFromISR(s_task_handle, &mustYield);
  return (mustYield == pdTRUE);
}

static void init_timer()
{
  gptimer_config_t config = {};
  config.clk_src = GPTIMER_CLK_SRC_DEFAULT;
  config.direction = GPTIMER_COUNT_UP;
  config.resolution_hz = TIMER_RESOLUTION_HZ;
  ESP_ERROR_CHECK(gptimer_new_timer(&config, &s_timer));

  gptimer_event_callbacks_t callbacks = {};
  callbacks.on_alarm = timer_isr;
  ESP_ERROR_CHECK(gptimer_register_event_callbacks(s_timer, &callbacks, NULL));

  gptimer_alarm_config_t alarm = {};
  alarm.alarm_count = TIMER_ALARM;
  alarm.reload_count = 0;
  alarm.flags.auto_reload_on_alarm = true;
  ESP_ERROR_CHECK(gptimer_set_alarm_action(s_timer, &alarm));

  ESP_ERROR_CHECK(gptimer_enable(s_timer));
  ESP_ERROR_CHECK(gptimer_start(s_timer));
}

static void deinit_timer()
{
  ESP_ERROR_CHECK(gptimer_stop(s_timer));
  ESP_ERROR_CHECK(gptimer_disable(s_timer));
  ESP_ERROR_CHECK(gptimer_del_timer(s_timer));
  s_timer = NULL;
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

// The ADC's buffer filled up before the task read it
static bool IRAM_ATTR adc_overflow(adc_continuous_handle_t handle, const adc_continuous_evt_data_t* edata, void* args)
{
  s_adc_overflows++;
  return false;
}

void init_cass()
{
  adc_continuous_handle_cfg_t handle_config = {};
  handle_config.max_store_buf_size = 4 * DMA_BUFFER_SIZE_IN_BYTES;
  handle_config.conv_frame_size = DMA_BUFFER_SIZE_IN_BYTES;
  ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_config, &s_adc));

  // As the I2S driver's ADC mode had it: 12 bits, 11 dB (now called 12 dB)
  adc_digi_pattern_config_t pattern = {};
  pattern.atten = ADC_ATTEN_DB_12;
  pattern.channel = ADC_CHANNEL_0;
  pattern.unit = ADC_UNIT_1;
  pattern.bit_width = ADC_BITWIDTH_12;

  adc_continuous_config_t adc_config = {};
  adc_config.pattern_num = 1;
  adc_config.adc_pattern = &pattern;
  adc_config.sample_freq_hz = I2S_SAMPLING_FREQ;
  adc_config.conv_mode = ADC_CONV_SINGLE_UNIT_1;
  ESP_ERROR_CHECK(adc_continuous_config(s_adc, &adc_config));

  adc_continuous_evt_cbs_t callbacks = {};
  callbacks.on_pool_ovf = adc_overflow;
  ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(s_adc, &callbacks, NULL));

  xTaskCreatePinnedToCore(cassTask, "cass", 3000, NULL, configMAX_PRIORITIES - 1, NULL, 1);
  xTaskCreatePinnedToCore(i2sTask, "i2s", 3000, NULL, configMAX_PRIORITIES - 2, NULL, 1);
}
