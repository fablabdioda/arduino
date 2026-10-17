/*
 * ELEKTRONICZNA POZIOMICA + KONTROLER MIDI – MPU-6050 + dioda RGB
 * ----------------------------------------------------------------
 * Płytka: Pro Micro (ATmega32U4)
 *   W Arduino IDE wybierz "Arduino Leonardo" albo (po dodaniu pakietu SparkFun)
 *   "SparkFun Pro Micro" z właściwą wersją: 5V/16MHz lub 3.3V/8MHz.
 *   Zła wersja = płytka znika z USB (ratunek: 2x szybko zewrzyj RST z GND
 *   i od razu wgraj ponownie).
 *
 * WYMAGANA BIBLIOTEKA: "MIDIUSB" (Arduino) – zainstaluj w Menedżerze bibliotek.
 * Po wgraniu Pro Micro jest jednocześnie portem szeregowym i urządzeniem
 * USB-MIDI (widocznym w DAW jako "Arduino Leonardo" / "Pro Micro").
 *
 * PODŁĄCZENIE
 *   MPU-6050 (GY-521)        Pro Micro
 *     VCC  ----------------- VCC  (wersja 3,3 V: przy problemach użyj RAW)
 *     GND  ----------------- GND
 *     SDA  ----------------- 2    (SDA)
 *     SCL  ----------------- 3    (SCL)
 *     AD0  ----------------- nic / GND  -> adres 0x68
 *
 *   Dioda RGB (wspólna katoda; dla wspólnej anody zmień LED_WSPOLNA_ANODA)
 *     R  --[220 Ω]--------- 9
 *     G  --[220 Ω]--------- 10
 *     B  --[220 Ω]--------- 5
 *     katoda (najdłuższa nóżka) -- GND
 *   PWM na Pro Micro jest tylko na pinach 3, 5, 6, 9, 10 (3 zajmuje I2C).
 *
 *   (opcjonalnie) MIDI OUT na gnieździe DIN-5 – wysyłane równolegle z USB:
 *     pin TX (1) --[220 Ω]-- DIN pin 5
 *     VCC        --[220 Ω]-- DIN pin 4     (dla Pro Micro 3,3 V: 33 Ω i 10 Ω)
 *     GND        ----------- DIN pin 2 (ekran)
 *
 * MIDI: 14-bitowe CC na kanale 1 (MSB na CC n, LSB na CC n+32)
 *   ROLL  -> CC 16 (MSB) + CC 48 (LSB)
 *   PITCH -> CC 17 (MSB) + CC 49 (LSB)
 *   -ZAKRES_MIDI° = 0,  poziom = 8192,  +ZAKRES_MIDI° = 16383
 *
 * KOLORY DIODY – "koło barw" kierunku przechyłu
 *   Barwa (hue)    = KIERUNEK przechyłu (kąt w płaszczyźnie roll/pitch)
 *   Nasycenie      = WIELKOŚĆ przechyłu (0 → biały, KAT_MAX → czysty kolor)
 *     roll+  : czerwony         roll-  : turkusowy (cyan)
 *     pitch+ : żółtozielony     pitch- : fioletowy
 *     ukosy  : kolory pośrednie (pomarańczowy, zielony, błękitny, różowy)
 *   Przeciwne kierunki = kolory dopełniające. Poziom = biały.
 *   miga BIAŁY / zgaszony – czujnik odwrócony "do góry nogami"
 *
 * KOMENDY W MONITORZE PORTU SZEREGOWEGO (115200 baud, wyślij literę + Enter)
 *   z – wyzeruj poziom (bieżąca pozycja staje się "poziomem", także dla MIDI)
 *   c – ponowna kalibracja żyroskopu (czujnik musi leżeć nieruchomo!)
 *   p – przełącz tryb: tabela  <->  Serial Plotter (wykresy kątów)
 */

#include <Wire.h>
#include <math.h>
#include <MIDIUSB.h>

// ---------------- KONFIGURACJA ----------------
const uint8_t MPU_ADDR = 0x68;

// Rodzaj diody:  1 = adresowalna WS2812 / NeoPixel,  0 = zwykła dioda RGB (3x PWM)
#define UZYJ_WS2812 1

