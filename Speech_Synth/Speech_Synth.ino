// Formant speech synthesis //

#define SAMPLE_RATE 44100
#define TABLE_SIZE 256
#define FORMANT_SZ 7

int frameTime = 15;
uint16_t basePitch;
int formantScale = 108;
uint8_t noiseMod = 75;
uint32_t noiseState = 123456789;

const int16_t sinTable[TABLE_SIZE] PROGMEM = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29955,
    30272, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30272, 29955, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804,
    0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29955,
    -30272, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30272, -29955, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804
};

const int16_t sqrTable[TABLE_SIZE] PROGMEM = {
    0, 402, 804, 1206, 1608, 2010, 2412, 2814, 3216, 3618, 4020, 4422, 4824, 5226, 5628, 6030,
    6432, 6834, 7236, 7638, 8040, 8442, 8844, 9246, 9648, 10050, 10452, 10854, 11256, 11658, 12060, 12462,
    12864, 13266, 13668, 14070, 14472, 14874, 15276, 15678, 16080, 16482, 16884, 17286, 17688, 18090, 18492, 18894,
    19296, 19698, 20100, 20502, 20904, 21306, 21708, 22110, 22512, 22914, 23316, 23718, 24120, 24522, 24924, 25326,
    25728, 26130, 26532, 26934, 27336, 27738, 28140, 28542, 28944, 29346, 29748, 30150, 30552, 30954, 31356, 31758,
    32160, 32562, 32767, 32562, 32160, 31758, 31356, 30954, 30552, 30150, 29748, 29346, 28944, 28542, 28140, 27738,
    27336, 26934, 26532, 26130, 25728, 25326, 24924, 24522, 24120, 23718, 23316, 22914, 22512, 22110, 21708, 21306,
    20904, 20502, 20100, 19698, 19296, 18894, 18492, 18090, 17688, 17286, 16884, 16482, 16080, 15678, 15276, 14874,
    14472, 14070, 13668, 13266, 12864, 12462, 12060, 11658, 11256, 10854, 10452, 10050, 9648, 9246, 8844, 8442,
    8040, 7638, 7236, 6834, 6432, 6030, 5628, 5226, 4824, 4422, 4020, 3618, 3216, 2814, 2412, 2010,
    1608, 1206, 804, 402, 0, -402, -804, -1206, -1608, -2010, -2412, -2814, -3216, -3618, -4020, -4422,
    -4824, -5226, -5628, -6030, -6432, -6834, -7236, -7638, -8040, -8442, -8844, -9246, -9648, -10050, -10452, -10854,
    -11256, -11658, -12060, -12462, -12864, -13266, -13668, -14070, -14472, -14874, -15276, -15678, -16080, -16482, -16884, -17286,
    -17688, -18090, -18492, -18894, -19296, -19698, -20100, -20502, -20904, -21306, -21708, -22110, -22512, -22914, -23316, -23718,
    -24120, -24522, -24924, -25326, -25728, -26130, -26532, -26934, -27336, -27738, -28140, -28542, -28944, -29346, -29748, -30150,
    -30552, -30954, -31356, -31758, -32160, -32562, -32767, -32562, -32160, -31758, -31356, -30954, -30552, -30150, -29748, -29346
};

uint16_t pitchPhase, form1Phase, form2Phase, form3Phase;
uint16_t pitchPhaseInc, form1PhaseInc, form2PhaseInc, form3PhaseInc;
uint8_t form1Amp, form2Amp, form3Amp;

enum {
    _SP,_DOT,_QM,_COM,_HYP,_IY,_IH,_EH,_AE,_AA,
    _AH,_AO,_UH,_AX,_IX,_ER,_UX,_OH,_RX,_LX,
    _WX,_YX,_WH,_R,_L,_W,_Y,_M,_N,_NX,
    _DX,_Q,_S,_SH,_F,_TH,__H,__X,_Z,_ZH,
    _V,_DH,_CHa,_CHb,_Ja,_Jb,_Jc,_Jd,_EY,_AY,
    _OY,_AW,_OW,_UW,_Ba,_Bb,_Bc,_Da,_Db,_Dc,
    _Ga,_Gb,_Gc,_GXa,_GXb,_GXc,_Pa,_Pb,_Pc,_Ta,
    _Tb,_Tc,_Ka,_Kb,_Kc,_KXa,_KXb,_KXc
};

