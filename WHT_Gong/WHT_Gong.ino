// Inverted WHT (Walsh-Hadamard) spectral gong engine //

#define SAMPLE_RATE 24000

#define LOG2_N 7
#define N (1 << LOG2_N)

unsigned long last_decay = 0;

static int16_t audio_buffer_0[N];
static int16_t audio_buffer_1[N];

volatile uint8_t active_buffer = 0;
volatile int sample_index = 0;
volatile bool buffer_needs_calc = true;
volatile int16_t bin_amplitudes[N] = {0}; 


void ifwht_agnostic(int16_t* data) {

    for (int step = 1; step < N; step <<= 1) {

        int jump = step << 1;
        for (int pair = 0; pair < N; pair += jump) {

            for (int i = 0; i < step; i++) {
                int idxA = pair + i;
                int idxB = idxA + step;

                int32_t a = data[idxA];
                int32_t b = data[idxB];

                data[idxA] = (int16_t)(a + b);
                data[idxB] = (int16_t)(a - b);
            }

        }

    }

}

void calculate_next(int16_t* real_q15) {

    for (int i = 0; i < N; i++) {
        if (abs(bin_amplitudes[i]) > 5) {
            real_q15[i] = bin_amplitudes[i];
        } else {
            real_q15[i] = 0;
        }
    }

    ifwht_agnostic(real_q15);
    
    for (int i = 0; i < N; i++) {

        int32_t amp = (int32_t)real_q15[i]; 
        if (amp > 32767)  amp = 32767;
        if (amp < -32768) amp = -32768;
        real_q15[i] = (int16_t)amp;

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

    if (random(2000) < 1) { 

        int density = random(20, 80);

        for (int i = 1; i < N; i++) {

            if (random(100) < density) {
                int16_t amp = random(32, 1024) / i;
                bin_amplitudes[i] = (random(100) > 50) ? amp : -amp;
            }

        }

    }

    if (millis() - last_decay >= 10) { 

        last_decay = millis();

        for (int i = 0; i < N; i++) {

            if (bin_amplitudes[i] != 0) {
                bin_amplitudes[i] = (int32_t)bin_amplitudes[i] * 98 / 100;
                if (abs(bin_amplitudes[i]) < 5) bin_amplitudes[i] = 0;
            }

        }

    }

    if (buffer_needs_calc) {

        if (active_buffer == 0) { calculate_next(audio_buffer_1); }
        else { calculate_next(audio_buffer_0); }
        buffer_needs_calc = false;

    }

}

ISR(TCB0_INT_vect) {

    int16_t val;

    if (active_buffer == 0) { val = audio_buffer_0[sample_index]; }
    else { val = audio_buffer_1[sample_index]; }

    int32_t sample = (val >> 2) + 512; 

    if (sample < 0) sample = 0;
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