#if UZYJ_WS2812
  // WYMAGANA BIBLIOTEKA: "Adafruit NeoPixel" (Menedżer bibliotek)
  // Podłączenie WS2812:  5V -> VCC (lub RAW),  GND -> GND,
  //                      DIN --[330 Ω]-- pin 6
  //   (dla paska/pierścienia: kondensator 470–1000 µF między 5V a GND)
  #include <Adafruit_NeoPixel.h>
  const uint8_t  PIN_WS2812 = 6;   // dowolny pin cyfrowy poza 2 i 3 (I2C)
  const uint8_t  LICZBA_LED = 1;   // pasek/pierścień: wszystkie świecą tak samo
  Adafruit_NeoPixel led(LICZBA_LED, PIN_WS2812, NEO_GRB + NEO_KHZ800);
#else
  // I2C: SDA = 2, SCL = 3 (sprzętowo, nie trzeba ustawiać)
  const uint8_t PIN_R = 9;
  const uint8_t PIN_G = 10;
  const uint8_t PIN_B = 5;
  const bool LED_WSPOLNA_ANODA = false;   // true dla diody ze wspólną anodą
#endif

// Kolory
const float KAT_MAX      = 30.0;  // [°] przy tym przechyle kolor jest w pełni nasycony
const float PROG_POZIOMU = 1.0;   // [°] poniżej – czysty biały ("martwa strefa")
const float JASNOSC      = 0.7;   // 0..1, ogólna jasność diody
const float GAMMA        = 2.2;   // korekcja nieliniowości oka (bez niej kolor
                                  // "nasyca się" już przy kilku stopniach)
// Balans kanałów – zielony w diodach RGB jest zwykle najjaśniejszy.
// Dobierz tak, żeby w poziomie dioda świeciła neutralnie na biało.
const float BALANS_R = 1.0;
const float BALANS_G = 0.75;
const float BALANS_B = 0.9;

// MIDI
const bool    MIDI_USB_WL   = true;    // wysyłaj przez USB-MIDI
const bool    MIDI_DIN_WL   = true;    // wysyłaj przez TX (gniazdo DIN)
const uint8_t MIDI_KANAL    = 0;       // 0 = kanał 1
const uint8_t CC_ROLL       = 16;      // LSB idzie na CC_ROLL + 32  (48)
const uint8_t CC_PITCH      = 17;      // LSB idzie na CC_PITCH + 32 (49)
const float   ZAKRES_MIDI   = 30.0;    // [°] ±ZAKRES_MIDI -> pełny zakres 0..16383
const unsigned long OKRES_MIDI_MS = 20; // max 50 aktualizacji/s

// Filtr i tempo
const float ALFA = 0.98;  // waga żyroskopu w filtrze komplementarnym
const unsigned long OKRES_PETLI_US = 10000;  // 100 Hz
const unsigned long OKRES_DRUKU_MS = 200;    // tabela odświeżana 5x/s

// Skale dla domyślnych zakresów: akcelerometr ±2 g, żyroskop ±250 °/s
const float LSB_NA_G   = 16384.0;
const float LSB_NA_DPS = 131.0;

// ---------------- ZMIENNE ----------------
float ax, ay, az;          // przyspieszenie [g]
float gx, gy, gz;          // prędkość kątowa [°/s]
float temperatura;         // [°C] (temperatura układu, nie otoczenia)

float gxBias = 0, gyBias = 0, gzBias = 0;   // offsety żyroskopu
float rollZero = 0, pitchZero = 0;          // "wyzerowany" poziom

float rollAcc, pitchAcc;   // kąty tylko z akcelerometru (szum, mylony ruchem)
float rollGyro, pitchGyro; // kąty tylko z całkowania żyroskopu (dryft)
float roll, pitch;         // kąty z filtru komplementarnego (najlepsze)

float kierunek = 0;        // [°] 0..360, kierunek przechyłu (= barwa)
float sila = 0;            // 0..1, wielkość przechyłu (= nasycenie)

uint16_t midiR = 8192, midiP = 8192;
uint16_t wyslaneMidiR = 0xFFFF, wyslaneMidiP = 0xFFFF;

bool trybPlotter = false;
unsigned long ostatniaPetla = 0, ostatniDruk = 0, ostatnieMidi = 0;
uint8_t licznikWierszy = 0;

