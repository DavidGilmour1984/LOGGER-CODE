/**** ESP32-CAM (AI-Thinker) – Triggered Photo Uploader with 30s Cooldown + WiFi Config AP ****/
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "time.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "FS.h"
#include "LittleFS.h"
#include <Preferences.h>
#include <WebServer.h>

WebServer server(80);
unsigned long accessPointStartTime = 0;
Preferences preferences;

// ==== USER SETTINGS ====
String apiKey = "8841216e260eef9d5883dd8f6da93ffd";   // Neocities API key
const char* remoteDirectory = "photos";               // remote folder
const char* jsonPath  = "/photos.json";               // local index (mirrored remotely)
const char* ntpServer  = "pool.ntp.org";
const long  gmtOffsetSeconds = 12 * 3600;             // NZST offset
const int   daylightOffsetSeconds = 0;

// Pins
#define FLASH_LED_PIN      4          // onboard flash LED (ESP32-CAM)
#define TRIGGER_PIN        13         // D13 → wake/capture on HIGH
#define COOLDOWN_MICROSECONDS        (30ULL * 1000000ULL)  // 30 seconds

// ==== CAMERA PINOUT (AI-Thinker) ====
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ==== Helpers: Filesystem ====
bool initializeFileSystem() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return false;
  }
  return true;
}

String loadJsonFile() {
  if (!LittleFS.exists(jsonPath)) return "[]";
  File file = LittleFS.open(jsonPath, "r");
  if (!file) return "[]";
  String content = file.readString();
  file.close();
  return (content.length() ? content : "[]");
}

void saveJsonFile(const String& json) {
  File file = LittleFS.open(jsonPath, "w");
  if (!file) return;
  file.print(json);
  file.close();
}

// ==== WiFi Config Portal Helpers ====
bool loadWiFiCredentials(String &ssid, String &password) {
  preferences.begin("wifi", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("pass", "");
  preferences.end();
  return (ssid.length() > 0);
}

void handleRootPage() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>";
  html += "<style>";
  html += "body { font-family: Helvetica, Arial, sans-serif; background:#f5f7fb; color:#0f172a; text-align:center; padding:50px; }";
  html += "h2 { font-size:40px; margin-bottom:30px; }";
  html += "form { background:#ffffff; display:inline-block; padding:30px; border-radius:20px; box-shadow:0 10px 25px rgba(0,0,0,0.1); }";
  html += "input { font-size:30px; margin:15px 0; padding:10px; border-radius:10px; border:1px solid #cbd5e1; width:80%; }";
  html += "input[type=submit] { background:#2563eb; color:white; border:none; width:85%; cursor:pointer; transition:0.3s; }";
  html += "input[type=submit]:hover { background:#1e40af; }";
  html += "a { display:block; margin-top:25px; font-size:28px; color:#2563eb; text-decoration:none; }";
  html += "a:hover { text-decoration:underline; }";
  html += "</style></head><body>";
  html += "<h2>WiFi Setup</h2>";
  html += "<form action='/save' method='POST'>";
  html += "<input type='text' name='ssid' placeholder='Enter WiFi SSID'><br>";
  html += "<input type='password' name='pass' placeholder='Enter WiFi Password'><br>";
  html += "<input type='submit' value='Save Credentials'>";
  html += "</form>";
  html += "<a href='/status'>View Saved Credentials</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleSavePage() {
  if (server.hasArg("ssid") && server.hasArg("pass")) {
    String ssid = server.arg("ssid");
    String password = server.arg("pass");
    preferences.begin("wifi", false);
    preferences.putString("ssid", ssid);
    preferences.putString("pass", password);
    preferences.end();
    server.send(200, "text/html", "<html><body style='font-family:Helvetica; font-size:30px;'>Saved! Rebooting...</body></html>");
    delay(1000);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Missing SSID or password");
  }
}

void handleStatusPage() {
  String ssid, password;
  loadWiFiCredentials(ssid, password);
  String html = "<html><head><style>";
  html += "body { font-family: Helvetica; font-size:30px; background:#f5f7fb; text-align:center; padding:50px; }";
  html += "div { background:#ffffff; display:inline-block; padding:30px; border-radius:20px; box-shadow:0 10px 25px rgba(0,0,0,0.1); }";
  html += "a { display:block; margin-top:25px; font-size:28px; color:#2563eb; text-decoration:none; }";
  html += "</style></head><body><div>";
  html += "<h2>Saved WiFi Credentials</h2>";
  if (ssid.length() > 0) {
    html += "SSID: " + ssid + "<br>";
    html += "Password: " + password + "<br>";
  } else {
    html += "No credentials saved.<br>";
  }
  html += "<a href='/'>Back</a></div></body></html>";
  server.send(200, "text/html", html);
}

void startAccessPointForConfiguration() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32 Access Point");
  Serial.println("Started AP: ESP32 Access Point");

  server.on("/", handleRootPage);
  server.on("/save", HTTP_POST, handleSavePage);
  server.on("/status", handleStatusPage);
  server.begin();
  accessPointStartTime = millis();
}

// ==== Helpers: Camera / Upload ====
String generateFilenameTimestamp() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buffer[32];
    strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &timeinfo);
    return String(buffer);
  }
  return String(millis());
}

