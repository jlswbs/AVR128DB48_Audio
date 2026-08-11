// Atari Pokey like generator //

#define SAMPLE_RATE 44100
#define POKEY_CHANNELS 4

#define MODE_TONE  0
#define MODE_NOISE 1

typedef struct {
  volatile uint16_t divider;
  uint16_t counter;
  uint8_t  output;
  volatile uint8_t  volume;
  uint8_t  mode;
  uint16_t lfsr;
} pokey_chan_t;

pokey_chan_t ch[POKEY_CHANNELS]; 

const uint16_t scale[] = {
  120, 135, 150, 160, 180, 200, 225, 240,
  270, 300, 320, 360
};
#define SCALE_LEN (sizeof(scale)/sizeof(scale[0]))

uint32_t lastTick = 0;
uint32_t lastDrum = 0;
uint32_t lastHat = 0;
uint32_t lastDecay = 0;

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

  ch[0] = (pokey_chan_t){ 200, 0, 0, 15, MODE_TONE,  0x1FF };
  ch[1] = (pokey_chan_t){ 300, 0, 0, 12, MODE_TONE,  0x1FF };
  ch[2] = (pokey_chan_t){  40, 0, 0,  0, MODE_NOISE, 0x1A5 };
  ch[3] = (pokey_chan_t){ 120, 0, 0,  0, MODE_NOISE, 0x0E7 };

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
  
  uint32_t now = millis();

  if (now - lastTick > 120) {
    lastTick = now;
    ch[0].divider = scale[random(SCALE_LEN)] / 3;
    ch[0].volume  = random(7, 13);
    ch[1].divider = scale[random(0, SCALE_LEN / 2)] * 2;
    ch[1].volume  = random(5, 12);
  }

  if (now - lastHat > 240) {
    lastHat = now; 
    ch[2].divider = random(10, 50);
    ch[2].volume  = 10;
  }

  if (now - lastDrum > 360) {
    lastDrum = now;
    ch[3].divider = random(80, 180);
    ch[3].volume  = 15;
  }

  if (now - lastDecay > 12) {
    lastDecay = now;
    if (ch[2].volume > 0) ch[2].volume--;
    if (ch[3].volume > 0) ch[3].volume--;
  }

}

ISR(TCB0_INT_vect) {

  int16_t sample_sum = 0;

  for (uint8_t i = 0; i < POKEY_CHANNELS; i++) {
    ch[i].counter++;
    if (ch[i].counter >= ch[i].divider) {
      ch[i].counter = 0;

      if (ch[i].mode == MODE_TONE) {
        ch[i].output ^= 1;
      } else {
        uint16_t bit = ((ch[i].lfsr >> 0) ^ (ch[i].lfsr >> 4)) & 1;
        ch[i].lfsr = (ch[i].lfsr >> 1) | (bit << 8);
        ch[i].output = ch[i].lfsr & 1;
      }
    }
    sample_sum += ch[i].output ? ch[i].volume : -ch[i].volume;
  }

  int16_t val = 512 + (sample_sum << 3);

  if (val > 1023) val = 1023;
  if (val < 0)    val = 0;

  DAC0.DATAL = (val & 0x03) << 6;
  DAC0.DATAH = val >> 2;

  TCB0.INTFLAGS = TCB_CAPT_bm;

}