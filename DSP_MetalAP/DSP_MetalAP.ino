// All-pass metalloid engine //

#define SAMPLE_RATE 22050
#define MAX_DELAY   192
#define MIN_DELAY   24
#define BPM         135

static int16_t delay_line[MAX_DELAY ] = {0};
volatile int current_size = 64; 
volatile int delay_ptr = 0;

static int16_t ap_state = 0; 
volatile int16_t g_ap = 128; 
volatile int16_t decay = 240;

void pluck(float freq) {

    int new_size = (int)((float)SAMPLE_RATE / freq + 0.5f);

    if (new_size < MIN_DELAY) new_size = MIN_DELAY;
    if (new_size > MAX_DELAY) new_size = MAX_DELAY;

    cli();
    current_size = new_size;
    delay_ptr = 0;
    ap_state = 0;
    sei();

    for (int i = 0; i < new_size; i++) {
        delay_line[i] = random(-16384, 16384);
    }

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

    g_ap = random(1, 255);
    float random_freq = random(220, 880); 
    pluck(random_freq);

    float tempo = 60000.0 / BPM;
    delay((int)(tempo / 2));

}

ISR(TCB0_INT_vect) {

    int16_t x = delay_line[delay_ptr];

    int32_t ap_out = (-(int32_t)g_ap * x) + ((int32_t)ap_state << 8);
    ap_out >>= 8;
    
    ap_state = x + (((int32_t)g_ap * ap_out) >> 8);

    int16_t feedback_signal = ((int32_t)ap_out * decay) >> 8;

    delay_line[delay_ptr] = feedback_signal;
    
    delay_ptr++;

    if (delay_ptr >= current_size) {
        delay_ptr = 0;
    }

    int16_t sample = (ap_out >> 5) + 512; 

    if (sample < 0)    sample = 0;
    if (sample > 1023) sample = 1023;

    DAC0.DATAL = (sample & 0x03) << 6;
    DAC0.DATAH = sample >> 2;

    TCB0.INTFLAGS = TCB_CAPT_bm;

}