#include <WiFi.h>
#include <WebSocketsClient.h>
#include "Qbyte-IoT.h"

#define RELAY_PIN 2

WebSocketsClient ws;
QbyteIoT iot(ws);

void setup() {
  pinMode(RELAY_PIN, OUTPUT);

  WiFi.begin("esp1", "12345678");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  ws.beginSSL("server-iot-qbyte.qbyte.web.id", 443, "/ws");
  ws.setExtraHeaders("Origin: https://server-iot-qbyte.qbyte.web.id");

  iot.begin();
  iot.sub("USR_692e5f5195ddc/de194bbd");
}

void loop() {
  iot.loop();

  digitalWrite(RELAY_PIN,
    iot.getInt("USR_692e5f5195ddc/de194bbd") ? HIGH : LOW
  );
}
