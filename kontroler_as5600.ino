#include <Adafruit_AS5600.h>
#include <MIDIUSB.h>

Adafruit_AS5600 as5600;

const uint8_t LED_PIN = 5;


void setup() {
  Serial.begin(9600);
  
  //while (!Serial) // blokada wyjścia bez seriala - wyłączone do midi
    //delay(10);

  pinMode(LED_PIN, OUTPUT);

  if (!as5600.begin()) {
    Serial.println("Could not find AS5600 sensor, check wiring!");
    while (1)
      delay(10);
  }

  Serial.println("AS5600 found!");

  as5600.enableWatchdog(false);
  // Normal (high) power mode
  as5600.setPowerMode(AS5600_POWER_MODE_NOM);
  // No Hysteresis
  as5600.setHysteresis(AS5600_HYSTERESIS_OFF);

  // analog output
  as5600.setOutputStage(AS5600_OUTPUT_STAGE_ANALOG_FULL);

  // OR can do pwm!
  // as5600.setOutputStage(AS5600_OUTPUT_STAGE_DIGITAL_PWM);
  // as5600.setPWMFreq(AS5600_PWM_FREQ_920HZ);

  // setup filters
  as5600.setSlowFilter(AS5600_SLOW_FILTER_16X);
  as5600.setFastFilterThresh(AS5600_FAST_FILTER_THRESH_SLOW_ONLY);

  // Reset position settings to defaults
  as5600.setZPosition(0);
  as5600.setMPosition(4095);
  as5600.setMaxAngle(4095);

  Serial.println("Waiting for magnet detection...");
}

void loop() {
  if (! as5600.isMagnetDetected()) {
    return;
  }

  // Continuously read and display angle values
  uint16_t rawAngle = as5600.getRawAngle();
  uint16_t angle = as5600.getAngle();

  uint8_t x = angle >> 4;               // 0–255
  uint16_t corrected = x * x;           // gamma ≈ 2
  uint8_t brightness = corrected >> 8;

  //uint8_t brightness = map(angle, 0, 4095, 0, 255);

  analogWrite(LED_PIN, brightness);


  //Serial.println(rawAngle);
  // Serial.print("Raw: ");
  // Serial.print(rawAngle);
  // Serial.print(" (0x");
  // Serial.print(rawAngle, HEX);
  // Serial.print(") | Scaled: ");
  // Serial.print(angle);
  // Serial.print(" (0x");
  // Serial.print(angle, HEX);
  // Serial.print(")");

  int value_scaled = map(rawAngle, 0, 4095, 0, 16383);
  int value_msb = value_scaled >> 7;
  int value_lsb = value_scaled & 127;
  controlChange(0, 20, value_msb);
  controlChange(0, 20 + 32, value_lsb);
  //prev_value[i] = value[i];
  MidiUSB.flush();


  // Check status conditions
  if (as5600.isAGCminGainOverflow()) {
    Serial.print(" | MH: magnet too strong");
  }
  if (as5600.isAGCmaxGainOverflow()) {
    Serial.print(" | ML: magnet too weak");
  }

  //Serial.println();
  //delay(50);
}

void controlChange(byte channel, byte control, byte value) {
  midiEventPacket_t event = { 0x0B, 0xB0 | channel, control, value };
  MidiUSB.sendMIDI(event);
}

void noteOn(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOn = { 0x09, 0x90 | channel, pitch, velocity };
  MidiUSB.sendMIDI(noteOn);
}

void noteOff(byte channel, byte pitch, byte velocity) {
  midiEventPacket_t noteOff = { 0x08, 0x80 | channel, pitch, velocity };
  MidiUSB.sendMIDI(noteOff);
}
