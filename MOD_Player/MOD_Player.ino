// Amiga module player //

#define SAMPLE_RATE 22050

#include "mod_player.h"
#include "fairlight_quake.h"

mod_ctx player;

void setup() {

  PORTD.PIN0CTRL = PORT_ISC_INPUT_DISABLE_gc; 
  VREF.ADC0REF = VREF_REFSEL_VDD_gc; 
  ADC0.CTRLA = ADC_ENABLE_bm | ADC_RESSEL_12BIT_gc;
  ADC0.CTRLC = ADC_PRESC_DIV16_gc;
  ADC0.MUXPOS = ADC_MUXPOS_AIN0_gc; 
  ADC0.COMMAND = ADC_STCONV_bm;
  while (!(ADC0.INTFLAGS & ADC_RESRDY_bm));
  uint16_t entropy = ADC0.RES;

  randomSeed(entropy);

  mod_init(&player, song, sizeof(song), SAMPLE_RATE/2);

  PORTD.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
  VREF.DAC0REF = VREF_REFSEL_1V024_gc | VREF_ALWAYSON_bm;
  DAC0.CTRLA = DAC_ENABLE_bm | DAC_OUTEN_bm | DAC_RUNSTDBY_bm;

  PORTD.PIN2CTRL = PORT_ISC_INPUT_DISABLE_gc;
  OPAMP.CTRLA = OPAMP_ENABLE_bm; 
  OPAMP.TIMEBASE = 23; 
  OPAMP.OP0CTRLA = OPAMP_ALWAYSON_bm | OPAMP_OP0CTRLA_OUTMODE_NORMAL_gc;
  OPAMP.OP0SETTLE = 0x7F; 
  OPAMP.OP0INMUX = OPAMP_OP0INMUX_MUXPOS_DAC_gc | OPAMP_OP0INMUX_MUXNEG_OUT_gc;

  TCB0.CTRLA = 0;
  TCB0.CTRLB = TCB_CNTMODE_INT_gc;
  TCB0.CCMP = (F_CPU / SAMPLE_RATE) - 1;
  TCB0.INTCTRL = TCB_CAPT_bm;
  TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_CLKDIV1_gc;

}

void loop() {

}

ISR(TCB0_INT_vect) {

  uint16_t val = 512 + (mod_render_sample(&player, 1) >> 6);

  DAC0.DATAL = (val & 0x03) << 6;
  DAC0.DATAH = val >> 2;

  TCB0.INTFLAGS = TCB_CAPT_bm;

}