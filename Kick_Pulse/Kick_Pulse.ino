// Karplus-Strong kick pulse drum //

#define SAMPLE_RATE 22050
#define BPM         120

#define BUFFER_SIZE 600
int16_t delayBuffer[BUFFER_SIZE];

volatile int16_t index = 0;
volatile int16_t currentDelayLength = 0;
volatile uint16_t feedbackGain = 0; 
volatile int16_t lastSample = 0;
volatile int8_t pulse = 0;

void triggerKick(float frequency) {

  int16_t delayLength = (int16_t)(SAMPLE_RATE / frequency);
  if (delayLength > BUFFER_SIZE) delayLength = BUFFER_SIZE;
  if (delayLength < 10) delayLength = 10;

  currentDelayLength = delayLength;
  index = 0;

  int16_t pulseWidth = delayLength / pulse;
  if (pulseWidth < 4) pulseWidth = 4;

  for (int i = 0; i < delayLength; i++) {
    if (i < pulseWidth) {
      delayBuffer[i] = 511 - (i * (511 / pulseWidth));
    } else {
      delayBuffer[i] = 0;
    }
  }
  
  for (int i = delayLength; i < BUFFER_SIZE; i++) delayBuffer[i] = 0;
  lastSample = 0;

}


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

  float randomPitchFreq = random(40, 90);
  feedbackGain = 20000 + random(0, 8500);
  pulse = random(0, 12);

  triggerKick(randomPitchFreq);

  float tempo = 60000.0 / BPM;
  delay((int)(tempo / 2));

}

ISR(TCB0_INT_vect) {

  int16_t currentSample = delayBuffer[index];
  int16_t filteredSample = (currentSample + lastSample) >> 1;

  lastSample = currentSample;

  int16_t feedbackSample = (int16_t)((-(int32_t)filteredSample * feedbackGain) >> 15);
  delayBuffer[index] = feedbackSample;

  uint16_t dacValue = 512 + filteredSample;
  if (dacValue > 1023) dacValue = 1023;
  
  DAC0.DATAL = (dacValue & 0x03) << 6;
  DAC0.DATAH = dacValue >> 2;

  index++;

  if (index >= currentDelayLength) index = 0;

  TCB0.INTFLAGS = TCB_CAPT_bm;

}