const uint8_t formantTable[] PROGMEM = {
   0x0, 0x0, 0x0,0x0,0x0,0x0,0x0, 0x13,0x43,0x5b,0x0,0x0,0x0,0x0,
  0x13,0x43,0x5b,0x0,0x0,0x0,0x0,     0x13,0x43,0x5b,0x0,0x0,0x0,0x0,
  0x13,0x43,0x5b,0x0,0x0,0x0,0x0,      0xa,0x54,0x6e,0xd,0xa,0x8,0x0,
   0xe,0x49,0x5d,0xd,0x8,0x7,0x0,    0x13,0x43,0x5b,0xe,0xd,0x8,0x0,
  0x18,0x3f,0x58,0xf,0xe,0x8,0x0,    0x1b,0x28,0x59,0xf,0xd,0x1,0x0,
  0x17,0x2c,0x57,0xf,0xc,0x1,0x0,    0x15,0x1f,0x58,0xf,0xc,0x0,0x0,
  0x10,0x25,0x52,0xf,0xb,0x1,0x0,    0x14,0x2c,0x57,0xe,0xb,0x0,0x0,
   0xe,0x49,0x5d,0xd,0xb,0x7,0x0,    0x12,0x31,0x3e,0xc,0xb,0x5,0x0,
   0xe,0x24,0x52,0xf,0xc,0x1,0x0,    0x12,0x1e,0x58,0xf,0xc,0x0,0x0,
  0x12,0x33,0x3e,0xd,0xc,0x6,0x0,    0x10,0x25,0x6e,0xd,0x8,0x1,0x0,
   0xd,0x1d,0x50,0xd,0x8,0x0,0x0,     0xf,0x45,0x5d,0xe,0xc,0x7,0x0,
   0xb,0x18,0x5a,0xd,0x8,0x0,0x0,    0x12,0x32,0x3c,0xc,0xa,0x5,0x0,
   0xe,0x1e,0x6e,0xd,0x8,0x1,0x0,      0xb,0x18,0x5a,0xd,0x8,0x0,0x0,
   0x9,0x53,0x6e,0xd,0xa,0x8,0x0,      0x6,0x2e,0x51,0xc,0x3,0x0,0x0,
   0x6,0x36,0x79,0x9,0x9,0x0,0x0,      0x6,0x56,0x65,0x9,0x6,0x3,0x0,
   0x6,0x36,0x79,0x0,0x0,0x0,0x0,    0x11,0x43,0x5b,0x0,0x0,0x0,0x0,
   0x6,0x49,0x63,0x7,0xa,0xd,0xf,      0x6,0x4f,0x6a,0x0,0x0,0x0,0x0,
   0x6,0x1a,0x51,0x3,0x3,0x3,0xf,      0x6,0x42,0x79,0x0,0x0,0x0,0x0,
   0xe,0x49,0x5d,0x0,0x0,0x0,0x0,    0x10,0x25,0x52,0x0,0x0,0x0,0x0,
   0x9,0x33,0x5d,0xf,0x3,0x0,0x3,      0xa,0x42,0x67,0xb,0x5,0x1,0x0,
   0x8,0x28,0x4c,0xb,0x3,0x0,0x0,      0xa,0x2f,0x5d,0xb,0x4,0x0,0x0,
   0x6,0x4f,0x65,0x0,0x0,0x0,0x0,    0x6,0x4f,0x65,0x0,0x0,0x0,0x0,
   0x6,0x42,0x79,0x1,0x0,0x0,0x0,     0x5,0x42,0x79,0x1,0x0,0x0,0x0,
   0x6,0x6e,0x79,0x0,0xa,0xe,0x0,     0x0, 0x0, 0x0,0x2,0x2,0x1,0x0,
  0x13,0x48,0x5a,0xe,0xe,0x9,0x0,    0x1b,0x27,0x58,0xf,0xd,0x1,0x0,
  0x15,0x1f,0x58,0xf,0xc,0x0,0x0,    0x1b,0x2b,0x58,0xf,0xd,0x1,0x0,
  0x12,0x1e,0x58,0xf,0xc,0x0,0x0,     0xd,0x22,0x52,0xd,0x8,0x0,0x0,
   0x6,0x1a,0x51,0x2,0x0,0x0,0x0,     0x6,0x1a,0x51,0x4,0x1,0x0,0xf,
   0x6,0x1a,0x51,0x0,0x0,0x0,0x0,     0x6,0x42,0x79,0x2,0x0,0x0,0x0,
   0x6,0x42,0x79,0x4,0x1,0x0,0xf,     0x6,0x42,0x79,0x0,0x0,0x0,0x0,
   0x6,0x6e,0x70,0x1,0x0,0x0,0x0,     0x6,0x6e,0x6e,0x4,0x1,0x0,0xf,
   0x6,0x6e,0x6e,0x0,0x0,0x0,0x0,     0x6,0x54,0x5e,0x1,0x0,0x0,0x0,
   0x6,0x54,0x5e,0x4,0x1,0x0,0xf,    0x6,0x54,0x5e,0x0,0x0,0x0,0x0,
   0x6,0x1a,0x51,0x0,0x0,0x0,0x0,     0x6,0x1a,0x51,0x0,0x0,0x0,0x0,
   0x6,0x1a,0x51,0x0,0x0,0x0,0x0,     0x6,0x42,0x79,0x0,0x0,0x0,0x0,
   0x6,0x42,0x79,0x0,0x0,0x0,0x0,     0x6,0x42,0x79,0x0,0x0,0x0,0x0,
   0x6,0x6d,0x65,0x0,0x0,0x0,0x0,     0xa,0x56,0x65,0xc,0xa,0x7,0x0,
   0xa,0x6d,0x70,0x0,0x0,0x0,0x0,     0x6,0x54,0x5e,0x0,0x0,0x0,0x0,
   0x6,0x54,0x5e,0x0,0xa,0x5,0x0,    0x6,0x54,0x5e,0x0,0x0,0x0,0x0
};

