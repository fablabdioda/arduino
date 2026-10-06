#include <Arduino.h>

// A0 - częstotliwość oscylatora A  -> wyjście D5 (Timer3, OC3A)
// A1 - częstotliwość oscylatora B  -> wyjście D9 (Timer1, OC1A)
//                    generator szumu -> wyjście D6 (Timer4, OC4D)
// Każde wyjście wymaga własnego filtru RC.

// ── Tablica przebiegu (wspólna dla obu oscylatorów) ───────────
uint8_t sinetable[256];

#define SAMPLE_RATE 62500.0f

// ── Stan DDS ──────────────────────────────────────────────────
volatile uint16_t phaseAccumA = 0;
volatile uint16_t phaseIncA   = 0;
volatile uint16_t phaseAccumB = 0;
volatile uint16_t phaseIncB   = 0;

// ── Stan generatora szumu (xorshift16, nie może być 0) ────────
volatile uint16_t noiseState = 0xACE1;

// ── Wygładzanie odczytu ADC ───────────────────────────────────
float smoothedA = 0;
float smoothedB = 0;
#define ALPHA 0.2f

// ── Inicjalizacja timerów ─────────────────────────────────────
// Wszystkie trzy: 8-bitowy Fast PWM, preskaler 1 → 62,5 kHz.
// Przerwanie generuje tylko Timer3, on odświeża wszystkie wyjścia.
void initTimers() {
  // Timer3 -> D5 (PC6), oscylator A, z przerwaniem
  DDRC   |= (1 << PC6);
  TCCR3A  = (1 << COM3A1) | (1 << WGM30);
  TCCR3B  = (1 << WGM32)  | (1 << CS30);
  TIMSK3  = (1 << TOIE3);

  // Timer1 -> D9 (PB5), oscylator B, bez przerwania
  DDRB   |= (1 << PB5);
  TCCR1A  = (1 << COM1A1) | (1 << WGM10);
  TCCR1B  = (1 << WGM12)  | (1 << CS10);

  // Timer4 -> D6 (PD7), szum, bez przerwania
  // Rdzeń Arduino ustawia Timer4 po swojemu, więc czyścimy rejestry.
  DDRD   |= (1 << PD7);
  TCCR4A  = 0;
  TCCR4B  = 0;
  TCCR4C  = (1 << COM4D1) | (1 << PWM4D);  // OC4D, nieodwracany PWM
  TCCR4D  = 0;                              // Fast PWM
  TC4H    = 0;
  OCR4C   = 255;                            // TOP = 255 -> 8 bitów
  OCR4D   = 128;
  TCCR4B  = (1 << CS40);                    // preskaler 1
}

// ── Procedura przerwania (62,5 kHz) ───────────────────────────
ISR(TIMER3_OVF_vect) {
  // Oscylator A
  phaseAccumA += phaseIncA;
  OCR3A = sinetable[phaseAccumA >> 8];

  // Oscylator B
  phaseAccumB += phaseIncB;
  OCR1A = sinetable[phaseAccumB >> 8];

  // Szum: xorshift16 (7, 9, 8)
  uint16_t x = noiseState;
  x ^= x << 7;
  x ^= x >> 9;
  x ^= x << 8;
  noiseState = x;
  TC4H  = 0;
  OCR4D = (uint8_t)x;
}

// ── Pomocnicze ────────────────────────────────────────────────
uint16_t freqToInc(float freq) {
  return (uint16_t)(freq * 65536.0f / SAMPLE_RATE);
}

// Pierwszy odczyt po zmianie kanału bywa zafałszowany,
// więc robimy jeden "ślepy" odczyt.
int readPot(uint8_t pin) {
  analogRead(pin);
  return analogRead(pin);
}

float rawToFreq(float smoothed) {
  // Mapowanie wykładnicze: 20 Hz – 20 kHz
  return 20.0f * powf(1000.0f, smoothed / 1023.0f);
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 256; i++) {
    sinetable[i] = (uint8_t)(128.0f + 127.0f * sinf(2.0f * M_PI * i / 256.0f));
  }

  smoothedA = readPot(A0);
  smoothedB = readPot(A1);

  initTimers();
}

void loop() {
  int rawA = readPot(A0);
  int rawB = readPot(A1);

  smoothedA += ALPHA * (rawA - smoothedA);
  smoothedB += ALPHA * (rawB - smoothedB);

  float freqA = rawToFreq(smoothedA);
  float freqB = rawToFreq(smoothedB);

  uint16_t incA = freqToInc(freqA);
  uint16_t incB = freqToInc(freqB);
  uint16_t seed = (uint16_t)micros();

  // Aktualizacja zmiennych współdzielonych z ISR
  cli();
  phaseIncA   = incA;
  phaseIncB   = incB;
  noiseState ^= seed;              // rozbija okres LFSR
  if (noiseState == 0) noiseState = 0xACE1;
  sei();

  Serial.print(rawA);  Serial.print(" ");
  Serial.print(freqA, 1); Serial.print(" | ");
  Serial.print(rawB);  Serial.print(" ");
  Serial.println(freqB, 1);

  delay(5);
}
