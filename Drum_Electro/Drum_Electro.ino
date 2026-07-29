// Electro drum samples //

#include "BD.h"
#include "CH.h"
#include "CL.h"
#include "CR.h"
#include "OH.h"
#include "RD.h"
#include "RS.h"
#include "SD.h"

#define SAMPLE_RATE 16000
#define BPM         120

struct Voice {
  const uint8_t* sampleData;
  uint32_t length;
  uint32_t cnt;
  bool active;
};

#define MAX_VOICES 4
volatile Voice voices[MAX_VOICES];

unsigned long lastStepTime = 0;
unsigned long stepDuration = (60000 / BPM) / 4;
int stepCounter = 0;

const uint8_t* const sampleTable[] = { BD, CH, CL, CR, OH, RD, RS, SD };
const uint32_t lengthTable[] = { BDLen, CHLen, CLLen, CRLen, OHLen, RDLen, RSLen, SDLen };

void triggerVoice(int sampleIndex) {

  for (int i = 0; i < MAX_VOICES; i++) {
    if (!voices[i].active) {
      voices[i].sampleData = sampleTable[sampleIndex];
      voices[i].length = lengthTable[sampleIndex];
      voices[i].cnt = 0;
      voices[i].active = true;
      break;
    }
  }

}

void setup() {

  randomSeed(analogRead(A0));

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

  unsigned long currentMillis = millis();

  if (currentMillis - lastStepTime >= stepDuration) {
    lastStepTime = currentMillis;

    if (stepCounter == 0) {
      triggerVoice(0);
    }

    if (random(0, 100) < 60) {
      int randomSample = random(1, 8); 
      triggerVoice(randomSample);
    }

    if (random(0, 100) < 30) {
      int secondSample = random(1, 8);
      triggerVoice(secondSample);
    }

    stepCounter++;
    if (stepCounter >= 4) {
      stepCounter = 0;
    }
  }

}

ISR(TCB0_INT_vect) {

  int16_t mixedSignal = 0;
  int activeCount = 0;

  for (int i = 0; i < MAX_VOICES; i++) {
    if (voices[i].active) {
      int16_t sample = pgm_read_byte_near(voices[i].sampleData + voices[i].cnt);

      sample -= 128;

      mixedSignal += sample;
      activeCount++;

      voices[i].cnt++;
      if (voices[i].cnt >= voices[i].length) {
        voices[i].active = false;
      }
    }
  }

  mixedSignal = (mixedSignal << 2) + 512;

  if (mixedSignal > 1023) mixedSignal = 1023;
  if (mixedSignal < 0) mixedSignal = 0;

  DAC0.DATAL = (mixedSignal & 0x03) << 6;
  DAC0.DATAH = mixedSignal >> 2;

  TCB0.INTFLAGS = TCB_CAPT_bm;

}