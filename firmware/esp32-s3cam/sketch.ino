#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include "esp_camera.h"

// ================= CONFIG =================
#define BOT_TOKEN "8638405630:AAHjMsDuNGAVY0JSqh0qkhZnOnutfUCC7fM"
#define CHAT_ID   "8508914803"

#define RELAY_PIN 12
#define LED_PIN   2

// ================= CAMERA PIN - FREENOVE ESP32-S3 =================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM      4
#define SIOC_GPIO_NUM      5
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM        8
#define Y3_GPIO_NUM        9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM     6
#define HREF_GPIO_NUM      7
#define PCLK_GPIO_NUM     13

// ================= GLOBALS =================
QueueHandle_t camQueue;
long lastUpdateId = 0;

// ================= TELEGRAM SEND MESSAGE =================
void sendTelegramMessage(const String& text) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure* sslClient = new WiFiClientSecure();
  sslClient->setInsecure();

  HTTPClient http;
  http.begin(*sslClient, "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.setTimeout(10000);

  String body = "chat_id=" + String(CHAT_ID) + "&text=" + text;
  int code = http.POST(body);
  Serial.println("[TG] sendMessage code: " + String(code));

  http.end();
  delete sslClient;
}

// ================= TELEGRAM SEND PHOTO =================
void sendPhoto() {
  Serial.println("[CAM] Ambil foto...");

  // ✅ Flush buffer lama
  camera_fb_t* flush = esp_camera_fb_get();
  if (flush) {
    esp_camera_fb_return(flush);
    delay(150);
  }

  // ✅ Ambil frame fresh
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("[CAM] Capture GAGAL");
    sendTelegramMessage("Gagal ambil foto");
    return;
  }

  Serial.printf("[CAM] Ukuran: %d bytes (%dx%d)\n", fb->len, fb->width, fb->height);

  WiFiClientSecure* sslClient = new WiFiClientSecure();
  sslClient->setInsecure();
  sslClient->setTimeout(20);

  if (!sslClient->connect("api.telegram.org", 443)) {
    Serial.println("[TG] sendPhoto connect GAGAL");
    esp_camera_fb_return(fb);
    delete sslClient;
    return;
  }

  String boundary = "ESPBND";
  String head = "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
                String(CHAT_ID) + "\r\n"
                "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"photo\"; filename=\"cam.jpg\"\r\n"
                "Content-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t totalLen = head.length() + fb->len + tail.length();

  sslClient->print("POST /bot" + String(BOT_TOKEN) + "/sendPhoto HTTP/1.1\r\n");
  sslClient->print("Host: api.telegram.org\r\n");
  sslClient->print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
  sslClient->print("Content-Length: " + String(totalLen) + "\r\n");
  sslClient->print("Connection: close\r\n\r\n");
  sslClient->print(head);

  // Kirim foto per chunk
  const size_t CHUNK = 2048;
  size_t sent = 0;
  while (sent < fb->len) {
    size_t toSend = min(CHUNK, fb->len - sent);
    sslClient->write(fb->buf + sent, toSend);
    sent += toSend;
  }
  sslClient->print(tail);

  // Baca response
  long start = millis();
  String resp = "";
  while (sslClient->connected() && millis() - start < 15000) {
    while (sslClient->available()) {
      resp += (char)sslClient->read();
    }
    delay(5);
  }

  if (resp.indexOf("\"ok\":true") != -1) {
    Serial.println("[TG] Foto terkirim!");
  } else {
    Serial.println("[TG] Foto gagal. Resp: " + resp.substring(0, 200));
  }

  esp_camera_fb_return(fb);
  sslClient->stop();
  delete sslClient;
}

