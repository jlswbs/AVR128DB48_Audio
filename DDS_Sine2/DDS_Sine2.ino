// DDS sinewave generator (linear interpolation) //

#define SAMPLE_RATE 44100

float freq = 220.f;

uint16_t sineTable[257];

volatile uint32_t phaseAccumulator = 0;
volatile uint32_t phaseIncrement = 0;

void setFrequency(float freq) { phaseIncrement = (uint32_t)(((uint64_t)freq << 32) / SAMPLE_RATE); }

void setup() {

  for (int i = 0; i < 256; i++) {
    float angle = (2.0 * M_PI * i) / 256.0;
    sineTable[i] = (uint16_t)((sin(angle) + 1.0) * 511.5);
  }

  sineTable[256] = sineTable[0];

  PORTD.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
  //VREF.DAC0REF = VREF_REFSEL_VDD_gc | VREF_ALWAYSON_bm;
  VREF.DAC0REF = VREF_REFSEL_2V048_gc | VREF_ALWAYSON_bm;
  DAC0.CTRLA = DAC_ENABLE_bm | DAC_OUTEN_bm | DAC_RUNSTDBY_bm;

  setFrequency(freq);

  TCB0.CTRLA = 0; 
  TCB0.CTRLB = TCB_CNTMODE_INT_gc; 
  TCB0.CCMP = (F_CPU / SAMPLE_RATE) - 1; 
  TCB0.INTCTRL = TCB_CAPT_bm; 
  TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_CLKDIV1_gc;

}

void loop() {

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