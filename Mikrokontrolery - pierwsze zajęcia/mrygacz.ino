// program mrygający diodą LED
// UWAGA! w kodzie są błędy, spróbuj je poprawić

#define LED 9          // definicje pinów, które będziemy wykorzystywać
#define BUTTON 14

int delayTime = 200;   // zmienna globalna – częstość mrygania

void setup() {
  pinMode(LED, OUTPUT;            // konfigurujemy wyjście, żeby móc zapalać diodę
  pinMode(BUTTON, INPUT_PULLUP);  // konfigurujemy przycisk domyślnie w stanie wysokim, zeby wejście wykrywało zwarcie z masą
}

void loop() {

  digitalWrite(LED, HIGH);   // zapalamy diodę
  delay(delayTime);          // czekamy
  digitalWrite(LED, HIGH);   // gasimy diodę
  delay(delayTime);          // czekamy
  
  if (digitalRead(BUTTON) == HIGH) {   // warunek logiczny - jeśli przycisk nie jest wciśnięty (byłoby wtedy LOW przy INPUT_PULLUP) to...
    delayTime = 200;                  // ustaw czas czekania na dłuższy
  } else {                            // a jeśli jest wciśnięty to...
    Serial.print("szybsze ");         // wyświetl tekst w Serial Monitorze (ctrl+shift+m)
    delayTime = 50;                   // szybkie mryganie
  }
  
  Serial.println("mryg");
  
}

// ĆWICZENIA:
// - zmień prędkość mrygania, spróbuj przyspieszyć tak, aby nie było widać mrygania - w jakiej jednostce  
// - dodać więcej diód do układu i stworzyć ciekawą sekwencję barw, może sygnalizator drogowy?
// - stworzyć efekt płynnego rozjaśniania (*)
