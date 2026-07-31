// Kick drum with wavefolder //

#define SAMPLE_RATE 22050
#define SIZE        256 
#define BPM         120

uint16_t table[SIZE];

volatile uint32_t phaseAccumulator = 0;
volatile uint32_t phaseIncrement = 0;

volatile uint32_t currentPhaseInc = 0;
volatile uint16_t ampDecayCoeff = 32748;
volatile uint8_t folderParam = 1;
volatile uint16_t ampEnvelope = 0;

uint32_t frequencyToIncrement(float frequency) {

  return (uint32_t)(((uint64_t)frequency << 32) / SAMPLE_RATE);

}

void triggerKick() {

  phaseAccumulator = 0;
  ampEnvelope = 32768;

}

void setup() {

  randomSeed(analogRead(A0));

  PORTD.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
  //VREF.DAC0REF = VREF_REFSEL_VDD_gc | VREF_ALWAYSON_bm;
  VREF.DAC0REF = VREF_REFSEL_2V048_gc | VREF_ALWAYSON_bm;
  DAC0.CTRLA = DAC_ENABLE_bm | DAC_OUTEN_bm | DAC_RUNSTDBY_bm;

  for (int i = 0; i < SIZE; i++) {
    float t = (float)i / SIZE * 2 * PI;
    float noise = ((random(0, 1000) / 500.0f) - 1.0f) * 0.2f;
    table[i] = (uint16_t)(512.0f + 511.0f * tanhf((sinf(t) * (1.0f + noise))));
  }

  TCB0.CTRLA = 0; 
  TCB0.CTRLB = TCB_CNTMODE_INT_gc; 
  TCB0.CCMP = (F_CPU / SAMPLE_RATE) - 1; 
  TCB0.INTCTRL = TCB_CAPT_bm; 
  TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_CLKDIV1_gc;

}

void loop() {

  float randomPitchFreq = 40.0f + (rand() % 50);
  currentPhaseInc = frequencyToIncrement(randomPitchFreq);
  ampDecayCoeff = 32730 + (rand() % 28);
  folderParam = 1 + (rand() % 10);

  triggerKick();
  
  float tempo = 60000.0 / BPM;
  delay((int)(tempo / 2));

}

ISR(TCB0_INT_vect) {

  phaseAccumulator += currentPhaseInc;
  uint8_t tableIndex = phaseAccumulator >> 24;

  int32_t rawSample = (int16_t)table[tableIndex] - 512;
  int32_t foldedSample = rawSample * folderParam;

  foldedSample += 2048; 
  
  int32_t rem = foldedSample & 2047;
  if (rem > 1023) {
    rem = 2047 - rem;
  }

  foldedSample = rem - 512;

  int16_t finalSample = (int16_t)((foldedSample * ampEnvelope) >> 15);
  uint16_t dacValue = finalSample + 512;

  DAC0.DATAL = (dacValue & 0x03) << 6;
  DAC0.DATAH = dacValue >> 2;

  ampEnvelope = (uint16_t)(((uint32_t)ampEnvelope * ampDecayCoeff) >> 15);

  TCB0.INTFLAGS = TCB_CAPT_bm;

}