bool uploadMultipartFile(const String& remotePath, const uint8_t* data, size_t length, const char* mimeType) {
  String boundary = "----ESP32Boundary" + String((uint32_t)millis());
  String head =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"" + remotePath + "\"; filename=\"" +
    remotePath.substring(remotePath.lastIndexOf('/')+1) + "\"\r\n"
    "Content-Type: " + String(mimeType) + "\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  size_t totalLength = head.length() + length + tail.length();
  uint8_t* body = (uint8_t*)malloc(totalLength);
  if (!body) return false;
  memcpy(body, head.c_str(), head.length());
  memcpy(body + head.length(), data, length);
  memcpy(body + head.length() + length, tail.c_str(), tail.length());

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, "https://neocities.org/api/upload")) { free(body); return false; }
  http.addHeader("Authorization", String("Bearer ") + apiKey);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  int code = http.POST(body, totalLength);
  free(body);
  http.end();
  Serial.printf("Upload %s → HTTP %d\n", remotePath.c_str(), code);
  return (code >= 200 && code < 300);
}

bool initializeCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count     = 1;
  return (esp_camera_init(&config) == ESP_OK);
}

// ==== Helpers: Deep Sleep / Wake ====
void holdFlashLowAndEnableDeepSleepHold() {
  digitalWrite(FLASH_LED_PIN, LOW);
  gpio_hold_en((gpio_num_t)FLASH_LED_PIN);
  gpio_deep_sleep_hold_en();
}

void releaseFlashHoldAfterWake() {
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t)FLASH_LED_PIN);
}

void armTriggerAndSleep() {
  while (digitalRead(TRIGGER_PIN) == HIGH) {
    delay(10);
  }
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 1);
  holdFlashLowAndEnableDeepSleepHold();
  Serial.println("Armed: waiting for D13 HIGH → deep sleep...");
  esp_deep_sleep_start();
}

void sleepCooldown30sOnlyTimer() {
  esp_sleep_enable_timer_wakeup(COOLDOWN_MICROSECONDS);
  holdFlashLowAndEnableDeepSleepHold();
  Serial.println("Cooldown: 30s timer → deep sleep...");
  esp_deep_sleep_start();
}

// ==== WiFi ====
bool connectWiFi(const String &ssid, const String &password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  Serial.printf("Connecting to %s", ssid.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("WiFi connect failed");
  return false;
}

// ==== Arduino setup ====
void setup() {
  Serial.begin(9600);
  delay(50);

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  pinMode(TRIGGER_PIN, INPUT_PULLDOWN);

  releaseFlashHoldAfterWake();
  initializeFileSystem();

  // Always start AP for 1 minute on boot/reset
  startAccessPointForConfiguration();

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  Serial.printf("Wake cause: %d\n", (int)cause);

  // If woken by trigger, capture immediately (minimal delay)
  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    if (!initializeCamera()) {
      Serial.println("Camera init failed");
      sleepCooldown30sOnlyTimer();
    }

    // Capture immediately
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(100);
    camera_fb_t* frameBuffer = esp_camera_fb_get();
    digitalWrite(FLASH_LED_PIN, LOW);

    if (!frameBuffer) {
      Serial.println("Camera capture failed");
      sleepCooldown30sOnlyTimer();
    }

    // Connect WiFi after capture
    String ssid, password;
    if (!loadWiFiCredentials(ssid, password) || !connectWiFi(ssid, password)) {
      esp_camera_fb_return(frameBuffer);
      sleepCooldown30sOnlyTimer();
    }

    configTime(gmtOffsetSeconds, daylightOffsetSeconds, ntpServer);

    // Upload captured frame
    String filename = generateFilenameTimestamp() + ".jpg";
    String remotePath = String(remoteDirectory) + "/" + filename;
    bool ok = uploadMultipartFile(remotePath, frameBuffer->buf, frameBuffer->len, "image/jpeg");

    esp_camera_fb_return(frameBuffer);
    esp_camera_deinit();

    if (ok) {
      String json = loadJsonFile();
      if (json == "[]") json = "[\"" + filename + "\"]";
      else json = json.substring(0, json.length()-1) + ",\"" + filename + "\"]";
      saveJsonFile(json);

      uploadMultipartFile(String(remoteDirectory) + "/photos.json",
                          (uint8_t*)json.c_str(), json.length(),
                          "application/json");
    } else {
      Serial.println("Upload failed");
    }

    sleepCooldown30sOnlyTimer();
  }
  else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    armTriggerAndSleep();
  }
  else {
    // Cold boot: keep AP running until timeout
    Serial.println("Cold boot: staying awake for AP window");
  }
}

// ==== Arduino loop ====
void loop() {
  if (WiFi.getMode() == WIFI_AP) {
    server.handleClient();
    if (millis() - accessPointStartTime > 60000) {
      Serial.println("AP window expired, continuing with normal workflow...");

      // Load WiFi credentials
      String ssid, password;
      if (!loadWiFiCredentials(ssid, password)) {
        Serial.println("No WiFi credentials saved, restarting...");
        ESP.restart();
      }

      // After AP window: enter normal workflow
      armTriggerAndSleep();
    }
  }
}
