// 4-voice lo-fi chip music generator //

#define SAMPLE_RATE 8000
#define BPM 75

volatile uint32_t t = 0;
volatile uint32_t step_t = 0;
uint32_t samples_per_step;

volatile uint32_t ph_pd = 0;
volatile uint32_t inc_pd = 0;
volatile uint16_t env_pd = 0;
volatile uint8_t pd_waveform = 0;

volatile uint32_t ph_wt = 0;
volatile uint32_t inc_wt = 0;
volatile uint16_t env_wt = 0;

const int8_t wt_sine[16] = {0, 12, 25, 36, 47, 55, 60, 63, 63, 60, 55, 47, 36, 25, 12, 0};
const int8_t wt_square[16] = {31, 31, 31, 31, 31, 31, 31, 31, -32, -32, -32, -32, -32, -32, -32, -32};
const int8_t wt_saw[16] = {-32, -28, -24, -20, -16, -12, -8, -4, 0, 4, 8, 12, 16, 20, 24, 28};
const int8_t wt_triangle[16] = {-32, -24, -16, -8, 0, 8, 16, 24, 31, 24, 16, 8, 0, -8, -16, -24};

volatile uint32_t ph_acid = 0;
volatile uint32_t ph_fm = 0;
volatile uint32_t ph_ring = 0;

volatile uint16_t env_a = 0;
volatile uint16_t env_r = 0;
volatile uint32_t inc_acid = 0;
volatile uint32_t inc_fm = 0;

volatile uint16_t env_pd_decay = 0;
volatile uint16_t env_wt_decay = 0;

const uint16_t notes[] = {220, 247, 261, 293, 329, 349, 392, 440};

void setup() {

  randomSeed(analogRead(0));

  PORTD.PIN6CTRL = PORT_ISC_INPUT_DISABLE_gc;
  //VREF.DAC0REF = VREF_REFSEL_VDD_gc | VREF_ALWAYSON_bm;
  VREF.DAC0REF = VREF_REFSEL_2V048_gc | VREF_ALWAYSON_bm;
  DAC0.CTRLA = DAC_ENABLE_bm | DAC_OUTEN_bm | DAC_RUNSTDBY_bm;

  samples_per_step = (uint32_t)SAMPLE_RATE * 60 / BPM / 4;

  TCB0.CTRLA = 0;
  TCB0.CTRLB = TCB_CNTMODE_INT_gc;
  TCB0.CCMP = (F_CPU / SAMPLE_RATE) - 1;
  TCB0.INTCTRL = TCB_CAPT_bm;
  TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_CLKDIV1_gc;

}

void loop() {

  if(step_t >= samples_per_step) {
    step_t = 0;
    static uint32_t long_s = 0;
    long_s++;

    uint8_t step = long_s % 16;
    uint16_t bar  = (long_s >> 4) % 32;
    uint8_t section = (bar / 8) % 4;

    if (step == 0 || step == 8 || step == 4) {
      uint16_t pd_notes[] = {110, 147, 165, 220, 131, 196, 174, 247};
      uint8_t note_idx = (bar + section) % 8;

      env_pd = 200 << 8;
      env_pd_decay = 200 << 8;
      inc_pd = ((uint32_t)pd_notes[note_idx] * 4294967296ULL) / SAMPLE_RATE;

      pd_waveform = (bar % 3 == 0) ? 0 : (bar % 2 == 0) ? 1 : 2;

      if (random(0, 5) == 0) inc_pd >>= 1;

      ph_pd = 0;
    }

    if (step == 2 || step == 6 || step == 10 || step == 14) {
      uint8_t wt_notes[] = {0, 3, 5, 7, 2, 4, 6, 8};
      uint8_t note_idx = (bar + step/2) % 8;

      env_wt = 180 << 8;
      env_wt_decay = 180 << 8;
      inc_wt = ((uint32_t)notes[wt_notes[note_idx] % 8] * 4294967296ULL) / SAMPLE_RATE;

      if (random(0, 4) == 0) inc_wt = inc_wt * 3 / 2;
    }

    bool acid_trigger = false;

    if (step == 0 || step == 3 || step == 5 || step == 7 ||
        step == 10 || step == 12 || step == 14) {
      acid_trigger = true;
    }

    if (step == 2 && (bar % 4 == 2)) acid_trigger = true;
    if (step == 6 && (bar % 3 == 1)) acid_trigger = true;
    if (random(0, 8) == 0) acid_trigger = true;

    if (acid_trigger) {
      uint8_t note_seq[] = {0, 2, 4, 5, 7, 4, 2, 0,
                            3, 5, 7, 9, 7, 5, 3, 2,
                            4, 7, 9, 11, 9, 7, 4, 5,
                            0, 3, 5, 7, 9, 7, 5, 4};

      uint8_t seq_pos = (bar * 2 + step / 2) % 32;
      uint8_t note_idx = note_seq[seq_pos];

      int8_t transpose = 0;
      if (section == 1) transpose = 2;
      else if (section == 2) transpose = -3;
      else if (section == 3) transpose = 1;

      note_idx = (note_idx + transpose + 12) % 8;

      if (random(0, 12) == 0) note_idx += 8;

      inc_acid = ((uint32_t)notes[note_idx % 8] * 4294967296ULL) / SAMPLE_RATE;

      if (random(0, 6) == 0) inc_acid >>= 1;

      env_a = 255 << 8;

      if (step == 0 || step == 8) {
        env_a = 255 << 8;
      } else if (step % 4 == 0) {
        env_a = 180 << 8;
      } else {
        env_a = 120 << 8;
      }

      ph_acid = 0;
    }

    if (step == 2 || step == 6 || step == 11 || step == 15) {
      uint8_t ring_seq[] = {0, 3, 5, 7, 2, 4, 6, 8,
                            1, 4, 6, 9, 3, 5, 7, 10};

      uint8_t ring_pos = (bar + step) % 16;
      uint8_t ring_note = ring_seq[ring_pos] % 8;

      if (section % 2 == 0) ring_note = (ring_note + 2) % 8;

      env_r = (235 + random(0, 20)) << 8;
      inc_fm = ((uint32_t)notes[ring_note] * 4294967296ULL) / SAMPLE_RATE;

      if (random(0, 5) == 0) inc_fm = inc_fm * 3 / 2;
      if (random(0, 8) == 0) inc_fm <<= 1;
    }
  }

}

