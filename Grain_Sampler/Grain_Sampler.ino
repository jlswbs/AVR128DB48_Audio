// Grain sampler with comb delay //

#include "sample.h"

#define SAMPLE_RATE 22050
#define BPM         120
#define DELAY_SIZE  689

uint16_t delay_buffer[DELAY_SIZE];
volatile uint32_t index = 0;
volatile uint32_t grain_start = 0;
volatile uint32_t grain_end = 0;
volatile uint8_t feedback = 100;
uint16_t delay_index = 0;
bool direction = false;

void setup() {

  randomSeed(analogRead(A0));

  PORTD.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
  //VREF.DAC0REF = VREF_REFSEL_VDD_gc | VREF_ALWAYSON_bm;
  VREF.DAC0REF = VREF_REFSEL_2V048_gc | VREF_ALWAYSON_bm;
  DAC0.CTRLA = DAC_ENABLE_bm | DAC_OUTEN_bm | DAC_RUNSTDBY_bm;

  TCB0.CTRLA = 0; 
  TCB0.CTRLB = TCB_CNTMODE_INT_gc; 
  TCB0.CCMP = (F_CPU / SAMPLE_RATE) - 1; 
  TCB0.INTCTRL = TCB_CAPT_bm; 
  TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_CLKDIV1_gc;

}

void loop() {

  uint32_t grain_length = random(5, 50) * 22; 
  uint32_t max_start = sizeof(table) - grain_length;
  grain_start = random(0, max_start);
  feedback = random(100, 255);
  direction = random(0, 2);

  noInterrupts();

  index = grain_start;
  grain_end = grain_start + grain_length;

  interrupts();

  float tempo = 60000.0 / BPM;
  delay((int)tempo);

}

ISR(TCB0_INT_vect) {

  if (direction == false) {
    if (index >= grain_end || index >= sizeof(table)) {
      index = grain_end - (grain_end - index);
      if (index >= sizeof(table)) index = 0;
    }
  } else {
    if (index == 0 || index < (grain_end - (grain_end - index)) || index >= sizeof(table)) {
      index = grain_end - 1;
    }
  }

  uint32_t far_address = pgm_get_far_address(table) + index;
  uint8_t raw_byte = pgm_read_byte_far(far_address);
  
  if (direction == false) { index++; } 
  else { index--; }

  int16_t sample_signed = ((int16_t)raw_byte - 128) * 4;
  int16_t delay_signed = (int16_t)delay_buffer[delay_index] - 512;
  int32_t mixed_signed = (int32_t)sample_signed + delay_signed;

  if (mixed_signed > 511) mixed_signed = 511;
  if (mixed_signed < -512) mixed_signed = -512;

  int32_t next_delay_signed = (int32_t)sample_signed + (((int32_t)delay_signed * feedback) >> 8);
  
  if (next_delay_signed > 511) next_delay_signed = 511;
  if (next_delay_signed < -512) next_delay_signed = -512;

  delay_buffer[delay_index] = (uint16_t)(next_delay_signed + 512);
  
  delay_index++;
  if (delay_index >= DELAY_SIZE) { delay_index = 0; }

  uint16_t val = (uint16_t)(mixed_signed + 512);
  
  DAC0.DATAL = (val & 0x03) << 6;
  DAC0.DATAH = val >> 2;

  TCB0.INTFLAGS = TCB_CAPT_bm;

}