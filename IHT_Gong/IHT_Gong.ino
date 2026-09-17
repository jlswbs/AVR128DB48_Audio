// Inverted Haar spectral gong engine //

#define SAMPLE_RATE 44100

#define LOG2_N 7
#define N (1 << LOG2_N)

unsigned long last_decay = 0;

static int16_t audio_buffer_0[N];
static int16_t audio_buffer_1[N];

volatile uint8_t active_buffer = 0;
volatile int sample_index = 0;
volatile bool buffer_needs_calc = true;
volatile int16_t bin_amplitudes[N] = {0}; 


void inverse_haar(int16_t* data) {

    int16_t temp[N];

    for (int len = 2; len <= N; len <<= 1) {

        int half = len >> 1;

        for (int i = 0; i < half; i++) {
            int16_t a = data[i];
            int16_t d = data[half + i];

            temp[2 * i]     = a + d;
            temp[2 * i + 1] = a - d;
        }

        for (int i = 0; i < len; i++) {
            data[i] = temp[i];
        }

    }

}

void calculate_next(int16_t* out_buffer) {

    for (int i = 0; i < N; i++) {

        if (abs(bin_amplitudes[i]) > 5) {
            out_buffer[i] = bin_amplitudes[i];
        } else {
            out_buffer[i] = 0;
        }

    }

    inverse_haar(out_buffer);

    for (int i = 0; i < N; i++) {

        int16_t amp = out_buffer[i];

        if (amp >  32767) amp =  32767;
        if (amp < -32768) amp = -32768;

        out_buffer[i] = amp;

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

    if (random(2500) < 1) { 

        int density = random(50, 90); 

        for (int i = 1; i < N; i++) {

            if (random(100) < density) {

                int16_t amp = random(16, 512) / (1 + (i / 16));
                bin_amplitudes[i] = (random(100) > 50) ? amp : -amp;

            }
        }

    }

    if (millis() - last_decay >= 10) {

        last_decay = millis();

        for (int i = 0; i < N; i++) {

            if (bin_amplitudes[i] != 0) {

                int decay_factor = 99 - (i >> 3); 
                if (decay_factor < 85) decay_factor = 85;
                bin_amplitudes[i] = (int32_t)bin_amplitudes[i] * decay_factor / 100;
                if (abs(bin_amplitudes[i]) < 5) bin_amplitudes[i] = 0;
            }

        }

    }

    if (buffer_needs_calc) {
        if (active_buffer == 0) { calculate_next(audio_buffer_1); }
        else                    { calculate_next(audio_buffer_0); }
        buffer_needs_calc = false;
    }

}

ISR(TCB0_INT_vect) {

    int16_t val;

    if (active_buffer == 0) { val = audio_buffer_0[sample_index]; }
    else                    { val = audio_buffer_1[sample_index]; }

    int16_t sample = (val >> 2) + 512; 

    if (sample < 0)    sample = 0;
    if (sample > 1023) sample = 1023;

    DAC0.DATAL = (sample & 0x03) << 6;
    DAC0.DATAH = sample >> 2;

    sample_index++;

    if (sample_index >= N) {
        sample_index = 0;
        if (!buffer_needs_calc) {
            active_buffer = !active_buffer;
            buffer_needs_calc = true;
        }
    }

    TCB0.INTFLAGS = TCB_CAPT_bm;

}