const int ledPin = 9;      // PWM pin connected to LED
const int potPin = A0;     // Analog pin connected to potentiometer

void setup() {
  pinMode(ledPin, OUTPUT);
}

void loop() {
  int potValue = analogRead(potPin);           // read potentiometer (0-1023)
  int brightness = map(potValue, 0, 1023, 0, 255); // map to PWM range
  analogWrite(ledPin, brightness);            // set LED brightness
}
