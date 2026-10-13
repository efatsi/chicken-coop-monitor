# Chicken Coop Door Monitor

Two Arduino Nano ESP32s talking over ESP-NOW (direct radio, no WiFi router).

- **coop_remote/**: at the coop. Reed switch on the door. Deep-sleeps and wakes when the door changes, plus every 5 min as a heartbeat.
- **coop_home/**: in the house. Red LED: off = closed, blinking = open, 3 quick blinks + 2s solid = no word from the coop for 16 min.

## Wiring

**Remote:** reed switch between `D2` and `GND`, nothing else (it uses the internal pull-up).
**Home:** `D3` → 220Ω resistor → red LED long leg; short leg → `GND`. Power it from any USB-C phone charger.

## Uploading

Arduino IDE → Boards Manager → install **Arduino ESP32 Boards** → pick **Arduino Nano ESP32**.
Flash `coop_home` first and open the Serial Monitor. Then flash `coop_remote`, open and close the switch, and you should see `Door OPEN` / `Door closed` printed.

## Power notes

The Nano ESP32 board draws around 2 mA in deep sleep. The ESP32 chip itself draws far less, but the rest of the board uses power even while it sleeps. On a 2000 mAh battery that works out to about **3–4 weeks**. Solar keeps it running indefinitely.

## Shopping list

| Item | Where | ~Price |
|---|---|---|
| Magnetic Door Switch Set | [SparkFun](https://www.sparkfun.com/magnetic-door-switch-set.html) | $4 |
| 5mm red LED + 220Ω resistor (home) | SparkFun / DigiKey | $1 |
| Hookup wire, 2-conductor outdoor wire to reach the door | any | $5 |
| Weatherproof project box | hardware store / DigiKey (Hammond 1554 series) | $10–15 |
| **Solar option:** | | |
| Solar Power Manager 5V (DFRobot DFR0559) | [DigiKey](https://www.digikey.com/en/products/detail/dfrobot/DFR0559/9356334) | $8 |
| 5V 0.6W solar panel (Adafruit 5856) | [DigiKey](https://www.digikey.com/en/products/detail/adafruit-industries-llc/5856/22596395) | $11 |
| Lithium Ion Battery 2Ah (JST-PH) | [SparkFun](https://www.sparkfun.com/lithium-ion-battery-2ah.html) | $13 |
| Short USB-A → USB-C cable | any | $5 |

The DFR0559 solar input accepts 4.5–6V only, so don't use "6V" panels. Their open-circuit voltage runs 7–8.5V.
Any regulated 5V USB solar panel can also plug into the DFR0559's USB input instead.

Solar hookup: panel → DFR0559 solar input, LiPo → DFR0559 battery port, DFR0559 5V USB out → Nano USB-C.
**Check the battery connector polarity against the DFR0559 silkscreen before plugging in.** JST-PH polarity is not standardized between vendors.
