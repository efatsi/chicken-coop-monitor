// Coop Remote — runs on the Nano ESP32 at the chicken coop.
//
// Wiring:
//   Reed switch (door sensor) between D2 and GND. Nothing else; the
//   internal pull-up is used. Magnet near switch (door closed) => D2 LOW.
//
// Behavior:
//   Wakes up when the door changes state OR every HEARTBEAT_SECONDS,
//   broadcasts the door state over ESP-NOW (no WiFi router needed),
//   then goes back to deep sleep. The home unit treats missing
//   heartbeats as "remote is dead" (battery, damage, out of range).

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_sleep.h>
#include <driver/rtc_io.h>

// D2 on the Nano ESP32 is ESP32-S3 GPIO5 (an RTC pin, so it can wake from deep sleep).
const gpio_num_t REED_GPIO = GPIO_NUM_5;

const uint64_t HEARTBEAT_SECONDS = 5 * 60;
const uint8_t WIFI_CHANNEL = 1;   // Must match coop_home
const int SEND_REPEATS = 3;       // Broadcasts aren't acknowledged, so send a few times
const bool DEBUG_LED = true;      // Blink onboard LED on wake: 1 = closed, 2 = open. Set false once installed.

const uint32_t COOP_MAGIC = 0xC0FFEE42; // Must match coop_home

struct __attribute__((packed)) CoopMsg {
  uint32_t magic;
  uint8_t doorOpen;
  uint32_t bootCount;
};

RTC_DATA_ATTR uint32_t bootCount = 0;

const uint8_t BROADCAST_ADDR[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

bool readDoorOpen() {
  // Pin may still be owned by the RTC domain from the last sleep.
  rtc_gpio_deinit(REED_GPIO);
  gpio_set_direction(REED_GPIO, GPIO_MODE_INPUT);
  gpio_set_pull_mode(REED_GPIO, GPIO_PULLUP_ONLY);
  delay(30); // let a bouncing door settle

  return gpio_get_level(REED_GPIO) == 1; // HIGH = magnet away = open
}

void sendState(bool doorOpen) {
  WiFi.mode(WIFI_STA);
  esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_max_tx_power(84); // max (21 dBm) for range out to the coop

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST_ADDR, 6);
  peer.channel = WIFI_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);

  CoopMsg msg = {COOP_MAGIC, (uint8_t)doorOpen, bootCount};
  for (int i = 0; i < SEND_REPEATS; i++) {
    esp_now_send(BROADCAST_ADDR, (uint8_t *)&msg, sizeof(msg));
    delay(20);
  }

  esp_now_deinit();
  WiFi.mode(WIFI_OFF);
}

void blinkStatus(bool doorOpen) {
  pinMode(LED_BUILTIN, OUTPUT);
  int blinks = doorOpen ? 2 : 1;
  for (int i = 0; i < blinks; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(150);
  }
}

void goToSleep(bool doorOpen) {
  // Wake when the pin moves to the opposite level. ext0 is level-triggered,
  // so if the door already changed while we were awake we wake right back up.
  rtc_gpio_init(REED_GPIO);
  rtc_gpio_set_direction(REED_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(REED_GPIO);
  rtc_gpio_pulldown_dis(REED_GPIO);
  esp_sleep_enable_ext0_wakeup(REED_GPIO, doorOpen ? 0 : 1);

  esp_sleep_enable_timer_wakeup(HEARTBEAT_SECONDS * 1000000ULL);
  esp_deep_sleep_start();
}

void setup() {
  bootCount++;
  Serial.begin(115200);

  bool doorOpen = readDoorOpen();
  Serial.printf("Boot #%lu: door %s\n", (unsigned long)bootCount, doorOpen ? "OPEN" : "closed");

  sendState(doorOpen);
  if (DEBUG_LED) blinkStatus(doorOpen); // after sending, so the message isn't delayed
  goToSleep(doorOpen);
}

void loop() {
  // Never reached; each wake runs setup() and goes back to sleep.
}
