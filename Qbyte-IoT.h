#ifndef QBYTE_IOT_H
#define QBYTE_IOT_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <map>
#include <vector>

class QbyteIoT {
public:
  QbyteIoT(WebSocketsClient& ws);

  void begin();
  void loop();

  void sub(const String& topic);
  void pub(const String& topic, int payload);
  void pub(const String& topic, const String& payload);

  bool has(const String& topic);
  int getInt(const String& topic);
  String get(const String& topic);

private:
  WebSocketsClient& _ws;

  std::vector<String> _subs;
  std::map<String, String> _data;

  static QbyteIoT* _instance;
  static void _wsEvent(WStype_t type, uint8_t* payload, size_t length);

  void _handleConnected();
  void _handleMessage(const String& msg);
};

#endif