ISR(TCB0_INT_vect) {

  t++;
  step_t++;

  static uint32_t lfo_phase = 0;
  static uint8_t lfo_wave = 0;
  lfo_phase += 25;

  uint8_t idx = (lfo_phase >> 12) & 0x0F;
  int8_t lfo_value = (lfo_wave == 0) ? wt_sine[idx] : wt_triangle[idx];

  int8_t vibrato_depth = 8;
  int8_t tremolo_depth = 12;

  int16_t vibrato = (int16_t)lfo_value * vibrato_depth / 32;
  int16_t tremolo = (int16_t)(32 + lfo_value * tremolo_depth / 32);

  int16_t pd_out = 0;
  if(env_pd > 0) {
    ph_pd += inc_pd;

    uint32_t phase = ph_pd >> 16;
    uint16_t pd_phase = phase & 0x3FFF;

    int16_t sample;
    switch(pd_waveform) {
      case 0: {
        uint16_t idx = (pd_phase * 16) >> 14;
        sample = wt_sine[idx] * 512;
        break;
      }
      case 1:
        sample = (pd_phase < 8192) ? 16384 : -16384;
        break;
      case 2:
        sample = ((int32_t)pd_phase - 8192) * 4;
        break;
    }

    uint16_t env_pd_trem = (uint16_t)((int32_t)env_pd * tremolo / 32);
    pd_out = (int16_t)(((int32_t)sample * (env_pd_trem >> 8)) >> 7);

    if(env_pd > 0) {
      if(env_pd > (200 << 6)) env_pd -= 512;
      else env_pd -= 32;
    }
  }

  int16_t wt_out = 0;
  if(env_wt > 0) {
    uint32_t inc_wt_vibrato = inc_wt + (int32_t)vibrato * 80;
    ph_wt += inc_wt_vibrato;

    uint16_t idx_frac = (ph_wt >> 8) & 0xFF;
    uint8_t idx = idx_frac >> 4;

    int8_t sample;
    if ((ph_wt >> 20) & 1)
      sample = wt_square[idx];
    else
      sample = wt_sine[idx];

    wt_out = (int16_t)(((int32_t)sample * 512 * (env_wt >> 8)) >> 7);

    if(env_wt > 0) {
      if(env_wt > (180 << 6)) env_wt -= 384;
      else env_wt -= 24;
    }
  }

  uint32_t inc_acid_vibrato = inc_acid + (int32_t)vibrato * 60;
  ph_acid += inc_acid_vibrato;

  int32_t saw = ((ph_acid >> 16) & 0xFFFF) - 32768;
  int32_t acid = (saw * saw) >> 15;
  if(saw < 0) acid = -acid;

  if(env_a > 0)
    acid = (int32_t)acid * (env_a >> 8) >> 7;
  else
    acid = 0;

  ph_fm += inc_fm + (ph_acid >> 16);
  ph_ring += inc_fm >> 1;

  int16_t fm_base = (ph_fm & 0x80000000) ? 4096 : -4096;
  int16_t ring_base = (ph_ring & 0x80000000) ? 4096 : -4096;
  int16_t ring = (int16_t)(((int32_t)fm_base * ring_base) >> 15);

  if(env_r > 0)
    ring = (int16_t)(((int32_t)ring * (env_r >> 8)) >> 7);
  else
    ring = 0;

  static uint8_t acid_counter = 0;
  acid_counter++;
  if(acid_counter >= 8) {
    acid_counter = 0;
    if(env_a > 0) {
      if(env_a > (80 << 8)) env_a -= (2 << 8);
      else if(env_a > (25 << 8)) env_a -= (1 << 8);
      else env_a = (env_a > 256) ? env_a - 256 : 0;
    }
  }

  static uint8_t ring_counter = 0;
  ring_counter++;
  if(ring_counter >= 32) {
    ring_counter = 0;
    if(env_r > 0) {
      if(env_r > (100 << 8)) env_r -= (2 << 8);
      else if(env_r > (40 << 8)) env_r -= (1 << 8);
      else env_r = (env_r > 256) ? env_r - 256 : 0;
    }
  }

  int32_t mixed = (pd_out >> 2) + (wt_out >> 2) + (acid >> 3) + (ring << 1);

  if(mixed > 16383) mixed = 16383;
  if(mixed < -16384) mixed = -16384;

  uint16_t val = 512 + (mixed >> 5);

  DAC0.DATAL = (val & 0x03) << 6;
  DAC0.DATAH = val >> 2;

  TCB0.INTFLAGS = TCB_CAPT_bm;

}