// ---------------- MPU-6050: niskopoziomowo ----------------
void zapiszRejestr(uint8_t reg, uint8_t wartosc) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.write(wartosc);
  Wire.endTransmission();
}

uint8_t czytajRejestr(uint8_t reg) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0xFF;
}

int16_t czytaj16() {
  int16_t hi = Wire.read();
  int16_t lo = Wire.read();
  return (hi << 8) | lo;
}

void inicjujMPU() {
  zapiszRejestr(0x6B, 0x00);  // PWR_MGMT_1: wybudzenie (domyślnie śpi!)
  delay(100);
  zapiszRejestr(0x19, 0x09);  // SMPLRT_DIV: 1 kHz / (1+9) = 100 Hz
  zapiszRejestr(0x1A, 0x03);  // CONFIG: filtr dolnoprzepustowy ~44 Hz
  zapiszRejestr(0x1B, 0x00);  // GYRO_CONFIG:  ±250 °/s
  zapiszRejestr(0x1C, 0x00);  // ACCEL_CONFIG: ±2 g
}

// Diagnostyka: wypisuje adresy wszystkich urządzeń na magistrali I2C
void skanujI2C() {
  Serial.print(F("Skan I2C: "));
  uint8_t ile = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("0x")); Serial.print(a, HEX); Serial.print(F(" "));
      ile++;
    }
  }
  if (ile == 0) Serial.print(F("nic nie znaleziono (okablowanie/zasilanie)"));
  Serial.println();
}

// Jeden odczyt 14 bajtów od rejestru 0x3B: ax ay az temp gx gy gz
void czytajCzujnik() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_ADDR, (uint8_t)14);

  int16_t rax = czytaj16(), ray = czytaj16(), raz = czytaj16();
  int16_t rt  = czytaj16();
  int16_t rgx = czytaj16(), rgy = czytaj16(), rgz = czytaj16();

  ax = rax / LSB_NA_G;
  ay = ray / LSB_NA_G;
  az = raz / LSB_NA_G;
  temperatura = rt / 340.0 + 36.53;
  gx = rgx / LSB_NA_DPS - gxBias;
  gy = rgy / LSB_NA_DPS - gyBias;
  gz = rgz / LSB_NA_DPS - gzBias;
}

// ---------------- DIODA RGB ----------------
void ustawKolor(uint8_t r, uint8_t g, uint8_t b) {
#if UZYJ_WS2812
  for (uint8_t i = 0; i < LICZBA_LED; i++) led.setPixelColor(i, r, g, b);
  led.show();
#else
  if (LED_WSPOLNA_ANODA) { r = 255 - r; g = 255 - g; b = 255 - b; }
  analogWrite(PIN_R, r);
  analogWrite(PIN_G, g);
  analogWrite(PIN_B, b);
#endif
}

// 0..1 (liniowa jasność postrzegana) -> 0..255 PWM z korekcją gamma
uint8_t naPWM(float x, float balans) {
  x = constrain(x, 0.0, 1.0);
  return (uint8_t)(pow(x, GAMMA) * balans * 255.0 + 0.5);
}

// HSV -> RGB;  h w stopniach, s i v w zakresie 0..1
void ustawHSV(float h, float s, float v) {
  h = fmod(h, 360.0);
  if (h < 0) h += 360.0;
  float c = v * s;
  float x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
  float m = v - c;
  float r, g, b;
  if      (h <  60) { r = c; g = x; b = 0; }
  else if (h < 120) { r = x; g = c; b = 0; }
  else if (h < 180) { r = 0; g = c; b = x; }
  else if (h < 240) { r = 0; g = x; b = c; }
  else if (h < 300) { r = x; g = 0; b = c; }
  else              { r = c; g = 0; b = x; }
  ustawKolor(naPWM(r + m, BALANS_R), naPWM(g + m, BALANS_G), naPWM(b + m, BALANS_B));
}

// Kierunek przechyłu -> barwa, wielkość przechyłu -> nasycenie
void obliczKolor(float r, float p) {
  kierunek = atan2(p, r) * 180.0 / PI;   // 0° = roll+, 90° = pitch+, ...
  if (kierunek < 0) kierunek += 360.0;
  float przechyl = sqrt(r * r + p * p);
  float t = (przechyl - PROG_POZIOMU) / (KAT_MAX - PROG_POZIOMU);
  t = constrain(t, 0.0, 1.0);
  // Pierwiastek: małe kąty szybciej "barwią" diodę, duże dochodzą łagodnie
  sila = sqrt(t);
}

