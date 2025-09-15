/**** ESP32-CAM (AI-Thinker) – Triggered Photo Uploader with 30s Cooldown ****/
#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "time.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "FS.h"
#include "LittleFS.h"

// ==== USER SETTINGS ====
String apiKey = "8841216e260eef9d5883dd8f6da93ffd";   // Neocities API key
const char* REMOTE_DIR = "photos";                     // remote folder
const char* JSON_PATH  = "/photos.json";               // local index (mirrored remotely)
const char* ntpServer  = "pool.ntp.org";
const long  gmtOffset_sec = 12 * 3600;                 // NZST offset
const int   daylightOffset_s = 0;

// WiFi credentials (fixed as requested)
const char* WIFI_SSID = "StPeters-PSK";
const char* WIFI_PASS = "4OddDevices";

// Pins
#define FLASH_LED_PIN      4          // onboard flash LED (ESP32-CAM)
#define TRIGGER_PIN        13         // D13 → wake/capture on HIGH
#define COOLDOWN_US        (30ULL * 1000000ULL)  // 30 seconds

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
bool initFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return false;
  }
  return true;
}

String loadJSON() {
  if (!LittleFS.exists(JSON_PATH)) return "[]";
  File f = LittleFS.open(JSON_PATH, "r");
  if (!f) return "[]";
  String content = f.readString();
  f.close();
  return (content.length() ? content : "[]");
}

void saveJSON(const String& json) {
  File f = LittleFS.open(JSON_PATH, "w");
  if (!f) return;
  f.print(json);
  f.close();
}

// ==== Helpers: Camera / Upload ====
String filenameTimestamp() {
  struct tm ti;
  if (getLocalTime(&ti)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &ti);
    return String(buf);
  }
  return String(millis());
}

bool uploadMultipart(const String& remotePath, const uint8_t* data, size_t len, const char* mime) {
  String boundary = "----ESP32Boundary" + String((uint32_t)millis());
  String head =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"" + remotePath + "\"; filename=\"" +
    remotePath.substring(remotePath.lastIndexOf('/')+1) + "\"\r\n"
    "Content-Type: " + String(mime) + "\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  size_t total = head.length() + len + tail.length();
  uint8_t* body = (uint8_t*)malloc(total);
  if (!body) return false;
  memcpy(body, head.c_str(), head.length());
  memcpy(body + head.length(), data, len);
  memcpy(body + head.length() + len, tail.c_str(), tail.length());

  WiFiClientSecure client;
  client.setInsecure();  // Neocities uses valid certs; skipping validation for simplicity
  HTTPClient http;
  if (!http.begin(client, "https://neocities.org/api/upload")) { free(body); return false; }
  http.addHeader("Authorization", String("Bearer ") + apiKey);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  int code = http.POST(body, total);
  free(body);
  http.end();
  Serial.printf("Upload %s → HTTP %d\n", remotePath.c_str(), code);
  return (code >= 200 && code < 300);
}

bool initCamera() {
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
  config.frame_size   = FRAMESIZE_SVGA;  // 800x600-ish; adjust if needed
  config.jpeg_quality = 12;              // lower = better quality
  config.fb_count     = 1;
  return (esp_camera_init(&config) == ESP_OK);
}

bool captureAndUpload() {
  digitalWrite(FLASH_LED_PIN, LOW);  // ensure known state
  digitalWrite(FLASH_LED_PIN, HIGH); // quick flash for exposure
  delay(100);
  camera_fb_t* fb = esp_camera_fb_get();
  digitalWrite(FLASH_LED_PIN, LOW);
  if (!fb) {
    Serial.println("Camera capture failed");
    return false;
  }

  String name = filenameTimestamp() + ".jpg";
  String remotePath = String(REMOTE_DIR) + "/" + name;

  bool ok = uploadMultipart(remotePath, fb->buf, fb->len, "image/jpeg");
  esp_camera_fb_return(fb);
  if (!ok) return false;

  // Update local JSON index and mirror it remotely
  String json = loadJSON();
  if (json == "[]") json = "[\"" + name + "\"]";
  else              json = json.substring(0, json.length()-1) + ",\"" + name + "\"]";
  saveJSON(json);

  uploadMultipart(String(REMOTE_DIR) + "/photos.json",
                  (uint8_t*)json.c_str(), json.length(),
                  "application/json");
  return true;
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
  // Make sure the trigger is not currently held HIGH before arming
  while (digitalRead(TRIGGER_PIN) == HIGH) {
    delay(10);
  }
  // EXT0 wake on HIGH for RTC-capable GPIO13
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_13, 1);
  holdFlashLowAndEnableDeepSleepHold();
  Serial.println("Armed: waiting for D13 HIGH → deep sleep...");
  esp_deep_sleep_start();
}

void sleepCooldown30sOnlyTimer() {
  esp_sleep_enable_timer_wakeup(COOLDOWN_US);
  holdFlashLowAndEnableDeepSleepHold();
  Serial.println("Cooldown: 30s timer → deep sleep...");
  esp_deep_sleep_start();
}

// ==== WiFi ====
bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to %s", WIFI_SSID);
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

// ==== Arduino setup/loop ====
void setup() {
  Serial.begin(9600);      // per your default
  delay(50);

  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  // Use internal pulldown on trigger pin
  pinMode(TRIGGER_PIN, INPUT_PULLDOWN);

  releaseFlashHoldAfterWake();
  initFS();

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  Serial.printf("Wake cause: %d\n", (int)cause);

  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    // Triggered by D13 going HIGH → take & upload photo, then cooldown
    if (!connectWiFi()) {
      // If WiFi failed, skip capture to save power and retry next trigger
      sleepCooldown30sOnlyTimer();
    }

    configTime(gmtOffset_sec, daylightOffset_s, ntpServer);
    if (!initCamera()) {
      Serial.println("Camera init failed");
      sleepCooldown30sOnlyTimer();
    }

    bool ok = captureAndUpload();
    if (!ok) Serial.println("Capture/upload failed");
    esp_camera_deinit();

    // Enforce 30s minimum between shots
    sleepCooldown30sOnlyTimer();
  }
  else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    // Cooldown finished → re-arm the trigger and go back to deep sleep
    armTriggerAndSleep();
  }
  else {
    // Cold boot or other cause → connect WiFi (optional for time sync), arm & sleep
    connectWiFi(); // not strictly required, but helps time sync on first run
    configTime(gmtOffset_sec, daylightOffset_s, ntpServer);
    armTriggerAndSleep();
  }
}

void loop() {
  // Not used — device spends nearly all time in deep sleep
}
