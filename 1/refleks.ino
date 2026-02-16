const int LED = 8;
const int BUTTON = 14;

unsigned long ledTime;      // moment zapalenia LED
unsigned long delayTime;    // losowy czas oczekiwania
bool waiting = false;       // czy LED świeci i czekamy na klik

void setup() {
  pinMode(LED, OUTPUT);
  pinMode(BUTTON, INPUT_PULLUP);
  Serial.begin(9600);
  
  randomSeed(analogRead(A0));  //generujemy źródło losowości

  delayTime = random(2000, 5000);   // pierwszy losowy czas
  Serial.println("Nowa runda...");
}

void loop() {

  // Zapalenie LED po losowym czasie
  if (!waiting && millis() > delayTime) {
    digitalWrite(LED, HIGH);
    ledTime = millis();
    waiting = true;
  }

  // Kliknięcie przy zapalonej LED
  if (waiting && digitalRead(BUTTON) == LOW) {

    unsigned long reaction = millis() - ledTime;

    digitalWrite(LED, LOW);

    Serial.print("Czas reakcji: ");
    Serial.print(reaction);
    Serial.println(" ms");

    delay(300);  // debounce

    // Nowa runda
    delayTime = millis() + random(2000, 5000);
    waiting = false;

    Serial.println("Nowa runda...");
  }
}
