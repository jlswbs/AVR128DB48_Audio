// IFFT spectral gong engine //

#define SAMPLE_RATE 44100
#define BPM         60
#define PPQN        4
#define TRIG_MS     (60000UL / (BPM * PPQN))

#define LOG2_N 6
#define N (1 << LOG2_N)

static int16_t sin_table_q15[257];
unsigned long last_trig = 0;
unsigned long last_decay = 0;

void get_twiddle(uint16_t idx, int16_t* c, int16_t* s) {

    uint16_t norm_idx = (idx * 1024) >> LOG2_N;
    norm_idx &= 1023;

    uint8_t quadrant = norm_idx >> 8;
    uint16_t table_pos = norm_idx & 255;

    if (quadrant == 0)      { *s = sin_table_q15[table_pos]; }
    else if (quadrant == 1) { *s = sin_table_q15[256 - table_pos]; }
    else if (quadrant == 2) { *s = -sin_table_q15[table_pos]; }
    else                    { *s = -sin_table_q15[256 - table_pos]; }

    uint16_t cos_idx = (norm_idx + 256) & 1023;
    quadrant = cos_idx >> 8;
    table_pos = cos_idx & 255;

    if (quadrant == 0)      { *c = sin_table_q15[table_pos]; }
    else if (quadrant == 1) { *c = sin_table_q15[256 - table_pos]; }
    else if (quadrant == 2) { *c = -sin_table_q15[table_pos]; }
    else                    { *c = -sin_table_q15[256 - table_pos]; }

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
static int16_t imag_q15[N];

volatile uint8_t active_buffer = 0;
volatile int sample_index = 0;
volatile bool buffer_needs_calc = true;

volatile int16_t bin_amplitudes[N / 2] = {0};
uint16_t phase_accumulator[N / 2] = {0};

void ifft_agnostic(int16_t* re, int16_t* im) {

    for (int i = 0; i < N; i++) {
        int j = reverse_bits(i, LOG2_N);
        if (i < j) {
            int16_t tmp = re[i];
            re[i] = re[j];
            re[j] = tmp;

            tmp = im[i];
            im[i] = im[j];
            im[j] = tmp;
        }
    }

    for (int step = 1; step < N; step <<= 1) {
        int jump = step << 1;
        int delta = N / jump;

        for (int group = 0; group < step; group++) {
            int16_t w_re, w_im;
            get_twiddle(group * delta, &w_re, &w_im);

            for (int pair = group; pair < N; pair += jump) {
                int match = pair + step;

                int32_t tr = ((int32_t)re[match] * w_re - (int32_t)im[match] * w_im) >> 15;
                int32_t ti = ((int32_t)re[match] * w_im + (int32_t)im[match] * w_re) >> 15;

                re[match] = (re[pair] - tr) >> 1;
                im[match] = (im[pair] - ti) >> 1;
                re[pair] = (re[pair] + tr) >> 1;
                im[pair] = (im[pair] + ti) >> 1;
            }
        }
    }

}

void calculate_next(int16_t* real_q15) {

    for (int i = 0; i < N; i++) {
        real_q15[i] = 0;
        imag_q15[i] = 0;
    }

    for (int bin = 1; bin < N / 2; bin++) {

        int16_t amp = bin_amplitudes[bin];

        if (amp > 10) {

            phase_accumulator[bin] += bin;

            if (phase_accumulator[bin] >= N)
                phase_accumulator[bin] -= N;

            int16_t cos_p, sin_p;
            get_twiddle(phase_accumulator[bin], &cos_p, &sin_p);

            real_q15[bin] = ((int32_t)amp * cos_p) >> 15;
            imag_q15[bin] = ((int32_t)amp * sin_p) >> 15;

            real_q15[N - bin] = real_q15[bin];
            imag_q15[N - bin] = -imag_q15[bin];
            
        }
    }

    ifft_agnostic(real_q15, imag_q15);

}

void setup() {

    randomSeed(analogRead(0));

    for (int i = 0; i < 256; i++) {
        float angle = (2.0f * M_PI * (float)i) / 1024.0f;
        sin_table_q15[i] = (int16_t)(sinf(angle) * 32767.0f);
    }

    sin_table_q15[256] = 32767;

    PORTD.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
    //VREF.DAC0REF = VREF_REFSEL_VDD_gc | VREF_ALWAYSON_bm;
    VREF.DAC0REF = VREF_REFSEL_2V048_gc | VREF_ALWAYSON_bm;
    DAC0.CTRLA = DAC_ENABLE_bm | DAC_OUTEN_bm | DAC_RUNSTDBY_bm;

    TCB0.CTRLA = 0;
    TCB0.CTRLB = TCB_CNTMODE_INT_gc;
    TCB0.CCMP = (F_CPU / SAMPLE_RATE) - 1;
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_CLKDIV1_gc;

}

void loop() {

    unsigned long now = millis();

    if (now - last_trig >= TRIG_MS) {
        last_trig = now;
        int density = random(20, 80);
        for (int i = 1; i < N / 2; i++) {
            if (random(100) < density) {
                bin_amplitudes[i] = random(6000, 10000) / i;
            }
        }
    }

    if (now - last_decay >= 10) {
        last_decay = now;

        for (int bin = 0; bin < N / 2; bin++) {
            int16_t current_amp = bin_amplitudes[bin];

            if (current_amp > 0) {
                int32_t next_amp = ((int32_t)current_amp * 1950) >> 11;
                bin_amplitudes[bin] = (next_amp < 10) ? 0 : (int16_t)next_amp;
            }
        }
    }

    if (buffer_needs_calc) {
        if (active_buffer == 0) {
            calculate_next(audio_buffer_1);
        } else {
            calculate_next(audio_buffer_0);
        }

        buffer_needs_calc = false;
    }

}

ISR(TCB0_INT_vect) {

    int16_t val;

    if (active_buffer == 0) {
        val = audio_buffer_0[sample_index];
    } else {
        val = audio_buffer_1[sample_index];
    }

    int32_t sample = val + 512;

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