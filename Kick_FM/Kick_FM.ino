// Kick drum FM //

#define SAMPLE_RATE 22050
#define SIZE        256 
#define BPM         120

uint16_t table[SIZE];

volatile uint32_t carrierPhase = 0;
volatile uint32_t modulatorPhase = 0;
volatile uint32_t carrierPhaseInc = 0;
volatile uint32_t modulatorPhaseInc = 0;
volatile uint16_t envCarrier = 65535;
volatile uint16_t envModulator = 65535;
volatile uint16_t envDecayCarrier = 20;
volatile uint16_t envDecayModulator = 40;
volatile uint32_t modDepth = 2000;  

uint32_t frequencyToIncrement(float frequency) {
  return (uint32_t)(frequency * SIZE * (1UL << 16) / SAMPLE_RATE);
}

void triggerKick() {

  carrierPhase = 0;
  modulatorPhase = 0;
  envCarrier = 65535;
  envModulator = 65535;

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

  for (int i = 0; i < SIZE; i++) {
    float t = (float)i / SIZE * 2 * PI;
    table[i] = (uint16_t)(512.0f + 511.0f * sinf(t));
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

  uint16_t freq = random(40, 90);
  float carrierFreq = freq;
  float modulatorFreq = 5 * freq;
  
  carrierPhaseInc = frequencyToIncrement(carrierFreq);
  modulatorPhaseInc = frequencyToIncrement(modulatorFreq);

  uint16_t decay = random(4, 28);

  envDecayCarrier = decay;
  envDecayModulator = 2 * decay;

  modDepth = random(1000, 35000); 

  triggerKick();

  float tempo = 60000.0 / BPM;
  delay((int)(tempo / 2));

}

ISR(TCB0_INT_vect) {

  if (envCarrier > envDecayCarrier) envCarrier -= envDecayCarrier;
  else envCarrier = 0;
  if (envModulator > envDecayModulator) envModulator -= envDecayModulator;
  else envModulator = 0;

  uint8_t modIndex = modulatorPhase >> 16;
  int16_t modSample = (int16_t)table[modIndex] - 512;
  int32_t phaseDeviation = ((int32_t)modSample * (int32_t)envModulator) >> 15;
  phaseDeviation = (phaseDeviation * (int32_t)modDepth);
  
  uint32_t modulatedCarrierPhase = carrierPhase + phaseDeviation;
  uint8_t carIndex = modulatedCarrierPhase >> 16;
  int16_t sample = (int16_t)table[carIndex] - 512;
  sample = ((int32_t)sample * (envCarrier >> 8)) >> 8;
  
  uint16_t dacValue = sample + 512;
  DAC0.DATAL = (dacValue & 0x03) << 6;
  DAC0.DATAH = dacValue >> 2;

  carrierPhase += carrierPhaseInc;
  modulatorPhase += modulatorPhaseInc;

  TCB0.INTFLAGS = TCB_CAPT_bm;

}