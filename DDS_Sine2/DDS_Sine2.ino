// DDS sinewave generator (linear interpolation) //

#define SAMPLE_RATE 44100
#define SIZE        256
#define BPM         120

float freq = 110;

uint16_t sineTable[SIZE+1];

volatile uint32_t phaseAccumulator = 0;
volatile uint32_t phaseIncrement = 0;

void setFrequency(float freq) { phaseIncrement = (uint32_t)(((uint64_t)freq << 32) / SAMPLE_RATE); }

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

  for (int i = 0; i < SIZE; i++) {
    float t = (float)i / SIZE * 2 * PI;
    sineTable[i] = (uint16_t)(512.0f + 511.0f * sinf(t));
  }

  sineTable[256] = sineTable[0];

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

  freq = random(110, 880);
  setFrequency(freq);

  float tempo = 60000.0 / BPM;
  delay((int)(tempo / 4));

}

ISR(TCB0_INT_vect) {

  phaseAccumulator += phaseIncrement;

  uint8_t tableIndex = phaseAccumulator >> 24;
  uint8_t fraction = (phaseAccumulator >> 16) & 0xFF;
  uint16_t y1 = sineTable[tableIndex];
  uint16_t y2 = sineTable[tableIndex + 1];

  int32_t diff = (int32_t)y2 - (int32_t)y1;
  uint16_t val = y1 + ((diff * fraction) >> 8);

  DAC0.DATAL = (val & 0x03) << 6;
  DAC0.DATAH = val >> 2;

  TCB0.INTFLAGS = TCB_CAPT_bm;

}