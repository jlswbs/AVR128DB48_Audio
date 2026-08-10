// Inverse DCT spectral gong engine //

#define SAMPLE_RATE 44100

#define LOG2_N 7
#define N (1 << LOG2_N)

static int16_t sin_table_q15[257];
unsigned long last_decay = 0;

int16_t get_cos_q15(uint16_t idx) {

    uint16_t norm_idx = (idx * 1024) >> LOG2_N;
    norm_idx = (norm_idx + 256) & 1023;

    uint8_t quadrant = norm_idx >> 8;
    uint16_t table_pos = norm_idx & 255;

    if (quadrant == 0)      { return sin_table_q15[table_pos]; }
    else if (quadrant == 1) { return sin_table_q15[256 - table_pos]; }
    else if (quadrant == 2) { return -sin_table_q15[table_pos]; }
    else                    { return -sin_table_q15[256 - table_pos]; }

}

inline uint16_t reverse_bits(uint16_t x, uint8_t bits) {

    uint16_t rev = 0;
    for (uint8_t i = 0; i < bits; i++) {
        if (x & (1 << i)) {
            rev |= (1 << ((bits - 1) - i));
        }
    }
    return rev;

}

static int16_t audio_buffer_0[N];
static int16_t audio_buffer_1[N];

volatile uint8_t active_buffer = 0;
volatile int sample_index = 0;
volatile bool buffer_needs_calc = true;

volatile int16_t bin_amplitudes[N] = {0};
uint32_t phase_accumulator[N] = {0};

void idct_fast(int16_t* data) {

    for (int i = 0; i < N; i++) {
        int j = reverse_bits(i, LOG2_N);
        if (i < j) {
            int16_t tmp = data[i];
            data[i] = data[j];
            data[j] = tmp;
        }
    }

    for (int step = 1; step < N; step <<= 1) {
        int jump = step << 1;
        int delta = N / jump;

        for (int group = 0; group < step; group++) {
            int16_t w_re = get_cos_q15(group * delta);

            for (int pair = group; pair < N; pair += jump) {
                int match = pair + step;

                int32_t tr = ((int32_t)data[match] * w_re) >> 15;

                data[match] = data[pair] - tr;
                data[pair] = data[pair] + tr;
            }
        }
    }

}

void calculate_next(int16_t* real_q15) {

    for (int i = 0; i < N; i++) {
        real_q15[i] = 0;
    }

    for (int bin = 0; bin < N; bin++) {
        int16_t amp = bin_amplitudes[bin];

        if (amp > 5) {
            phase_accumulator[bin] += (uint32_t)bin << (LOG2_N - 1);
            phase_accumulator[bin] &= 1023;

            int16_t cos_p = get_cos_q15(phase_accumulator[bin]);

            real_q15[bin] = ((int32_t)amp * cos_p) >> 15;
        }
    }

    idct_fast(real_q15);

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

    for (int i = 0; i < 256; i++) {
        float angle = (2.0f * M_PI * (float)i) / 1024.0f;
        sin_table_q15[i] = (int16_t)(sinf(angle) * 32767.0f);
    }

    sin_table_q15[256] = 32767;

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

    if (random(1000) < 1) {
        int density = random(20, 80);
        for (int i = 1; i < N; i++) {
            if (random(100) < density) {
                bin_amplitudes[i] = random(10, 650) / i;
            }
        }
    }

    if (millis() - last_decay >= 10) {
        last_decay = millis();

        for (int i = 0; i < N; i++) {
            if (bin_amplitudes[i] > 0) {
                bin_amplitudes[i] = (bin_amplitudes[i] * (N - i)) / N;
                if (bin_amplitudes[i] < 5) bin_amplitudes[i] = 0;
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