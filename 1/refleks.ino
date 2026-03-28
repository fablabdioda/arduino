#define LED 9
#define BUTTON 14

unsigned long delayTime;    // losowy czas oczekiwania na zapalenie LED
unsigned long ledTime;      // czas w któ®ym zapaliła się LED
bool waiting = false;       // czy LED świeci i czekamy na klik

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  Serial.begin(9600);
  
  randomSeed(analogRead(A0));  //generujemy źródło losowości, odczytując szum z niepodłączonego wejścia analogowego

  delayTime = random(2000, 5000);   // generujemy pierwszy losowy czas, z zakresu 2000 do 5000 ms
  Serial.println("Nowa runda...");
  
}

void loop() {

  if (!waiting && millis() > delayTime) {   // jeśli nie czekamy na klik i losowy czas już upłynął
                                            // (funkcja millis() daje milisekundy od startu programu)
    digitalWrite(LED, HIGH);
    ledTime = millis();                     // zapisujemy czas, w którym LED się włączyła
    waiting = true;
  }

  // Kliknięcie przy zapalonej LED
  if (waiting && digitalRead(BUTTON) == LOW) {

    unsigned long reaction = millis() - ledTime;

    digitalWrite(LED, LOW);

    Serial.print("Czas reakcji: ");
    Serial.print(reaction);
    Serial.println(" ms");

    delay(300);  // debounce przycisku

    // startujemy nową rundę
    delayTime = millis() + random(2000, 5000);
    waiting = false;

    Serial.println("Nowa runda...");
  }
}

// spróbuj dodać funkcję wykrywającą falstart
