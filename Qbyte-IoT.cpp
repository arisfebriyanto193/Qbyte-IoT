#include "Qbyte-IoT.h"

QbyteIoT* QbyteIoT::_instance = nullptr;

QbyteIoT::QbyteIoT(WebSocketsClient& ws) : _ws(ws) {
  _instance = this;
}

// =====================
void QbyteIoT::begin() {
  _ws.onEvent(_wsEvent);
}

// =====================
void QbyteIoT::loop() {
  _ws.loop();
}

// =====================
void QbyteIoT::setOnlineTopic(const String& topic) {
  _onlineTopic = topic;
}

// =====================
void QbyteIoT::sub(const String& topic) {
  _subs.push_back(topic);

  // Otomatis jadikan topik subscribe pertama sebagai identitas online jika belum diset
  if (_onlineTopic.length() == 0) {
    _onlineTopic = topic;
  }

  // jika sudah connected → langsung kirim
  if (_ws.isConnected()) {
    String msg = "{\"action\":\"subscribe\",\"topic\":\"" + topic + "\"}";
    _ws.sendTXT(msg);
  }
}

// =====================
void QbyteIoT::pub(const String& topic, int payload) {
  if (_onlineTopic.length() == 0) {
    _onlineTopic = topic;
  }
  _ws.sendTXT(topic + "|" + String(payload));
}

void QbyteIoT::pub(const String& topic, const String& payload) {
  if (_onlineTopic.length() == 0) {
    _onlineTopic = topic;
  }
  _ws.sendTXT(topic + "|" + payload);
}

// =====================
bool QbyteIoT::has(const String& topic) {
  return _data.count(topic);
}

int QbyteIoT::getInt(const String& topic) {
  return has(topic) ? _data[topic].toInt() : 0;
}

String QbyteIoT::get(const String& topic) {
  return has(topic) ? _data[topic] : "";
}

// =====================
// STATIC EVENT
// =====================
void QbyteIoT::_wsEvent(WStype_t type, uint8_t* payload, size_t length) {
  if (!_instance) return;

  switch (type) {
    case WStype_CONNECTED:
      _instance->_handleConnected();
      break;

    case WStype_TEXT:
      _instance->_handleMessage(String((char*)payload));
      break;

    default:
      break;
  }
}

// =====================
void QbyteIoT::_handleConnected() {
  // 1. KIRIM IDENTITAS ONLINE (opsional tapi disarankan untuk SENSOR yg jarang nge-publish)
  if (_onlineTopic.length() > 0) {
    String msg = "{\"action\":\"publish\",\"topic\":\"" + _onlineTopic + "/status\",\"payload\":\"ON\"}";
    _ws.sendTXT(msg);
  }

  // 2. KIRIM ULANG SEMUA SUBSCRIBE
  for (auto& t : _subs) {
    String msg = "{\"action\":\"subscribe\",\"topic\":\"" + t + "\"}";
    _ws.sendTXT(msg);
  }
}

// =====================
void QbyteIoT::_handleMessage(const String& msg) {
  int t1 = msg.indexOf("\"topic\":\"");
  if (t1 == -1) return;
  int t2 = msg.indexOf("\"", t1 + 9);
  if (t2 == -1) return;

  String topic = msg.substring(t1 + 9, t2);

  int p1 = msg.indexOf("\"payload\":");
  if (p1 == -1) return;

  int valStart = p1 + 10;
  while(valStart < msg.length() && (msg[valStart] == ' ' || msg[valStart] == ':')) {
      valStart++;
  }
  
  String payload = "";
  if (msg[valStart] == '"') {
      int valEnd = msg.indexOf("\"", valStart + 1);
      if (valEnd != -1) {
          payload = msg.substring(valStart + 1, valEnd);
      }
  } else {
      int valEnd = valStart;
      while(valEnd < msg.length() && msg[valEnd] != ',' && msg[valEnd] != '}') {
          valEnd++;
      }
      payload = msg.substring(valStart, valEnd);
      payload.replace("\"", "");
  }

  payload.trim();
  _data[topic] = payload;
}
