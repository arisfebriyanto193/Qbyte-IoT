#include <WiFi.h>
#include <WebSocketsClient.h>
#include "QbyteIoT.h"

WebSocketsClient ws;
QbyteIoT iot(ws);

void setup() {
  WiFi.begin("SSID", "PASSWORD");
  while (WiFi.status() != WL_CONNECTED) delay(500);

  ws.beginSSL("server-iot-qbyte.qbyte.web.id", 443, "/ws");
  ws.setExtraHeaders("Origin: https://server-iot-qbyte.qbyte.web.id");

  iot.begin();
  iot.sub("user/relay");
}

void loop() {
  iot.loop();

  int relay = iot.getInt("user/relay");
  Serial.println(relay);
}
