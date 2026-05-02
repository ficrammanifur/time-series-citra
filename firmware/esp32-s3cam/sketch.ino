#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include "esp_camera.h"
#include <WiFiManager.h>
#include <WebServer.h>

// ================= CONFIG =================
#define BOT_TOKEN "86xxxxxxx:AAHtFExxxxxxxxxxxxxxxxxxxxxx" // your tokenn
#define CHAT_ID   "85xxxxxxxxx" // your id

const char* MQTT_BROKER = "broker.emqx.io";
const int   MQTT_PORT   = 1883;
#define TOPIC_RELAY   "1/2/relay"
#define TOPIC_CAMERA  "1/2/kamera"
#define TOPIC_STATUS  "1/2/status"

#define DISCORD_HOST    "discord.com"
#define DISCORD_WEBHOOK "/api/webhooks/1PuZLVzK58e7Tq6" // link webwook

#define RELAY_PIN 14
#define LED_PIN   2

// Camera pins (FREENOVE ESP32-S3)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

long lastStatusPublish = 0;
WiFiClient espClient;
PubSubClient mqtt(espClient);
WebServer server(80);

bool initCamera() {
  camera_config_t config = {};
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  
  esp_err_t err = esp_camera_init(&config);
  return err == ESP_OK;
}

void sendTelegram(String text) {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://api.telegram.org/bot" + String(BOT_TOKEN) + "/sendMessage";
  http.begin(client, url);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  String body = "chat_id=" + String(CHAT_ID) + "&text=" + text;
  http.POST(body);
  http.end();
}

void sendPhotoDiscord() {
  digitalWrite(LED_PIN, HIGH);
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    sendTelegram("❌ Gagal mengambil foto");
    digitalWrite(LED_PIN, LOW);
    return;
  }
  
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = "https://" + String(DISCORD_HOST) + String(DISCORD_WEBHOOK);
  http.begin(client, url);
  
  String boundary = "ESP32BOUNDARY";
  String head = "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"payload_json\"\r\n"
                "Content-Type: application/json\r\n\r\n"
                "{\"content\":\"📸 Foto dari Jetson Controller\"}\r\n"
                "--" + boundary + "\r\n"
                "Content-Disposition: form-data; name=\"files[0]\"; filename=\"jetson.jpg\"\r\n"
                "Content-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";
  uint32_t totalLen = head.length() + fb->len + tail.length();
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  http.addHeader("Content-Length", String(totalLen));
  String allData = head + String((char*)fb->buf, fb->len) + tail;
  int code = http.POST(allData);
  
  if (code == 200 || code == 204) {
    sendTelegram("✅ Foto berhasil dikirim ke Discord");
  } else {
    sendTelegram("❌ Gagal kirim foto ke Discord");
  }
  http.end();
  esp_camera_fb_return(fb);
  digitalWrite(LED_PIN, LOW);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg = "";
  for(int i=0; i<length; i++) msg += (char)payload[i];
  String topicStr = String(topic);
  
  if (topicStr == TOPIC_RELAY) {
    if (msg == "ON") {
      digitalWrite(RELAY_PIN, LOW);
      sendTelegram("✅ Power ON");
    } else if (msg == "OFF") {
      digitalWrite(RELAY_PIN, HIGH);
      sendTelegram("✅ Power OFF");
    }
  }
  else if (topicStr == TOPIC_CAMERA && msg == "CAPTURE") {
    sendTelegram("📸 Mengambil foto...");
    sendPhotoDiscord();
  }
}

void reconnectMQTT() {
  while (!mqtt.connected()) {
    if (mqtt.connect("ESP32_Jetson")) {
      mqtt.subscribe(TOPIC_RELAY);
      mqtt.subscribe(TOPIC_CAMERA);
      mqtt.publish(TOPIC_STATUS, "ONLINE");
    } else {
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
  
  WiFiManager wm;
  wm.autoConnect("ESP32-Jetson-Control");
  
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  
  initCamera();
  
  server.on("/", []() {
    String html = "<h1>Jetson Controller</h1>";
    html += "<p>Relay: " + String(digitalRead(RELAY_PIN) == LOW ? "ON" : "OFF") + "</p>";
    html += "<a href='/on'><button>Power ON</button></a>";
    html += "<a href='/off'><button>Power OFF</button></a>";
    html += "<a href='/capture'><button>Capture</button></a>";
    server.send(200, "text/html", html);
  });
  server.on("/on", []() { digitalWrite(RELAY_PIN, LOW); server.send(200, "text/plain", "ON"); });
  server.on("/off", []() { digitalWrite(RELAY_PIN, HIGH); server.send(200, "text/plain", "OFF"); });
  server.on("/capture", []() { sendPhotoDiscord(); server.send(200, "text/plain", "Capturing..."); });
  server.begin();
  
  sendTelegram("✅ ESP32 Online");
}

void loop() {
  server.handleClient();
  if (!mqtt.connected()) reconnectMQTT();
  mqtt.loop();
  
  if (millis() - lastStatusPublish > 60000) {
    mqtt.publish(TOPIC_STATUS, "ONLINE");
    lastStatusPublish = millis();
  }
  delay(10);
}
