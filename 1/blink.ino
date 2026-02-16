#define LED 8          // definicje pinów, które będziemy wykorzystywać
#define BUTTON 14

int delayTime = 200;   // zmienna globalna – prędkość migania

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);  // konfigurujemy przycisk, zeby zwarcie z masą działało

  // Serial.begin(9600);
}

void loop() {

  digitalWrite(LED, HIGH); // zapalamy diodę
  delay(delayTime);        // czekamy
  digitalWrite(LED, LOW);  // gasimy diodę
  delay(delayTime);        // czekamy

  // jeśli przycisk wciśnięty (LOW przy INPUT_PULLUP)
  if (digitalRead(BUTTON) == LOW) {
    Serial.print("szybsze ");
    delayTime = 50;   // szybkie miganie
  } else {
    delayTime = 200;   // wolne miganie
  }

    Serial.println("mryg");

}
