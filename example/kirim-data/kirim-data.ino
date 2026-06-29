#include <WiFi.h>
#include <WebSocketsClient.h>
#include "Qbyte-IoT.h"

WebSocketsClient ws;
QbyteIoT iot(ws);

void setup() {
  WiFi.begin("esp1", "12345678");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  ws.beginSSL("server-iot-qbyte.qbyte.web.id", 443, "/ws");
  ws.setExtraHeaders("Origin: https://server-iot-qbyte.qbyte.web.id");

  iot.begin();
}

void loop() {
  iot.loop();

  static unsigned long t = 0;
  if (millis() - t > 2000) {
    float temp = random(200, 350) / 10.0;
    iot.pub("USR_692e5f5195ddc/sensor/temp", temp);
    t = millis();
  }
}