void pokazPoziomice() {
  if (az < 0) {                     // do góry nogami -> miga biały
    bool wl = (millis() / 250) % 2;
    ustawHSV(0, 0, wl ? JASNOSC : 0);
    return;
  }
  ustawHSV(kierunek, sila, JASNOSC);
}

// ---------------- MIDI ----------------
void wyslijCC(uint8_t cc, uint8_t wartosc) {
  uint8_t status = 0xB0 | MIDI_KANAL;   // Control Change
  if (MIDI_USB_WL) {
    midiEventPacket_t ev = {0x0B, status, cc, wartosc};
    MidiUSB.sendMIDI(ev);
  }
  if (MIDI_DIN_WL) {
    Serial1.write(status);
    Serial1.write(cc);
    Serial1.write(wartosc);
  }
}

// 14-bitowe CC: najpierw MSB (CC n), potem LSB (CC n+32)
void wyslijCC14(uint8_t cc, uint16_t wartosc) {
  wyslijCC(cc,      (wartosc >> 7) & 0x7F);
  wyslijCC(cc + 32,  wartosc       & 0x7F);
}

uint16_t katNaMidi14(float kat) {
  float t = (kat + ZAKRES_MIDI) / (2.0 * ZAKRES_MIDI);   // -Z..+Z -> 0..1
  t = constrain(t, 0.0, 1.0);
  return (uint16_t)(t * 16383.0 + 0.5);
}

void obsluzMidi(float r, float p) {
  midiR = katNaMidi14(r);
  midiP = katNaMidi14(p);
  if (millis() - ostatnieMidi < OKRES_MIDI_MS) return;
  ostatnieMidi = millis();

  bool wyslano = false;
  if (midiR != wyslaneMidiR) { wyslijCC14(CC_ROLL,  midiR); wyslaneMidiR = midiR; wyslano = true; }
  if (midiP != wyslaneMidiP) { wyslijCC14(CC_PITCH, midiP); wyslaneMidiP = midiP; wyslano = true; }
  if (wyslano && MIDI_USB_WL) MidiUSB.flush();
}

// ---------------- KALIBRACJA ----------------
// Kalibracja: offset żyroskopu + bieżąca pozycja staje się "poziomem" (biały).
// Czujnik musi leżeć nieruchomo.
void kalibrujZyroskop() {
  Serial.println(F("Kalibracja – NIE RUSZAJ czujnika..."));
  ustawKolor(0, 0, 40);  // przygaszony niebieski podczas kalibracji
  gxBias = gyBias = gzBias = 0;
  const int N = 500;
  float sx = 0, sy = 0, sz = 0;
  float sRoll = 0, sPitch = 0;
  for (int i = 0; i < N; i++) {
    czytajCzujnik();
    obliczKatyAkcelerometru();
    sx += gx; sy += gy; sz += gz;
    sRoll += rollAcc; sPitch += pitchAcc;   // uśrednione kąty pozycji wyjściowej
    delay(3);
  }
  gxBias = sx / N; gyBias = sy / N; gzBias = sz / N;
  Serial.print(F("Offsety [deg/s]: gx="));  Serial.print(gxBias, 2);
  Serial.print(F("  gy="));                  Serial.print(gyBias, 2);
  Serial.print(F("  gz="));                  Serial.println(gzBias, 2);

  // Start filtrów od uśrednionej pozycji i ustawienie jej jako zera
  roll  = rollGyro  = rollZero  = sRoll  / N;
  pitch = pitchGyro = pitchZero = sPitch / N;
  Serial.print(F("Poziom odniesienia [deg]: roll="));  Serial.print(rollZero, 1);
  Serial.print(F("  pitch="));                         Serial.println(pitchZero, 1);
  licznikWierszy = 0;
}

// ---------------- OBLICZENIA ----------------
void obliczKatyAkcelerometru() {
  // Kąty z kierunku wektora grawitacji
  rollAcc  = atan2(ay, az) * 180.0 / PI;
  pitchAcc = atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / PI;
}