uint16_t pitchTable[64] = {
    58,61,65,69,73,77,82,86,92,97,
    103,109,115,122,129,137,145,154,163,173,
    183,194,206,218,231,244,259,274,291,308,
    326,346,366,388,411,435,461,489,518,549,
    581,616,652,691,732,776,822,871,923,978,
    1036,1097,1163,1232,1305,1383,1465,1552,1644,1742,
    1845,1955,2071,2195
};

const uint8_t frameList[] PROGMEM = {
#if 1
  _Da,3,0,39,_Db,1,0,39,_Dc,1,3,39,_EY,8,6,39,_YX,20,3,39,
  _Z,10,0,36,_IY,35,3,36,
  _Da,3,0,32,_Db,1,0,32,_Dc,1,3,32,_EY,8,6,32,_YX,20,3,32,
  _Z,10,0,27,_IY,35,3,27,
  _Ga,2,0,29,_Gb,2,0,29,_Gc,2,0,29,_IH,10,3,29,_V,5,0,29,
  _M,2,0,31,_IY,10,3,31,
  _YX,5,0,32,_AO,10,0,32,_RX,5,0,32,
  _AH,25,0,29,_NX,5,0,29,
  _S,2,0,32,_ER,10,0,32,_RX,3,0,32,
  _Da,3,0,27,_Db,1,0,27,_Dc,1,3,27,_UX,80,3,27,_WX,5,0,27,
  _AY,5,20,34,_YX,10,0,34,_M,8,0,34,
  __H,5,0,39,_AX,30,0,39,_F,10,0,39,
  _Ka,3,0,36,_Kb,3,0,36,_Kc,4,0,36,_R,5,0,36,_EY,30,0,36,
  _Z,5,0,32,_IY,40,0,32,
  _AO,10,0,29,_LX,5,0,29,
  _F,5,0,31,_AO,10,0,31,
  _DH,5,0,32,_AH,10,0,32,
  _L,5,0,34,_AH,20,0,34,_V,5,0,34,
  _AA,10,0,36,_V,5,0,36,
  _Y,10,0,34,_UX,80,0,34,
  _IH,10,0,36,_Ta,2,0,36,_Tb,1,0,36,_Tc,2,0,36,
  _W,2,0,37,_OH,10,0,37,_N,1,0,37,_Ta,1,0,37,_Tb,1,0,37,_Tc,1,0,37,
  _Ba,2,0,36,_Bb,1,0,36,_Bc,2,0,36,_IY,10,0,36,
  _AH,15,0,34,
  _S,2,0,39,_Ta,2,0,39,_Tb,2,0,39,_Tc,2,0,39,_AY,1,10,39,_YX,10,0,39,
  _L,3,0,36,_IH,10,0,36,_SH,2,0,36,
  _M,5,0,34,_AE,10,0,34,
  _R,5,0,32,_IH,60,0,32,_Ja,2,0,32,_Jb,2,0,32,_Jc,2,0,32,
  _AY,5,10,34,_YX,5,0,34,
  _Ka,2,0,36,_Kb,2,0,36,_Kc,2,0,36,_AH,20,0,36,_N,2,0,36,_Ta,2,0,36,_Tb,2,0,26,_Tc,2,0,36,
  _AX,15,0,32,
  _F,5,0,29,_AO,20,0,29,_R,2,0,29,_Da,1,0,29,_Db,1,0,29,_Dc,1,0,29,
  _AX,15,0,32,
  _Ka,1,0,29,_Kb,1,0,29,_Kc,1,0,29,_AE,12,0,29,
  _R,5,0,27,_IH,45,0,27,_Ja,2,0,27,_Jb,2,0,27,_Jc,2,0,27,
  _Ba,1,0,27,_Bb,1,0,27,_Bc,1,0,27,_AH,10,0,27,_Ta,1,0,27,_Tb,1,0,27,_Tc,1,0,27,
  _Y,5,0,32,_UH,10,10,32,_L,5,0,32,
  _L,3,0,36,_UH,10,0,36,_Ka,1,0,36,_Kb,1,0,36,_Kc,1,0,36,
  _S,2,0,34,_W,2,0,34,_IY,20,0,34,_Ta,2,0,34,_Tb,2,0,34,_Tc,2,0,34,
  _AX,15,0,27,
  _Ka,2,0,32,_Kb,2,0,32,_Kc,2,0,32,_R,2,0,32,_AA,20,0,32,_S,5,0,32,
  _DH,5,0,36,_AH,10,0,36,
  _S,2,0,34,_IY,10,0,34,_Ta,2,0,34,_Tb,2,0,34,_Tc,2,0,34,
  _AA,10,0,36,_V,5,0,36,
  _AE,15,0,37,
  _Ba,2,0,39,_Bb,2,0,39,_Bc,2,0,39,_AY,5,5,39,_YX,5,0,39,
  _S,5,0,36,_IH,10,0,36,
  _Ka,2,0,32,_Kb,2,0,32,_Kc,2,0,32,_L,9,0,32,
  _M,2,0,34,_EY,5,10,34,_YX,10,0,34,_Da,2,0,34,_Db,2,0,34,_Dc,2,0,34,
  _F,5,0,27,_OY,1,5,27,_RX,5,0,27,
  _Ta,2,0,32,_Tb,2,0,32,_Tc,2,0,32,_UX,50,0,32,
#endif
  _Ta,0,0,61
};

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

    noiseState = entropy;

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

    const uint8_t *framePos = frameList;

    while(1) {
        uint8_t startFormant, staticFrames, tweenFrames;
        uint16_t startPitch, nextPitch;
        uint8_t nextFormant;
        int16_t startForm1PhaseInc, startForm2PhaseInc, startForm3PhaseInc;
        uint8_t startForm1Amp, startForm2Amp, startForm3Amp;
        uint8_t startMod;
        const uint8_t *formantPos;

        startFormant = pgm_read_byte(framePos++);
        staticFrames = pgm_read_byte(framePos++);

        if (!staticFrames) break;

        tweenFrames = pgm_read_byte(framePos++);

        startPitch = pitchTable[pgm_read_byte(framePos++)];
        nextFormant = pgm_read_byte(framePos);
        nextPitch = pitchTable[pgm_read_byte(framePos + 3)];

        pitchPhaseInc = startPitch;
        formantPos = formantTable + startFormant * FORMANT_SZ;

        form1PhaseInc = startForm1PhaseInc = (int16_t)pgm_read_byte(formantPos++) * formantScale;
        form2PhaseInc = startForm2PhaseInc = (int16_t)pgm_read_byte(formantPos++) * formantScale;
        form3PhaseInc = startForm3PhaseInc = (int16_t)pgm_read_byte(formantPos++) * formantScale;
        form1Amp = startForm1Amp = pgm_read_byte(formantPos++);
        form2Amp = startForm2Amp = pgm_read_byte(formantPos++);
        form3Amp = startForm3Amp = pgm_read_byte(formantPos++);
        noiseMod = startMod = pgm_read_byte(formantPos++);

        for (; staticFrames--;) delay(frameTime);

        if (tweenFrames) {
            const uint8_t* formantPos2;
            int16_t deltaForm1PhaseInc, deltaForm2PhaseInc, deltaForm3PhaseInc;
            int8_t deltaForm1Amp, deltaForm2Amp, deltaForm3Amp;
            int8_t deltaMod;
            int16_t deltaPitch;

            tweenFrames--;
            formantPos2 = formantTable + nextFormant * FORMANT_SZ;

            deltaForm1PhaseInc = (int16_t)pgm_read_byte(formantPos2++) * formantScale - startForm1PhaseInc;
            deltaForm2PhaseInc = (int16_t)pgm_read_byte(formantPos2++) * formantScale - startForm2PhaseInc;
            deltaForm3PhaseInc = (int16_t)pgm_read_byte(formantPos2++) * formantScale - startForm3PhaseInc;
            deltaForm1Amp = pgm_read_byte(formantPos2++) - startForm1Amp;
            deltaForm2Amp = pgm_read_byte(formantPos2++) - startForm2Amp;
            deltaForm3Amp = pgm_read_byte(formantPos2++) - startForm3Amp;
            deltaMod = pgm_read_byte(formantPos2++) - startMod;
            deltaPitch = nextPitch - startPitch;

            for (int i = 1; i <= tweenFrames; i++) {
                form1PhaseInc = startForm1PhaseInc + (i * deltaForm1PhaseInc) / tweenFrames;
                form2PhaseInc = startForm2PhaseInc + (i * deltaForm2PhaseInc) / tweenFrames;
                form3PhaseInc = startForm3PhaseInc + (i * deltaForm3PhaseInc) / tweenFrames;
                form1Amp = startForm1Amp + (i * deltaForm1Amp) / tweenFrames;
                form2Amp = startForm2Amp + (i * deltaForm2Amp) / tweenFrames;
                form3Amp = startForm3Amp + (i * deltaForm3Amp) / tweenFrames;
                pitchPhaseInc = startPitch + (i * deltaPitch) / tweenFrames;
                noiseMod = startMod + (i * deltaMod) / tweenFrames;
                delay(frameTime);
            }
        }
    }

}
    
