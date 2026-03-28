#define LED 9          // definicje pinów, które będziemy wykorzystywać
#define BUTTON 14

int delayTime = 200;   // zmienna globalna – częstość mrygania

// UWAGA! w kodzie są błędy, spróbuj je poprawić

void setup() {
  pinMode(LED, OUTPUT;            // konfigurujemy wyjście, żeby móc zapalać diodę
  pinMode(BUTTON, INPUT_PULLUP);  // konfigurujemy przycisk, zeby zwarcie z masą działało
}

void loop() {

  digitalWrite(LED, HIGH);   // zapalamy diodę
  delay(delayTime);          // czekamy
  digitalWrite(LED, HIGH);   // gasimy diodę
  delay(delayTime);          // czekamy

  if (digitalRead(BUTTON) == HIGH) {   // jeśli przycisk nie jest wciśnięty (LOW przy INPUT_PULLUP) to...
    delayTime = 200;                  // włącz wolne mryganie
  } else {                            // a jeśli jest wciśnięty to...
    Serial.print("szybsze ");         // wyświetl tekst w Serial Monitorze (ctrl+shift+m)
    delayTime = 50;                   // szybkie mryganie
  }
  
  Serial.println("mryg");
  
}

// spróbuj zmienić czas mrygania, odwrócić działanie przycisku, dodać więcej diod, stworzyć efekt płynnego rozjaśniania(*)