// ================= CHECK TELEGRAM =================
void checkTelegram() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure* sslClient = new WiFiClientSecure();
  sslClient->setInsecure();

  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) +
               "/getUpdates?offset=" + String(lastUpdateId + 1) +
               "&timeout=3&limit=5";

  http.begin(*sslClient, url);
  http.setTimeout(10000);

  int code = http.GET();

  if (code == 200) {
    String response = http.getString();

    // Parse update_id
    int idIdx = response.indexOf("\"update_id\":");
    if (idIdx != -1) {
      long newId = response.substring(idIdx + 12, idIdx + 24).toInt();
      if (newId > lastUpdateId) {
        lastUpdateId = newId;
        Serial.println("[TG] update_id: " + String(lastUpdateId));
      }
    }

    if (response.indexOf("/foto") != -1) {
      Serial.println("[TG] /foto diterima");
      int cmd = 1;
      xQueueSend(camQueue, &cmd, 0);
    }

    if (response.indexOf("/relay_on") != -1) {
      Serial.println("[TG] /relay_on");
      digitalWrite(RELAY_PIN, LOW);
      sendTelegramMessage("Relay ON");
    }

    if (response.indexOf("/relay_off") != -1) {
      Serial.println("[TG] /relay_off");
      digitalWrite(RELAY_PIN, HIGH);
      sendTelegramMessage("Relay OFF");
    }

    if (response.indexOf("/status") != -1) {
      String msg = "Status ESP32-S3\n";
      msg += "IP: " + WiFi.localIP().toString() + "\n";
      msg += "RSSI: " + String(WiFi.RSSI()) + " dBm\n";
      msg += "Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
      msg += "PSRAM: " + String(ESP.getFreePsram()) + " bytes\n";
      msg += "Uptime: " + String(millis() / 1000) + " detik";
      sendTelegramMessage(msg);
    }

  } else {
    Serial.println("[TG] getUpdates code: " + String(code));
  }

  http.end();
  delete sslClient;
}

// ================= INIT CAMERA =================
bool initCamera() {
  camera_config_t config;

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // ✅ GRAB_WHEN_EMPTY = capture hanya saat diminta, cegah FB-OVF
  config.frame_size   = FRAMESIZE_SVGA;   // 800x600, bagus dan ringan
  config.jpeg_quality = 10;
  config.fb_count     = 1;                // ✅ 1 buffer saja
  config.fb_location  = CAMERA_FB_IN_PSRAM;
  config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY; // ✅ bukan GRAB_LATEST

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Init GAGAL: 0x%x\n", err);
    return false;
  }

  // Sensor tuning
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_awb_gain(s, 1);
    s->set_exposure_ctrl(s, 1);
    s->set_aec2(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_lenc(s, 1);
    s->set_hmirror(s, 0);
    s->set_vflip(s, 0);
  }

  Serial.println("[CAM] Init OK");
  return true;
}

// ================= TASKS =================
void telegramTask(void* pv) {
  vTaskDelay(2000 / portTICK_PERIOD_MS);

  sendTelegramMessage("ESP32-S3 ONLINE - IP: " + WiFi.localIP().toString());
  vTaskDelay(500 / portTICK_PERIOD_MS);
  sendTelegramMessage("Siap! Ketik: /foto /relay_on /relay_off /status");

  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      checkTelegram();
    } else {
      Serial.println("[WIFI] Putus, reconnect...");
      WiFi.reconnect();
      vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
  }
}

void cameraTask(void* pv) {
  // ✅ Flush semua frame lama saat boot
  camera_fb_t* f;
  int flushed = 0;
  while ((f = esp_camera_fb_get()) != NULL) {
    esp_camera_fb_return(f);
    flushed++;
    if (flushed > 5) break; // safety
  }
  Serial.println("[CAM] Flush " + String(flushed) + " frame lama");

  int cmd;
  while (true) {
    if (xQueueReceive(camQueue, &cmd, portMAX_DELAY)) {
      digitalWrite(LED_PIN, HIGH);
      sendPhoto();
      digitalWrite(LED_PIN, LOW);
    }
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========== ESP32-S3 CAM BOT ==========");

  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);

  camQueue = xQueueCreate(5, sizeof(int));

  Serial.printf("[MEM] Heap: %d | PSRAM: %d\n",
                ESP.getFreeHeap(), ESP.getPsramSize());

  WiFiManager wm;
  wm.setConnectTimeout(20);
  wm.autoConnect("ESP32S3-CAM-Setup");

  Serial.println("[WIFI] Connected: " + WiFi.localIP().toString());
  WiFi.setSleep(false);
  digitalWrite(LED_PIN, HIGH);

  bool camOK = initCamera();
  if (!camOK) {
    Serial.println("[CAM] GAGAL - cek hardware & pin");
  }

  // ✅ Stack 20480 untuk SSL di telegramTask
  xTaskCreatePinnedToCore(telegramTask, "telegram", 20480, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(cameraTask,   "camera",   16384, NULL, 1, NULL, 0);

  Serial.println("[SETUP] Done.");
}

// ================= LOOP =================
void loop() {
  vTaskDelay(1000 / portTICK_PERIOD_MS);
}
