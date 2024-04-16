
#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/i2s.h>
#include <driver/adc.h>


#define NOISE_FLOOR 64
#define SIGNAL_CENTER 127

#define I2S_SAMPLING_FREQ 25000
#define RING_BUFFER_SIZE 12000
#define DMA_BUFFER_SIZE 512

#define SPEED_500     0
#define SPEED_1500    1
#define SPEED_250     2

typedef unsigned char Uchar;
typedef unsigned char Uint8;
typedef unsigned short Uint16;
typedef Uint16 Ushort;
typedef unsigned int Uint32;


#define SOUND_RING_SIZE 32000

#define NUM_PULSE_SAMPLES 10


class TRSSamplesGenerator {
private:
  Uint8 sound_ring[SOUND_RING_SIZE];
  Uint8 *sound_ring_read_ptr = sound_ring;
  Uint8 *sound_ring_write_ptr = sound_ring;
  Uint8 *sound_ring_end = sound_ring + SOUND_RING_SIZE;

  uint16_t samples[NUM_PULSE_SAMPLES];
  int      sample_idx = 0;

  Uint8 curr_read;
  Uint8 curr_read_cnt;
  Uint8 curr_write;
  Uint8 curr_write_cnt;

public:
  TRSSamplesGenerator();

  void lock();
  void unlock();
  bool detectPulse(uint16_t sample, int cassette_noisefloor);
  void putSample(uint16_t sample, int cassette_noisefloor);
  int getSample();
};


void init_cass();
