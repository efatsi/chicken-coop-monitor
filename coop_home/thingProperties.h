// Arduino IoT Cloud setup, in the shape the Cloud editor generates.
// If you create the Thing in the Cloud editor, its generated file can
// replace this one as long as the variable names match.
//
// Cloud variables (all read-only from the cloud side):
// coop_home only connects after the remote's first message, so these are
// never published with boot defaults.
//   doorOpen          true = open. Only meaningful while remoteStale is false.
//   doorClosed        !doorOpen, but false until the door has been open since
//                     boot, so a reboot doesn't trip a "door closed" alert
//   remoteStale       true = no word from the coop in STALE_AFTER_MS
//   minutesSinceHeard minutes since the coop remote last checked in

#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include "arduino_secrets.h"

const char DEVICE_LOGIN_NAME[] = SECRET_DEVICE_LOGIN_NAME;
const char SSID[] = SECRET_SSID;
const char PASS[] = SECRET_OPTIONAL_PASS;
const char DEVICE_KEY[] = SECRET_DEVICE_KEY;

bool doorOpen;
bool doorClosed;
bool remoteStale;
int minutesSinceHeard;

void initProperties() {
  ArduinoCloud.setBoardId(DEVICE_LOGIN_NAME);
  ArduinoCloud.setSecretDeviceKey(DEVICE_KEY);
  ArduinoCloud.addProperty(doorOpen, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(doorClosed, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(remoteStale, READ, ON_CHANGE, NULL);
  ArduinoCloud.addProperty(minutesSinceHeard, READ, ON_CHANGE, NULL);
}

WiFiConnectionHandler ArduinoIoTPreferredConnection(SSID, PASS);
