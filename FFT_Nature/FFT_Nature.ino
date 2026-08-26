// Inverse FFT spectral nature engine //

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

    static uint16_t ambient_counter = 0;
    static float drift_phase = 0;
    static uint8_t drone_layer = 0;
    ambient_counter++;
    
    if (ambient_counter % 1000 < 20) {
        drone_layer = random(0, 6);
        
        switch (drone_layer) {
            case 0:
                bin_amplitudes[2] = 350 + random(0, 150);
                bin_amplitudes[4] = 250 + random(0, 100);
                bin_amplitudes[6] = 150 + random(0, 50);
                break;
                
            case 1:
                bin_amplitudes[8] = 300 + random(0, 150);
                bin_amplitudes[12] = 250 + random(0, 100);
                bin_amplitudes[16] = 100 + random(0, 50);
                break;
                
            case 2:
                bin_amplitudes[20] = 250 + random(0, 120);
                bin_amplitudes[25] = 200 + random(0, 100);
                bin_amplitudes[30] = 100 + random(0, 75);
                break;
                
            case 3:
                bin_amplitudes[35] = 150 + random(0, 100);
                bin_amplitudes[40] = 100 + random(0, 80);
                bin_amplitudes[45] = 50 + random(0, 25);
                break;
                
            case 4:
                for (int i = 10; i < 30; i += 3) {
                    bin_amplitudes[i] += random(20, 80);
                }
                break;
                
            case 5:
                for (int i = 2; i < 50; i++) {
                    if (bin_amplitudes[i] > 0) {
                        bin_amplitudes[i] += (bin_amplitudes[i+2] * 100) / 100;
                    }
                }
                break;
        }
    }
    
    drift_phase += (float)(random(1, 100)/100);
    if (drift_phase > 2 * M_PI) drift_phase -= 2 * M_PI;
    
    int drift_bin = 5 + (int)((sinf(drift_phase) * 0.5 + 0.5) * 20);
    int drift_strength = 100 + (int)((sinf(drift_phase * 0.7) * 0.5 + 0.5) * 50);
    
    bin_amplitudes[drift_bin] += drift_strength;
    bin_amplitudes[drift_bin + 3] += drift_strength / 3;
    bin_amplitudes[drift_bin + 7] += drift_strength / 5;
    
    if (random(1000) < 3) {
        int start_bin = random(10, 40);
        int fall_speed = random(2, 10);
        
        for (int step = 0; step < 6; step++) {
            int bin = start_bin - step * fall_speed;
            if (bin > 0) {
                int amp = 150 - step * 40;
                if (amp > 0) {
                    bin_amplitudes[bin] += amp;
                    
                    if (bin * 2 < N / 2) {
                        bin_amplitudes[bin * 2] += amp / 3;
                    }
                    if (bin * 3 / 2 < N / 2) {
                        bin_amplitudes[bin * 3 / 2] += amp / 5;
                    }
                }
            }
        }
    }
    
    if (ambient_counter % 1500 < 20) {
        int wave_center = (ambient_counter / 50) % 25 + 5;
        int wave_amp = 200 + (int)(sinf(ambient_counter / 100.0f) * 100);
        
        for (int i = -3; i <= 3; i++) {
            int bin = wave_center + i;
            if (bin > 0 && bin < N / 2) {
                bin_amplitudes[bin] += (wave_amp * (4 - abs(i))) / 4;
            }
        }
    }
    
    if (ambient_counter % 3000 < 50) {
        for (int i = 1; i < 20; i++) {
            float phase_shift = i * 0.3;
            float breath_effect = sinf(ambient_counter / 1000.0f * 2 * M_PI + phase_shift);
            int effect = (int)((breath_effect * 0.5 + 0.5) * 25);
            
            if (bin_amplitudes[i] > 0) {
                bin_amplitudes[i] += effect;
            }
        }
    }
    
    if (random(2000) < 2) {
        int droplet = random(8, 40);
        bin_amplitudes[droplet] += random(50, 200);
        bin_amplitudes[droplet + 2] += random(25, 100);
        
        if (droplet + 5 < N / 2) {
            bin_amplitudes[droplet + 4] += random(15, 60);
        }
    }
    
    for (int i = 0; i < N / 2; i++) {
        if (bin_amplitudes[i] > 0) {       
            bin_amplitudes[i] = (bin_amplitudes[i] * 99) / 100;        
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