#include <Arduino.h>

// A0 frequency input, D5 output - needs RC filter (10K, 10u)

// ── Wavetable ─────────────────────────────────────────────────
// Generated at runtime — no hand-counting errors.
// 256 samples, 8-bit unsigned (0=min, 128=zero, 255=max).
// Lives in RAM — 256 bytes, fine for Pro Micro (2.5k available).
uint8_t sinetable[256];

// ── DDS state ─────────────────────────────────────────────────
// 16-bit phase accumulator wraps naturally at 65536.
// Upper 8 bits = wavetable index.
// phaseIncrement = freq * 65536 / SAMPLE_RATE
#define SAMPLE_RATE 62500.0f

volatile uint16_t phaseAccum     = 0;
volatile uint16_t phaseIncrement = 0;

// ── ADC smoothing ─────────────────────────────────────────────
float smoothedRaw = 0;
#define ALPHA 0.2f

// ── Timer 3 init ──────────────────────────────────────────────
// 8-bit Fast PWM, no prescaler → 16MHz/256 = 62.5kHz overflow.
// OCR3A updated each ISR tick produces the audio waveform.
// RC filter on pin 5 smooths PWM into analog voltage.
void initTimer3() {
  DDRC   |= (1 << PC6);
  TCCR3A  = (1 << COM3A1) | (1 << WGM30);
  TCCR3B  = (1 << WGM32)  | (1 << CS30);
  TIMSK3  = (1 << TOIE3);
}

// ── ISR: runs at 62.5kHz ──────────────────────────────────────
ISR(TIMER3_OVF_vect) {
  phaseAccum += phaseIncrement;
  OCR3A = sinetable[phaseAccum >> 8];
}

// ── Frequency control ─────────────────────────────────────────
void setFreq(float freq) {
  uint16_t inc = (uint16_t)(freq * 65536.0f / SAMPLE_RATE);
  cli();
  phaseIncrement = inc;
  sei();
}

void setup() {
  Serial.begin(115200);

  // Generate sine table
  for (int i = 0; i < 256; i++) {
    sinetable[i] = (uint8_t)(128.0f + 127.0f * sinf(2.0f * M_PI * i / 256.0f));
  }

  smoothedRaw = analogRead(A0);
  initTimer3();
}

void loop() {
  int raw = analogRead(A0);
  smoothedRaw += ALPHA * (raw - smoothedRaw);

  // Exponential map: 20Hz–20kHz
  float freq = 20.0f * powf(1000.0f, smoothedRaw / 1023.0f);
  setFreq(freq);

  Serial.print(raw);
  Serial.print(" ");
  Serial.print(smoothedRaw);
  Serial.print(" ");
  Serial.println(freq, 1);

  delay(5);
}
