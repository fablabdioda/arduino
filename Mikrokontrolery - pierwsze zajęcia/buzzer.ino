#define BUZZER 2

void setup() {
  pinMode(BUZZER, OUTPUT);   // dobrze ustawić jako OUTPUT
}

void loop() {

  beep(BUZZER, 3000, 100);
  beep(BUZZER, 4000, 100);
  beep(BUZZER, 4500, 100);
  beep(BUZZER, 4700, 100);

  // for(int f = 2000; f <= 10000; f += 10) {
  //   tone(BUZZER, f);
  //   delay(10);
  // }
  // noTone(BUZZER);

}

void beep(int pin, int frequency, int duration) {
  tone(pin, frequency);
  delay(duration);
  noTone(pin);
  delay(duration);
}
