// Particle grain cluster //

#define SAMPLE_RATE   44100 
#define BUFFER_SIZE   4096       
#define DELAY_BUFFER  1024        
#define WINDOW_EDGE   4 
 
uint16_t audioBuffer[BUFFER_SIZE]; 
uint16_t delayBuffer[DELAY_BUFFER]; 
 
volatile uint16_t grainPosition = 0; 
volatile uint16_t grainLength = 8 << 8; 
volatile uint16_t grainSpeed = 1 << 8; 
volatile uint16_t delayIndex = 0; 
volatile uint8_t feedbackGain = 92; 
 
volatile uint16_t nextGrainResetPosition = 0; 
volatile uint16_t currentDelayMax = DELAY_BUFFER; 
float wowPhase = 0.0f; 
float flutterPhase = 0.0f; 
 
uint8_t bbShift1, bbShift2, bbShift3; 
uint8_t bbMask1, bbMask2;
uint8_t bbMul1, bbMul2;
 
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
 
  bbShift1 = random(2, 6); 
  bbShift2 = random(4, 10); 
  bbShift3 = random(3, 6); 
  bbMask1  = random(1, 16); 
  bbMask2  = random(1, 16);
  bbMul1   = random(3, 13) | 1;
  bbMul2   = random(1, 8); 
 
  for (int i = 0; i < BUFFER_SIZE; i++) {

    uint8_t bytebeat = ((i >> bbShift1) | (i >> bbShift2)) * (i & ((i >> bbShift3) ? bbMask1 : bbMask2));
    bytebeat = (bytebeat * bbMul1) ^ (i >> bbMul2);
    float finalSignal = ((float)bytebeat / 128.0f - 1.0f);
    audioBuffer[i] = (uint16_t)(512.0f + 511.0f * tanhf(8.0f * finalSignal));

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

  uint16_t currentLength = random(8, 224);  
  grainLength = currentLength << 8; 
 
  uint16_t maxStart = BUFFER_SIZE - currentLength; 
  if (maxStart > 0) { 
    nextGrainResetPosition = random(0, maxStart) << 8; 
  } else { 
    nextGrainResetPosition = 0; 
  } 
 
  uint16_t baseSpeed = random(32, 2048); 
 
  wowPhase += 0.2f;  
  if (wowPhase > 2 * PI) wowPhase -= 2 * PI; 
  float wowMod = sinf(wowPhase) * 40.0f;  
 
  flutterPhase += 0.4f; 
  if (flutterPhase > 2 * PI) flutterPhase -= 2 * PI; 
  float flutterMod = sinf(flutterPhase) * 4.0f;  
 
  grainSpeed = (uint16_t)((float)baseSpeed + wowMod) << 5;  
  currentDelayMax = (DELAY_BUFFER - 5) + (int16_t)flutterMod; 
 
  delay(random(1, 18));
 
} 
 
ISR(TCB0_INT_vect) { 

  grainPosition += grainSpeed; 
 
  if (grainPosition >= grainLength) { 
    grainPosition = nextGrainResetPosition;  
  } 
 
  uint16_t index1 = (grainPosition >> 8) & (BUFFER_SIZE - 1); 
  uint16_t index2 = (index1 + 1) & (BUFFER_SIZE - 1);  
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
  uint16_t mutateIndex = (index1 + (DELAY_BUFFER >> 1)) & (BUFFER_SIZE - 1); 
  audioBuffer[mutateIndex] = ((uint32_t)audioBuffer[mutateIndex] * 3 + mixedSample) >> 2; 
   
  delayIndex++; 
  if (delayIndex >= currentDelayMax || delayIndex >= DELAY_BUFFER) delayIndex = 0; 
 
  uint16_t outputSample = (uint16_t)mixedSample; 
  if (outputSample > 1023) outputSample = 1023; 
 
  DAC0.DATAL = (outputSample & 0x03) << 6; 
  DAC0.DATAH = outputSample >> 2; 
 
  TCB0.INTFLAGS = TCB_CAPT_bm;

}