void aktualizujFiltr(float dt) {
  obliczKatyAkcelerometru();

  // Sam żyroskop: całkowanie prędkości kątowej -> z czasem dryfuje
  rollGyro  += gx * dt;
  pitchGyro += gy * dt;

  // Filtr komplementarny: żyroskop na krótką metę + akcelerometr na długą
  roll  = ALFA * (roll  + gx * dt) + (1.0 - ALFA) * rollAcc;
  pitch = ALFA * (pitch + gy * dt) + (1.0 - ALFA) * pitchAcc;
}

// ---------------- WYDRUK ----------------
void drukujLiczbe(float v, int8_t szerokosc, uint8_t miejsca) {
  char buf[12];
  dtostrf(v, szerokosc, miejsca, buf);
  Serial.print(buf);
}

void drukujCalkowita(long v, uint8_t szerokosc) {
  char buf[12];
  ltoa(v, buf, 10);
  for (uint8_t i = strlen(buf); i < szerokosc; i++) Serial.print(' ');
  Serial.print(buf);
}

void drukujNaglowek() {
  Serial.println();
  Serial.println(F("  ROLL  PITCH |   ax    ay    az [g] |    gx     gy     gz [deg/s] | T[C] | KIER  SILA | MIDI_R MIDI_P | kolor / kierunek"));
  Serial.println(F("--------------+----------------------+-----------------------------+------+------------+---------------+------------------"));
}

// Opis jednego z 8 sektorów koła barw
void drukujKierunek() {
  uint8_t sektor = (uint8_t)((kierunek + 22.5) / 45.0) % 8;
  switch (sektor) {
    case 0: Serial.print(F("czerwony        roll+"));        break;
    case 1: Serial.print(F("pomaranczowy    roll+ pitch+")); break;
    case 2: Serial.print(F("zoltozielony    pitch+"));       break;
    case 3: Serial.print(F("zielony         roll- pitch+")); break;
    case 4: Serial.print(F("turkusowy       roll-"));        break;
    case 5: Serial.print(F("blekitny        roll- pitch-")); break;
    case 6: Serial.print(F("fioletowy       pitch-"));       break;
    case 7: Serial.print(F("rozowy          roll+ pitch-")); break;
  }
}

void drukujTabele(float r, float p) {
  if (licznikWierszy % 20 == 0) drukujNaglowek();
  licznikWierszy++;

  drukujLiczbe(r, 6, 1);           Serial.print(F(" "));
  drukujLiczbe(p, 6, 1);           Serial.print(F(" | "));
  drukujLiczbe(ax, 5, 2);          Serial.print(F(" "));
  drukujLiczbe(ay, 5, 2);          Serial.print(F(" "));
  drukujLiczbe(az, 5, 2);          Serial.print(F("     | "));
  drukujLiczbe(gx, 6, 1);          Serial.print(F(" "));
  drukujLiczbe(gy, 6, 1);          Serial.print(F(" "));
  drukujLiczbe(gz, 6, 1);          Serial.print(F("         | "));
  drukujLiczbe(temperatura, 4, 1); Serial.print(F(" | "));
  drukujCalkowita((long)kierunek, 3);      Serial.print(F("° "));
  drukujCalkowita((long)(sila * 100), 4);  Serial.print(F("% | "));
  drukujCalkowita(midiR, 6);       Serial.print(F(" "));
  drukujCalkowita(midiP, 6);       Serial.print(F(" | "));

  if (az < 0)          Serial.println(F("ODWROCONY"));
  else if (sila == 0)  Serial.println(F("bialy           POZIOM  <==="));
  else               { drukujKierunek(); Serial.println(); }
}

// Format "etykieta:wartość" rozumiany przez Serial Plotter w Arduino IDE.
// Ciekawe porównanie: potrząśnij czujnikiem (skacze 'akc'),
// poczekaj minutę (ucieka 'zyro'), a 'filtr' cały czas jest OK.
void drukujPlotter(float r) {
  Serial.print(F("roll_akc:"));    Serial.print(rollAcc - rollZero, 2);
  Serial.print(F(",roll_zyro:"));  Serial.print(rollGyro - rollZero, 2);
  Serial.print(F(",roll_filtr:")); Serial.println(r, 2);
}

