#include "esp_camera.h"
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <base64.h>
#include <mbedtls/base64.h> /
// ===================
// Select camera model
// ===================
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE  // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_CAMS3_UNIT  // Has PSRAM
#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
//#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD
//#define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3 // Has PSRAM
//#define CAMERA_MODEL_DFRobot_Romeo_ESP32S3 // Has PSRAM
#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "Tenda";
const char *password = "wifiserver";

const char* websocket_host = "server-mqtt-js.qbyte.web.id";
const int websocket_port = 443;
const char* websocket_path = "/ws"; 

const char* cameraTopic = "camera/stream";
const char* controlTopic = "camera/control";

bool isWsStreaming = false;
unsigned long lastKeepAlive = 0;
const unsigned long STREAM_TIMEOUT = 10000; // 10 detik tanpa instruksi, otomatis mati

WebSocketsClient webSocket;

void setupLedFlash(int pin);

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      Serial.println("✅ WebSocket connected");
      // Subscribe ke topic control agar ESP bisa menerima perintah start/stop
      webSocket.sendTXT(String("{\"action\":\"subscribe\", \"topic\":\"") + controlTopic + "\"}");
      break;
    case WStype_DISCONNECTED:
      Serial.println("❌ WebSocket disconnected");
      break;
    case WStype_TEXT: {
      String msg = String((char*)payload);
      if (msg.indexOf(controlTopic) != -1) {
        if (msg.indexOf("\"start\"") != -1) {
          lastKeepAlive = millis();
          if (!isWsStreaming) {
            isWsStreaming = true;
            Serial.println("▶️ Perintah STREAMING DIMULAI");
          }
        } else if (msg.indexOf("\"stop\"") != -1) {
          isWsStreaming = false;
          Serial.println("⏹️ Perintah STREAMING BERHENTI");
        }
      }
      break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  // Diubah ke SVGA/VGA agar Base64 String tidak melebihi alokasi memori
  config.frame_size = FRAMESIZE_VGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12; // Quality: 10-15 biasanya seimbang. 
  config.fb_count = 1;

  // Jika PSRAM ada, frame_buff diganti. Batasi maksimum VGA.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      // Nilai makin KECIL = Gambarnya makin BAGUS dan jernih, tapi FPS bisa turun sedikit
      config.jpeg_quality = 8; // Sebelumnya 10, kita turunkan ke 8 untuk gambar lebih bagus
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
      // Jangan pakai UXGA, agar Payload di WebSocket bisa lewat dan tidak hang.
      config.frame_size = FRAMESIZE_VGA; 
    } else {
      config.frame_size = FRAMESIZE_QVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // Tetap VGA
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_VGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Init Websocket connection
  webSocket.beginSSL(websocket_host, websocket_port, websocket_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);

  Serial.println("🚀 Mulai terhubung ke Websocket Server...");
}

unsigned long lastFrameTime = 0;

// Siapkan buffer Base64 di PSRAM (jika ada) agar memori RAM utamanya tidak habis yang bisa bikin macet
unsigned char * base64Buffer = NULL;
size_t maxBase64Size = 0;

void loop() {
  webSocket.loop();

  // Matikan stream otomatis jika lebih dari 10 detik tidak ada "start" (keep-alive) baru
  if (isWsStreaming && (millis() - lastKeepAlive > STREAM_TIMEOUT)) {
      isWsStreaming = false;
      Serial.println("⏱️ Stream otomatis berhenti (Tidak ada penonton aktif di web)");
  }

  // Kirim gambar jika waktu tercapai DAN status isWsStreaming AKTIF
  if (isWsStreaming && (millis() - lastFrameTime > 60) && webSocket.isConnected()) {
    lastFrameTime = millis();

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("🚨 Camera capture failed");
      return;
    }

    // Hitung ukuran Base64 yang akan dibutuhkan
    // Base64 memakan 4/3 lebih besar dari ukuran asli bytes-nya. 
    size_t out_len = 0;
    size_t required_size = ((fb->len + 2) / 3) * 4 + 1; // +1 untuk Null terminator
    
    // Alokasi memori buffer secara dinamis untuk pertama kalinya
    if (maxBase64Size < required_size) {
        if(base64Buffer != NULL) free(base64Buffer);
        maxBase64Size = required_size + 1024; // Kasih cadangan ekstra 1KB
        
        if (psramFound()) {
            base64Buffer = (unsigned char *)ps_malloc(maxBase64Size); // Taruh di PSRAM yg lega
        } else {
            base64Buffer = (unsigned char *)malloc(maxBase64Size);
        }
    }

    if (base64Buffer != NULL) {
         // Lakukan proses konversi Base64 lebih dalam menggunakan mbedtls ESP
         mbedtls_base64_encode(base64Buffer, maxBase64Size, &out_len, fb->buf, fb->len);
         base64Buffer[out_len] = '\0'; // Jangan lupa akhiri dengan byte kosong 
         
         // Format payload
         String payload = String(cameraTopic) + "|" + (char*)base64Buffer;
         webSocket.sendTXT(payload);
    } else {
         Serial.println("🚨 Memori penuh, tidak bisa mengalokasi Base64 buffer");
         maxBase64Size = 0; // Reset agar dia mencoba alokasi lagi di frame berikutnya
    }

    // Kembalikan framebuffer setelah beres
    esp_camera_fb_return(fb);
  }
}

