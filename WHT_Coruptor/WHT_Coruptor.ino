// Inverted WHT (Walsh-Hadamard) spectral particle coruptor //

#define SAMPLE_RATE 44100

#define LOG2_N 8
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
            real_q15[i] = (random(100) > 50) ? bin_amplitudes[i] : -bin_amplitudes[i];
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