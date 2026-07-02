#include <Arduino.h>

// A0 - wejście częstotliwości, D5 - wyjście PWM
// Wymaga filtru RC (10 kΩ + 10 µF)

// ── Tablica przebiegu (wavetable) ─────────────────────────────
// Generowana podczas uruchamiania programu — nie trzeba ręcznie
// wpisywać wartości.
// 256 próbek, 8-bit bez znaku (0=min, 128=zero, 255=max).
// Przechowywana w RAM — zajmuje 256 bajtów, co jest bez problemu
// dla Pro Micro (ok. 2,5 kB dostępne).
uint8_t sinetable[256];

// ── Stan DDS (Direct Digital Synthesis) ───────────────────────
// 16-bitowy akumulator fazy automatycznie zawija się po 65536.
// Starsze 8 bitów wybiera indeks w tablicy przebiegu.
// phaseIncrement = częstotliwość * 65536 / SAMPLE_RATE
#define SAMPLE_RATE 62500.0f

volatile uint16_t phaseAccum     = 0;
volatile uint16_t phaseIncrement = 0;

// ── Wygładzanie odczytu ADC ───────────────────────────────────
float smoothedRaw = 0;
#define ALPHA 0.2f

// ── Inicjalizacja Timera 3 ────────────────────────────────────
// 8-bitowy Fast PWM, bez preskalera → 16 MHz / 256 = 62,5 kHz
// przepełnienia timera.
// OCR3A jest aktualizowany przy każdym wywołaniu przerwania,
// tworząc przebieg audio.
// Filtr RC na pinie D5 zamienia sygnał PWM na napięcie analogowe.
void initTimer3() {
  DDRC   |= (1 << PC6);
  TCCR3A  = (1 << COM3A1) | (1 << WGM30);
  TCCR3B  = (1 << WGM32)  | (1 << CS30);
  TIMSK3  = (1 << TOIE3);
}

// ── Procedura przerwania (ISR) ────────────────────────────────
// Wywoływana z częstotliwością 62,5 kHz.
ISR(TIMER3_OVF_vect) {
  phaseAccum += phaseIncrement;
  OCR3A = sinetable[phaseAccum >> 8];
}

// ── Sterowanie częstotliwością ────────────────────────────────
void setFreq(float freq) {
  uint16_t inc = (uint16_t)(freq * 65536.0f / SAMPLE_RATE);

  // Aktualizacja zmiennej współdzielonej z ISR
  // przy wyłączonych przerwaniach.
  cli();
  phaseIncrement = inc;
  sei();
}

void setup() {
  Serial.begin(115200);

  // Wygenerowanie tablicy sinusoidy
  for (int i = 0; i < 256; i++) {
    sinetable[i] = (uint8_t)(128.0f + 127.0f * sinf(2.0f * M_PI * i / 256.0f));
  }

  // Pierwszy odczyt ADC jako wartość początkowa filtru
  smoothedRaw = analogRead(A0);

  // Uruchomienie Timera 3
  initTimer3();
}

void loop() {
  // Odczyt potencjometru / napięcia na A0
  int raw = analogRead(A0);

  // Wygładzanie wykładnicze (filtr IIR)
  smoothedRaw += ALPHA * (raw - smoothedRaw);

  // Mapowanie wykładnicze: 20 Hz – 20 kHz
  float freq = 20.0f * powf(1000.0f, smoothedRaw / 1023.0f);

  // Ustawienie nowej częstotliwości DDS
  setFreq(freq);

  // Dane diagnostyczne przez port szeregowy
  Serial.print(raw);
  Serial.print(" ");
  Serial.print(smoothedRaw);
  Serial.print(" ");
  Serial.println(freq, 1);

  delay(5);
}
