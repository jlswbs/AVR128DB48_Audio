// Granular glitch cluster //

#define SAMPLE_RATE   44100
#define BUFFER_SIZE   256 
#define DELAY_BUFFER  512
#define WINDOW_EDGE   8
#define BPM           120

uint16_t audioBuffer[BUFFER_SIZE];
uint16_t delayBuffer[DELAY_BUFFER];

volatile uint16_t grainPosition = 0;
volatile uint16_t grainLength = 32 << 8;
volatile uint16_t grainSpeed = 1 << 8;
volatile uint16_t delayIndex = 0;
volatile uint8_t feedbackGain = 95;

volatile uint16_t nextGrainResetPosition = 0;
volatile uint16_t currentDelayMax = DELAY_BUFFER;
float wowPhase = 0.0f;
float flutterPhase = 0.0f;

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

  for (int i = 0; i < BUFFER_SIZE; i++) {
    float t = (float)i / BUFFER_SIZE * 2 * PI;
    float noise = ((random(0, 1000) / 500.0f) - 1.0f) * 0.3f;
    audioBuffer[i] = (uint16_t)(512.0f + 511.0f * tanhf(6.0f * (sinf(t) * (1.0f + noise))));
  }

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

  uint16_t currentLength = random(8, BUFFER_SIZE);
  uint16_t baseSpeed = random(64, 1024);
  grainLength = currentLength << 8;

  uint16_t maxStart = BUFFER_SIZE - currentLength;
  if (maxStart > 0) {
    nextGrainResetPosition = random(0, maxStart) << 8;
  } else {
    nextGrainResetPosition = 0;
  }

  wowPhase += 0.05f;
  if (wowPhase > 2 * PI) wowPhase -= 2 * PI;
  float wowMod = sinf(wowPhase) * 12.0f;

  flutterPhase += 0.25f;
  if (flutterPhase > 2 * PI) flutterPhase -= 2 * PI;
  float flutterMod = sinf(flutterPhase) * 1.2f; 

  grainSpeed = (uint16_t)((float)baseSpeed + wowMod) << 6;
  currentDelayMax = 510 + (int16_t)flutterMod;

  float tempo = 60000.0 / BPM;
  delay((int)(tempo / 4));

}

ISR(TCB0_INT_vect) {

  grainPosition += grainSpeed;

  if (grainPosition >= grainLength) {
    grainPosition = nextGrainResetPosition; 
  }

  uint8_t index1 = grainPosition >> 8;
  uint8_t index2 = (index1 + 1) & (BUFFER_SIZE - 1); 
  uint8_t fraction = grainPosition & 0xFF; 

  uint16_t sample1 = audioBuffer[index1];
  uint16_t sample2 = audioBuffer[index2];

  uint16_t currentSample;
  if (sample2 >= sample1) {
    currentSample = sample1 + (((uint32_t)(sample2 - sample1) * fraction) >> 8);
  } else {
    currentSample = sample1 - (((uint32_t)(sample1 - sample2) * fraction) >> 8);
  }

  uint16_t currentSampleIdx = grainPosition >> 8;
  uint16_t samplesLeft = (grainLength >> 8) - currentSampleIdx;
  
  uint16_t gain = 256;

  if (currentSampleIdx < WINDOW_EDGE) {
    gain = (currentSampleIdx * 256) / WINDOW_EDGE;
  } else if (samplesLeft < WINDOW_EDGE) {
    gain = (samplesLeft * 256) / WINDOW_EDGE;
  }

  int32_t shiftedSample = (int32_t)currentSample - 512;
  shiftedSample = (shiftedSample * gain) >> 8;
  currentSample = (uint16_t)(shiftedSample + 512);

  uint16_t delayedSample = delayBuffer[delayIndex];
  uint32_t mixedSample = ((uint32_t)currentSample * (100 - feedbackGain)) + ((uint32_t)delayedSample * feedbackGain);
  mixedSample /= 100;

  delayBuffer[delayIndex] = (uint16_t)mixedSample;
  
  delayIndex++;
  if (delayIndex >= currentDelayMax || delayIndex >= DELAY_BUFFER) delayIndex = 0;

  uint16_t outputSample = (uint16_t)mixedSample;
  if (outputSample > 1023) outputSample = 1023;

  DAC0.DATAL = (outputSample & 0x03) << 6;
  DAC0.DATAH = outputSample >> 2;

  TCB0.INTFLAGS = TCB_CAPT_bm;

}