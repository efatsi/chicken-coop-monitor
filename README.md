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

## Arduino Cloud + MCP server

`coop_home` publishes the door state to Arduino Cloud as `doorOpen`, `remoteStale` and `minutesSinceHeard`, so agents can check it through the MCP server in `mcp_server/` (Ruby). `mcp_server_py/` is the same server in Python.

1. In Arduino Cloud, add the Nano ESP32 as a manual device and create a Thing with those three read-only variables: `doorOpen` (bool), `remoteStale` (bool) and `minutesSinceHeard` (int), all set to update on change.
2. Copy `coop_home/arduino_secrets.h.example` to `arduino_secrets.h` and fill in the device ID, secret key and 2.4 GHz WiFi details. Then flash `coop_home`.
3. Open the Serial Monitor on `coop_home` and copy the MAC address it prints at boot into `HOME_MAC` in `coop_remote`, then flash `coop_remote`. The remote sends straight to that address and finds `coop_home`'s WiFi channel on its own, so the router's channel doesn't matter.
4. Create an API key in Arduino Cloud, then add the server to your MCP client (for example, Claude Desktop's `claude_desktop_config.json`):

```json
{
  "mcpServers": {
    "chicken-coop": {
      "command": "/path/to/ruby",
      "args": ["/path/to/chicken-coop-monitor/mcp_server/coop_mcp.rb"],
      "env": {
        "ARDUINO_CLIENT_ID": "...",
        "ARDUINO_CLIENT_SECRET": "...",
        "COOP_THING_ID": "..."
      }
    }
  }
}
```

Install the gems with `cd mcp_server && bundle install`. For `command`, use the full path that `ruby -e 'puts RbConfig.ruby'` prints, not a version-manager shim, since MCP clients don't load your shell setup.

To use the Python version instead, set up its venv with `python3 -m venv mcp_server_py/.venv && mcp_server_py/.venv/bin/pip install -r mcp_server_py/requirements.txt`, then point `command` at `mcp_server_py/.venv/bin/python` and `args` at `mcp_server_py/coop_mcp.py`.

Tools: `get_coop_door_status` (open, closed or unknown, plus whether the reading is stale) and `get_coop_door_history(hours)`.

### Hosting on Fly.io

So claude.ai and the phone app can use it, `mcp_server/` also runs as an HTTP server (`config.ru`). It has no auth, so anyone with the URL can read the door state.

```sh
cd mcp_server
fly launch --copy-config --no-deploy   # pick a new app name if chicken-coop-mcp is taken
fly secrets set ARDUINO_CLIENT_ID=... ARDUINO_CLIENT_SECRET=... COOP_THING_ID=...
fly deploy
```

Then in claude.ai, go to Settings → Connectors → Add custom connector, and use `https://<app>.fly.dev/mcp`. The machine sleeps when idle, so the first call after a while takes a second or two longer.

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
