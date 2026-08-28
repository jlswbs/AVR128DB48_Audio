// Inverse FFT spectral mystery engine //

#define SAMPLE_RATE 24000

#define LOG2_N 6
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
 
    static uint16_t glitch_counter = 0; 
    static uint8_t pattern_step = 0; 
    static uint8_t melody_note = 0; 
    glitch_counter++; 
     
    if (glitch_counter % 1000 < 100) { 
        pattern_step = (pattern_step + 1) % 16; 
         
        int bass_notes[] = {2, 5, 3, 7, 2, 4, 6, 3}; 
        int bass_pattern = bass_notes[pattern_step % 8]; 
        bin_amplitudes[bass_pattern] += random(500, 1500); 
        bin_amplitudes[bass_pattern + 1] += random(200, 600); 
         
        if (random(100) < 30) { 
            bin_amplitudes[bass_pattern + 3] += random(100, 400); 
            bin_amplitudes[bass_pattern - 1] += random(50, 200); 
        } 
    } 
     
    if (glitch_counter % 80 < 5) { 
        melody_note = (melody_note + 1) % 12; 
         
        int melody[] = {12, 15, 19, 14, 17, 20, 16, 18, 13, 21, 15, 22}; 
        int note = melody[melody_note % 12]; 
         
        bin_amplitudes[note] += random(300, 750); 
        bin_amplitudes[note + 2] += random(100, 300); 
        bin_amplitudes[note + 4] += random(50, 150); 
        bin_amplitudes[note + 7] += random(30, 100); 
         
        if (random(100) < 10) { 
            bin_amplitudes[note + random(1, 5)] += random(100, 300); 
        } 
    } 
     
    if (glitch_counter % 30 < 3) { 
        int click = 1 + random(0, 3); 
        bin_amplitudes[click] += random(100, 500); 
        bin_amplitudes[click + 2] += random(50, 200); 
         
        if (random(100) < 50) { 
            bin_amplitudes[click + random(1, 4)] += random(50, 150); 
        } 
    } 
     
    if (glitch_counter % 120 < 8) { 
        int chord_base = 10 + random(0, 10); 
        int chord_type = random(0, 3); 
         
        switch (chord_type) { 
            case 0: 
                bin_amplitudes[chord_base] += random(200, 600); 
                bin_amplitudes[chord_base + 4] += random(150, 400); 
                bin_amplitudes[chord_base + 7] += random(100, 300); 
                break; 
                 
            case 1: 
                bin_amplitudes[chord_base] += random(300, 500); 
                bin_amplitudes[chord_base + 3] += random(200, 400); 
                bin_amplitudes[chord_base + 7] += random(150, 300); 
                bin_amplitudes[chord_base + 10] += random(100, 200); 
                break; 
                 
            case 2: 
                for (int i = 0; i < 5; i++) { 
                    int glitch_note = chord_base + random(0, 12); 
                    bin_amplitudes[glitch_note] += random(50, 200); 
                } 
                break; 
        } 
    } 
     
    if (glitch_counter % 45 < 4) { 
        int arp_base = 18 + (glitch_counter / 45) % 8; 
        int arp_notes[] = {0, 4, 7, 12, 7, 4, 0, 3}; 
        int arp_note = arp_base + arp_notes[(glitch_counter / 45) % 8]; 
         
        bin_amplitudes[arp_note] += random(200, 500); 
        bin_amplitudes[arp_note + 1] += random(100, 200); 
         
        if (random(100) < 25) { 
            bin_amplitudes[arp_note + random(2, 5)] += random(100, 300); 
        } 
    } 
     
    if (random(200) < 3) { 
        int noise_center = random(5, 30); 
        int noise_width = random(2, 6); 
         
        for (int i = 0; i < noise_width; i++) { 
            int bin = noise_center + i * 2; 
            if (bin < N / 2) { 
                bin_amplitudes[bin] += random(50, 200); 
                bin_amplitudes[bin + 1] += random(30, 100); 
            } 
        } 
    } 
     
    if (random(300) < 2) { 
        int glitch_bin = random(3, 30); 
        int glitch_amp = random(250, 800); 
         
        for (int i = 0; i < 3; i++) { 
            int bin = glitch_bin + i * 2; 
            if (bin < N / 2) { 
                bin_amplitudes[bin] += glitch_amp / (i + 1); 
                if (bin / 2 > 0) { 
                    bin_amplitudes[bin / 2] += glitch_amp / (i + 3); 
                } 
            } 
        } 
    } 
     
    for (int i = 0; i < N / 2; i++) { 
        if (bin_amplitudes[i] > 0) { 
            int decay; 
            if (i < 5) decay = 90; 
            else if (i < 15) decay = 85; 
            else if (i < 30) decay = 75; 
            else decay = 70; 
             
            bin_amplitudes[i] = (bin_amplitudes[i] * decay) / 100; 
             
            if (random(100) < 5 && bin_amplitudes[i] > 50) { 
                bin_amplitudes[i] = (bin_amplitudes[i] * 150) / 100; 
            } 
             
            if (bin_amplitudes[i] < 10) bin_amplitudes[i] = 0; 
        } 
    } 
     
    if (glitch_counter % 8000 > 7900 && glitch_counter % 8000 < 7950) { 
        for (int i = 15; i < N / 2; i += 2) { 
            bin_amplitudes[i] = (bin_amplitudes[i] * 60) / 100; 
        } 
        bin_amplitudes[8] += random(350, 1000); 
        bin_amplitudes[12] += random(250, 750); 
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