// Inverted FFT spectral fragments engine //

#define SAMPLE_RATE 24000

#define LOG2_N 7
#define N (1 << LOG2_N)

static int16_t sin_table_q15[257];

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
uint32_t phase_accumulator[N / 2] = {0};

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

                re[match] = (re[pair] - tr);
                im[match] = (im[pair] - ti);
                re[pair] = (re[pair] + tr);
                im[pair] = (im[pair] + ti);
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

        if (amp > 5) {

            phase_accumulator[bin] += (uint32_t)bin << LOG2_N;
            phase_accumulator[bin] &= 1023;

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

    static uint16_t rhythm_counter = 0;
    static uint8_t rhythm_phase = 0;
    rhythm_counter++;
    
    if (rhythm_counter % 300 < 5) {
        
        if (rhythm_phase % 4 == 0) {
            bin_amplitudes[3] += random(250, 600);
            bin_amplitudes[5] += random(150, 400);
            bin_amplitudes[7] += random(100, 300);
        }
        
        if (rhythm_phase % 4 == 2) {
            bin_amplitudes[12] += random(150, 500);
            bin_amplitudes[18] += random(100, 300);
            bin_amplitudes[25] += random(50, 200);
        }
        
        if (rhythm_counter % 200 < 5) {
            int bin = 8 + (rhythm_counter / 50) % 5;
            bin_amplitudes[bin] += random(50, 150);
            
            if (bin * 2 < N / 2) {
                bin_amplitudes[bin * 2] += random(25, 75);
            }
        }
        
        rhythm_phase++;
    }
    
    if (rhythm_counter % 850 < 20) {
        int base_bin = 6 + (rhythm_counter / 100) % 20;
        
        for (int i = 0; i < 7; i++) {
            int bin = base_bin + i * 3;
            if (bin < N / 2) {
                int amp = (7 - i) * 30 + random(0, 100);
                bin_amplitudes[bin] += amp;
                
                if (bin * 3 < N / 2) {
                    bin_amplitudes[bin * 3] += amp / 4;
                }
            }
        }
    }
    
    if (random(100) < 5) {
        int start_bin = random(10, 50);
        int count = random(3, 8);
        int base_amp = random(50, 250);
        
        for (int i = 0; i < count; i++) {
            int bin = start_bin + i * 2;
            if (bin < N / 2) {
                int amp = base_amp - i * 15;
                if (amp > 0) {
                    bin_amplitudes[bin] += amp;
                    
                    if (bin * 3 / 2 < N / 2) {
                        bin_amplitudes[bin * 3 / 2] += amp / 3;
                    }
                }
            }
        }
    }
    
    for (int i = 0; i < N / 2; i++) {
        if (bin_amplitudes[i] > 0) {
            int decay;
            if (i < 8) decay = 99;
            else if (i < 20) decay = 96;
            else if (i < 40) decay = 93;
            else decay = 90;       
            bin_amplitudes[i] = (bin_amplitudes[i] * decay) / 100;          
        }
    }

    if (buffer_needs_calc) {
        if (active_buffer == 0)
            calculate_next(audio_buffer_1);
        else
            calculate_next(audio_buffer_0);
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