// ---------------- KOMENDY ----------------
void obsluzKomendy() {
  while (Serial.available()) {
    char k = Serial.read();
    if (k == 'z' || k == 'Z') {
      rollZero = roll; pitchZero = pitch;
      rollGyro = roll; pitchGyro = pitch;
      Serial.println(F(">> Wyzerowano poziom w biezacej pozycji."));
      licznikWierszy = 0;
    } else if (k == 'c' || k == 'C') {
      kalibrujZyroskop();
    } else if (k == 'p' || k == 'P') {
      trybPlotter = !trybPlotter;
      if (!trybPlotter) {
        Serial.println(F(">> Tryb tabeli."));
        licznikWierszy = 0;
      }
    }
  }
}

// ---------------- SETUP / LOOP ----------------
void setup() {
  Serial.begin(115200);
  // Pro Micro ma USB w procesorze: czekamy (max 3 s) na otwarcie monitora,
  // żeby nie zgubić komunikatów startowych.
  while (!Serial && millis() < 3000) {}
  if (MIDI_DIN_WL) Serial1.begin(31250);   // standardowa prędkość MIDI

#if UZYJ_WS2812
  led.begin();
  led.show();          // zgaś wszystkie
#else
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
#endif
  // Test diody: CZERWONY -> ZIELONY -> NIEBIESKI (po 0,5 s).
  // Jeśli kolejność jest inna, nóżki diody są zamienione.
  ustawKolor(255, 0, 0); delay(500);
  ustawKolor(0, 255, 0); delay(500);
  ustawKolor(0, 0, 255); delay(500);
  ustawKolor(0, 0, 0);

  Wire.begin();
  Wire.setClock(100000);   // 100 kHz – pewniej na długich przewodach/stykach
#if defined(WIRE_HAS_TIMEOUT)
  Wire.setWireTimeout(3000, true);  // nie zawieszaj się, gdy magistrala "stanie"
#endif

  Serial.println(F("\n=== Elektroniczna poziomica MPU-6050 + MIDI ==="));
  Serial.println(F("Wersja: v7 (WS2812 lub RGB; kalibracja zeruje poziom; kolo barw; 14-bit MIDI)"));
  Serial.print(F("Skompilowano: ")); Serial.print(F(__DATE__));
  Serial.print(F(" ")); Serial.println(F(__TIME__));

  uint8_t who = czytajRejestr(0x75);   // WHO_AM_I, oryginał zwraca 0x68
  Serial.print(F("WHO_AM_I = 0x")); Serial.println(who, HEX);
  if (who == 0xFF || who == 0x00) {
    // Błąd: miga CZERWONY, a w monitorze co sekundę komunikat + skan I2C
    // (żeby był widoczny także po późniejszym otwarciu monitora).
    while (true) {
      Serial.println(F("BLAD: brak MPU-6050 pod 0x68. Sprawdz: SDA->2, SCL->3, VCC, GND."));
      skanujI2C();
      for (uint8_t i = 0; i < 3; i++) {
        ustawKolor(255, 0, 0); delay(150);
        ustawKolor(0, 0, 0);   delay(150);
      }
    }
  }
  if (who != 0x68) {
    Serial.println(F("Uwaga: nietypowy WHO_AM_I (klon?) - probuje dalej."));
  }

  inicjujMPU();
  kalibrujZyroskop();
  Serial.println(F("Komendy: z = zeruj poziom, c = kalibracja zyroskopu, p = tabela/plotter"));

  ostatniaPetla = micros();
}

void loop() {
  obsluzKomendy();

  // Stałe tempo pętli ~100 Hz
  unsigned long teraz = micros();
  if (teraz - ostatniaPetla < OKRES_PETLI_US) return;
  float dt = (teraz - ostatniaPetla) / 1e6;
  ostatniaPetla = teraz;

  czytajCzujnik();
  aktualizujFiltr(dt);

  float r = roll - rollZero;
  float p = pitch - pitchZero;

  obliczKolor(r, p);
  pokazPoziomice();
  obsluzMidi(r, p);

  if (trybPlotter) {
    drukujPlotter(r);
  } else if (millis() - ostatniDruk >= OKRES_DRUKU_MS) {
    ostatniDruk = millis();
    drukujTabele(r, p);
  }
}
