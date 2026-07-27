// Granular phase-distortion glitch //

#define SAMPLE_RATE   22050
#define BUFFER_SIZE   4096
#define DELAY_BUFFER  1024
#define BPM           120

uint16_t audioBuffer[BUFFER_SIZE];
uint16_t delayBuffer[DELAY_BUFFER];

volatile uint32_t phaseAccumulator = 0;
volatile uint32_t phaseStep = 0;
volatile uint16_t delayIndex = 0;

volatile uint8_t feedbackGain = 90;
volatile uint8_t glitchChaos = 0;
volatile uint16_t chaosModulation = 0;

const float scale[] = {
  55.00, 58.27, 69.30, 73.42, 82.41, 87.31, 98.00,
  110.00, 116.54, 138.59, 146.83, 164.81, 174.61, 196.00,
  220.00, 233.08, 277.18, 293.66, 329.63, 349.23, 392.00
};
const uint8_t SCALE_SIZE = sizeof(scale) / sizeof(scale[0]);

const uint8_t melodyPattern[16] = {0, 7, 1, 8, 2, 7, 3, 14, 0, 7, 2, 1, 15, 8, 12, 7};
uint8_t stepCounter = 0;

void setup() {

  randomSeed(analogRead(A0));

  PORTD.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
  //VREF.DAC0REF = VREF_REFSEL_VDD_gc | VREF_ALWAYSON_bm;
  VREF.DAC0REF = VREF_REFSEL_2V048_gc | VREF_ALWAYSON_bm;
  DAC0.CTRLA = DAC_ENABLE_bm | DAC_OUTEN_bm | DAC_RUNSTDBY_bm;

  for (int i = 0; i < BUFFER_SIZE; i++) {
    float t = (float)i / BUFFER_SIZE * 2.0f * PI;
    float fm = sinf(t * 3.0f + sinf(t * 7.0f) * 1.5f);
    float noise = ((random(0, 1000) / 500.0f) - 1.0f) * 0.05f;
    float signal = tanhf(0.8f * (fm + noise));
    audioBuffer[i] = (uint16_t)(512.0f + 511.0f * signal);
  }

  TCB0.CTRLA = 0;
  TCB0.CTRLB = TCB_CNTMODE_INT_gc;
  TCB0.CCMP = (F_CPU / SAMPLE_RATE) - 1;
  TCB0.INTCTRL = TCB_CAPT_bm;
  TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_CLKDIV1_gc;

}

void loop() {

  uint8_t currentNoteIndex = melodyPattern[random(0, 16)];
  float baseFreq = scale[currentNoteIndex];

  float stepCalc = (baseFreq * BUFFER_SIZE) / SAMPLE_RATE;
  phaseStep = (uint32_t)(stepCalc * 256.0f);

  if (stepCounter % 4 == 3 || random(0, 100) > 92) {
    glitchChaos = random(2, 16);
    feedbackGain = random(75, 95);
  } else {
    glitchChaos = 0;
    feedbackGain = 85;
  }

  chaosModulation = random(0, 64) * glitchChaos;

  stepCounter++;
  if (stepCounter >= 16) { stepCounter = 0; }

  float tempo = 60000.0 / BPM;
  delay((int)(tempo / 2));

}

ISR(TCB0_INT_vect) {

  phaseAccumulator += (phaseStep + chaosModulation);
  uint32_t currentPhase = phaseAccumulator;

  uint16_t delayFeedbackSample = delayBuffer[delayIndex];
  currentPhase += ((uint32_t)delayFeedbackSample << 4) * glitchChaos;

  uint16_t idx1 = (currentPhase >> 8) & (BUFFER_SIZE - 1);
  uint16_t idx2 = (idx1 + 1) & (BUFFER_SIZE - 1);
  uint8_t fraction = currentPhase & 0xFF;

  int32_t s1 = audioBuffer[idx1];
  int32_t s2 = audioBuffer[idx2];
  int32_t interpolatedSample = s1 + (((s2 - s1) * fraction) >> 8);

  uint32_t mixedSample = ((uint32_t)interpolatedSample * (100 - feedbackGain)) + ((uint32_t)delayFeedbackSample * feedbackGain);
  mixedSample /= 100;

  delayBuffer[delayIndex] = (uint16_t)(mixedSample) ^ (glitchChaos >> 1);

  delayIndex++;
  if (delayIndex >= DELAY_BUFFER) { delayIndex = 0; }

  uint16_t outputSample = (uint16_t)mixedSample;
  if (outputSample > 1023) outputSample = 1023;

  DAC0.DATAL = (outputSample & 0x03) << 6;
  DAC0.DATAH = outputSample >> 2;

  TCB0.INTFLAGS = TCB_CAPT_bm;

}