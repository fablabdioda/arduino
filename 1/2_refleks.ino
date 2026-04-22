// program mierzący czas reakcji użytkownika
// dioda zapala się po losowym czasie, a użytkownik musi jak najszybciej nacisnąć przycisk

#define LED 9
#define BUTTON 14
#define BUZZER 2

unsigned long delayTime;    // czas, po którym LED ma się zapalić (losowy)
unsigned long ledTime;      // moment (czas), w którym LED została zapalona
bool waiting = false;       // flaga – czy czekamy na reakcję użytkownika

void setup() {
  pinMode(LED, OUTPUT);            // ustawiamy pin diody jako wyjście
  pinMode(BUTTON, INPUT_PULLUP);   // ustawiamy pin przycisku jako wejście (z podciąganiem)
  Serial.begin(9600);              // uruchamiamy komunikację z komputerem

  randomSeed(analogRead(A0));      // inicjalizacja generatora liczb losowych (szum z wejścia analogowego)

  delayTime = random(2000, 5000);  // losujemy czas (2–5 sekund) do pierwszego zapalenia LED
  Serial.println("Nowa runda..."); // informacja o rozpoczęciu gry
}

void loop() {

  // zapalenie LED po upływie losowego czasu
  if (!waiting && millis() > delayTime) {
    digitalWrite(LED, HIGH);   // zapalamy diodę
    tone(BUZZER, 1000);
    ledTime = millis();        // zapisujemy moment zapalenia
    waiting = true;            // zaczynamy czekać na reakcję
  }

  // sprawdzenie, czy użytkownik nacisnął przycisk w odpowiednim momencie
  if (waiting && digitalRead(BUTTON) == LOW) {

    unsigned long reaction = millis() - ledTime;  // obliczamy czas reakcji

    digitalWrite(LED, LOW);   // gasimy diodę
    tone(BUZZER, 1500, 100);

    Serial.print("Czas reakcji: ");
    Serial.print(reaction);
    Serial.println(" ms");    // wyświetlamy wynik w milisekundach

    delay(300);               // proste usunięcie drgań styków (debounce)

    // przygotowanie nowej rundy
    delayTime = millis() + random(2000, 5000);  // losujemy nowy czas startu
    waiting = false;                            // wracamy do stanu oczekiwania

    Serial.println("Nowa runda...");
  }
}