ISR(TCB0_INT_vect) {

    int32_t sum = 0;
    static int32_t prevSample = 0;
    static int16_t smoothNoise = 0;

    form1Phase += form1PhaseInc;
    uint8_t idx1 = (form1Phase >> 8) & 0xFF;
    uint8_t frac1 = form1Phase & 0xFF;
    int16_t sample1a = pgm_read_word(&sinTable[idx1]);
    int16_t sample1b = pgm_read_word(&sinTable[(idx1 + 1) & 0xFF]);
    int16_t sample1 = sample1a + ((int32_t)(sample1b - sample1a) * frac1 >> 8);
    sum += (int32_t)sample1 * form1Amp;

    form2Phase += form2PhaseInc;
    uint8_t idx2 = (form2Phase >> 8) & 0xFF;
    uint8_t frac2 = form2Phase & 0xFF;
    int16_t sample2a = pgm_read_word(&sinTable[idx2]);
    int16_t sample2b = pgm_read_word(&sinTable[(idx2 + 1) & 0xFF]);
    int16_t sample2 = sample2a + ((int32_t)(sample2b - sample2a) * frac2 >> 8);
    sum += (int32_t)sample2 * form2Amp;

    form3Phase += form3PhaseInc;
    uint8_t idx3 = (form3Phase >> 8) & 0xFF;
    uint8_t frac3 = form3Phase & 0xFF;
    int16_t sample3a = pgm_read_word(&sqrTable[idx3]);
    int16_t sample3b = pgm_read_word(&sqrTable[(idx3 + 1) & 0xFF]);
    int16_t sample3 = sample3a + ((int32_t)(sample3b - sample3a) * frac3 >> 8);
    sum += ((int32_t)sample3 * form3Amp) >> 2;

    noiseState = (noiseState * 1664525 + 1013904223) & 0xFFFFFFFF;
    int16_t rawNoise = (noiseState >> 16) & 0xFFFF;
    smoothNoise = smoothNoise + ((rawNoise - smoothNoise) >> 3);
    
    if (smoothNoise > 12000) smoothNoise = 12000;
    if (smoothNoise < -12000) smoothNoise = -12000;
    
    sum += (int32_t)smoothNoise * noiseMod;

    uint16_t pitchEnv = (pitchPhase >> 8) & 0xFF;
    sum = (sum * (255 - pitchEnv)) >> 8;

    pitchPhase += pitchPhaseInc;

    int32_t filtered = (sum + prevSample) >> 1;
    prevSample = sum;
    sum = filtered;

    int16_t out = sum >> 12;
    if (out < -512) out = -512;
    if (out > 511) out = 511;

    uint16_t dacValue = out + 512;
    DAC0.DATAL = (dacValue & 0x03) << 6;
    DAC0.DATAH = dacValue >> 2;

    TCB0.INTFLAGS = TCB_CAPT_bm;

}