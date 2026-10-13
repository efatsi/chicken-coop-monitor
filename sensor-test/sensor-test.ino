// Sensor Test — bench check for the reed switch on a Nano ESP32.
//
// Wiring:
//   Same as coop_remote: reed switch between D2 and GND, internal pull-up.
//   Magnet near switch (door closed) => D2 LOW.
//
// Behavior:
//   Onboard LED follows the switch: on = magnet away (open), off = magnet
//   near (closed). Each change is also printed to Serial.

const int REED_PIN = D2;

int lastLevel = -1;

void setup() {
  Serial.begin(115200);
  pinMode(REED_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  int level = digitalRead(REED_PIN); // HIGH = magnet away = open
  digitalWrite(LED_BUILTIN, level);

  if (level != lastLevel) {
    Serial.printf("Door %s\n", level == HIGH ? "OPEN" : "closed");
    lastLevel = level;
  }

  delay(30); // crude debounce
}
