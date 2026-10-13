// Coop Home — runs on the Nano ESP32 inside the house, powered by USB.
//
// Wiring:
//   D3 -> 220 ohm resistor -> red LED (long leg) ; LED short leg -> GND
//   (Optional: the on-board orange LED mirrors D3, so you can test without it.)
//
// Red LED (and on-board orange LED):
//   OFF      door closed, remote checking in normally
//   BLINKING door OPEN (steady 0.5s on / 0.5s off)
//   PATTERN  3 quick blinks then 2s solid — no message from the remote in
//            STALE_AFTER_MS (dead battery, out of range, broken) — go check on it
//
// The on-board RGB LED flashes green each time a message arrives, which is
// handy while positioning the remote to test range.

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

const int RED_LED_PIN = D3;

// Drive the external LED and the on-board LED together.
void setStatusLed(bool on) {
  digitalWrite(RED_LED_PIN, on);
  digitalWrite(LED_BUILTIN, on);
}

const uint8_t WIFI_CHANNEL = 1;          // Must match coop_remote
const uint32_t COOP_MAGIC = 0xC0FFEE42;  // Must match coop_remote

// Remote heartbeats every 5 min; allow 3 missed ones before alarming.
const unsigned long STALE_AFTER_MS = 16UL * 60 * 1000;
const unsigned long BLINK_MS = 500;        // door-open blink half-period
const unsigned long STALE_BLINK_MS = 250;  // stale: 3 blinks x (on+off) = 1.5s
const unsigned long STALE_SOLID_MS = 2000; // stale: then solid on
const unsigned long RX_FLASH_MS = 100;

struct __attribute__((packed)) CoopMsg {
  uint32_t magic;
  uint8_t doorOpen;
  uint32_t bootCount;
};

volatile bool doorOpen = false;
volatile bool gotMessage = false;
volatile unsigned long lastHeardMs = 0;

void handleMessage(const uint8_t *data, int len) {
  if (len != sizeof(CoopMsg)) return;

  CoopMsg msg;
  memcpy(&msg, data, sizeof(msg));
  if (msg.magic != COOP_MAGIC) return;

  doorOpen = msg.doorOpen;
  lastHeardMs = millis();
  gotMessage = true;
}

// The ESP-NOW receive callback signature changed in arduino-esp32 3.x.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onReceive(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  handleMessage(data, len);
}
#else
void onReceive(const uint8_t *mac, const uint8_t *data, int len) {
  handleMessage(data, len);
}
#endif

void setup() {
  Serial.begin(115200);

  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  setStatusLed(false);
  digitalWrite(LED_GREEN, HIGH); // on-board RGB is active LOW

  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed, restarting");
    delay(1000);
    ESP.restart();
  }
  esp_now_register_recv_cb(onReceive);

  // Start the staleness clock at boot so we don't alarm before the
  // remote's first heartbeat has had a chance to arrive.
  lastHeardMs = millis();
  Serial.println("Listening for coop remote...");
}

void loop() {
  unsigned long now = millis();
  static unsigned long rxFlashUntil = 0;

  if (gotMessage) {
    gotMessage = false;
    rxFlashUntil = now + RX_FLASH_MS;
    Serial.printf("Door %s\n", doorOpen ? "OPEN" : "closed");
  }
  digitalWrite(LED_GREEN, (long)(now - rxFlashUntil) < 0 ? LOW : HIGH);

  bool stale = now - lastHeardMs > STALE_AFTER_MS;

  if (stale) {
    unsigned long t = now % (6 * STALE_BLINK_MS + STALE_SOLID_MS);
    if (t < 6 * STALE_BLINK_MS) {
      setStatusLed((t / STALE_BLINK_MS) % 2 == 0);
    } else {
      setStatusLed(true);
    }
  } else if (doorOpen) {
    setStatusLed((now / BLINK_MS) % 2);
  } else {
    setStatusLed(false);
  }

  delay(10);
}
