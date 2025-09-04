#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "time.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "FS.h"
#include "LittleFS.h"
#include <WebServer.h>
#include <ArduinoJson.h>

// ==== USER SETTINGS ====
String apiKey = "";
const char* REMOTE_DIR = "photos";
const char* JSON_PATH  = "/photos.json";
const uint32_t CAPTURE_MS = 30000;

const char* ntpServer        = "pool.ntp.org";
const long  gmtOffset_sec    = 12 * 3600;
const int   daylightOffset_s = 0;

#define FLASH_LED_PIN      4
#define TRIGGER_PIN       15
#define COOLDOWN_US (30ULL * 1000000ULL)

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

// ==== GLOBALS ====
WebServer server(80);
String wifiSSID = "";
String wifiPASS = "";
unsigned long apStartMillis = 0;
bool clientConnected = false;
bool unlocked = false;  // portal lock state

// ==== Filesystem Helpers ====
bool initFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return false;
  }
  return true;
}

void loadWiFiConfig() {
  if (!LittleFS.exists("/config.json")) return;
  File f = LittleFS.open("/config.json", "r");
  if (!f) return;
  DynamicJsonDocument doc(512);
  if (!deserializeJson(doc, f)) {
    wifiSSID = doc["ssid"].as<String>();
    wifiPASS = doc["pass"].as<String>();
    apiKey   = doc["api"].as<String>();
  }
  f.close();
}

void saveWiFiConfig() {
  DynamicJsonDocument doc(512);
  doc["ssid"] = wifiSSID;
  doc["pass"] = wifiPASS;
  doc["api"]  = apiKey;
  File f = LittleFS.open("/config.json", "w");
  if (!f) return;
  serializeJson(doc, f);
  f.close();
}

void clearWiFiConfig() {
  LittleFS.remove("/config.json");
  wifiSSID = "";
  wifiPASS = "";
  apiKey   = "";
}

void clearPhotoLog() {
  File f = LittleFS.open(JSON_PATH, "w");
  if (f) {
    f.print("[]");
    f.close();
  }
}

// ==== JSON for photo index ====
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

// ==== Camera helpers ====
String filenameTimestamp() {
  struct tm ti;
  if (getLocalTime(&ti)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &ti);
    return String(buf);
  }
  return String(millis());
}

bool uploadMultipart(const String& filename, const uint8_t* data, size_t len, const char* mime) {
  String boundary = "----ESP32Boundary" + String((uint32_t)millis());
  String head =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"" + filename + "\"; filename=\"" + filename.substring(filename.lastIndexOf('/')+1) + "\"\r\n"
    "Content-Type: " + String(mime) + "\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  size_t total = head.length() + len + tail.length();
  uint8_t* body = (uint8_t*)malloc(total);
  if (!body) return false;
  memcpy(body, head.c_str(), head.length());
  memcpy(body + head.length(), data, len);
  memcpy(body + head.length() + len, tail.c_str(), tail.length());

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, "https://neocities.org/api/upload")) return false;
  http.addHeader("Authorization", String("Bearer ") + apiKey);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  int code = http.POST(body, total);
  free(body);
  http.end();
  Serial.printf("Upload %s HTTP %d\n", filename.c_str(), code);
  return (code >= 200 && code < 300);
}

bool captureAndUpload() {
  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(100);
  camera_fb_t* fb = esp_camera_fb_get();
  digitalWrite(FLASH_LED_PIN, LOW);
  if (!fb) { Serial.println("Camera capture failed"); return false; }

  String name = filenameTimestamp() + ".jpg";
  String remotePath = String(REMOTE_DIR) + "/" + name;

  bool ok = uploadMultipart(remotePath, fb->buf, fb->len, "image/jpeg");
  esp_camera_fb_return(fb);
  if (!ok) return false;

  String json = loadJSON();
  if (json == "[]") json = "[\"" + name + "\"]";
  else json = json.substring(0, json.length()-1) + ",\"" + name + "\"]";
  saveJSON(json);

  uploadMultipart(String(REMOTE_DIR) + "/photos.json",
                  (uint8_t*)json.c_str(), json.length(),
                  "application/json");
  return true;
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
  config.frame_size   = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count     = 1;
  return (esp_camera_init(&config) == ESP_OK);
}

// ==== Sleep helpers ====
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
  while (digitalRead(TRIGGER_PIN) == HIGH) delay(10);
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_15, 1);
  holdFlashLowAndEnableDeepSleepHold();
  esp_deep_sleep_start();
}

void sleepCooldown30sOnlyTimer() {
  esp_sleep_enable_timer_wakeup(COOLDOWN_US);
  holdFlashLowAndEnableDeepSleepHold();
  esp_deep_sleep_start();
}

// ==== HTML Portal ====
String htmlPage(bool showSetup = false) {
  String mac = WiFi.macAddress();

  if (!showSetup) {
    // Keypad lock screen
    String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'/>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
    "<title>Enter Code</title>"
    "<style>"
    "body{margin:0;background:#0e1726;color:#f5f9ff;font:48px Helvetica,Arial,sans-serif;"
    "display:flex;flex-direction:column;align-items:center;justify-content:center;height:100vh}"
    "h1{font-size:4rem;margin-bottom:40px}"
    "#code{margin-bottom:40px;font-size:3rem;letter-spacing:12px}"
    ".grid{display:grid;grid-template-columns:repeat(3,1fr);gap:20px;width:90%;max-width:600px}"
    "button{font-size:3rem;padding:40px;border:none;border-radius:20px;background:#00e5ff;color:#000;cursor:pointer}"
    "</style><script>"
    "let input='';"
    "function press(k){input+=k;document.getElementById('code').innerText=input;"
    "if(input==='1379#'){window.location.href='/unlock';}"
    "if(input.length>10){input='';document.getElementById('code').innerText='';}}"
    "</script></head><body>"
    "<h1>Enter Code</h1><div id='code'></div>"
    "<div class='grid'>"
    "<button onclick=\"press('1')\">1</button><button onclick=\"press('2')\">2</button><button onclick=\"press('3')\">3</button>"
    "<button onclick=\"press('4')\">4</button><button onclick=\"press('5')\">5</button><button onclick=\"press('6')\">6</button>"
    "<button onclick=\"press('7')\">7</button><button onclick=\"press('8')\">8</button><button onclick=\"press('9')\">9</button>"
    "<button onclick=\"press('*')\">*</button><button onclick=\"press('0')\">0</button><button onclick=\"press('#')\">#</button>"
    "</div></body></html>";
    return page;
  }

  // WiFi/API setup page (big UI)
  String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'/>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
  "<title>BushSentinel Setup</title>"
  "<style>"
  ":root{--bg:#0e1726;--panel:rgba(255,255,255,0.08);--ink:#f5f9ff;--muted:#b0c4de;--accent1:#00e5ff;--danger:#ff6b6b;}"
  "body{margin:0;background:var(--bg);color:var(--ink);font:32px Helvetica,Arial,sans-serif;"
  "display:flex;align-items:center;justify-content:center;height:100vh}"
  ".card{background:var(--panel);padding:80px;border-radius:32px;max-width:900px;width:100%;"
  "box-shadow:0 30px 60px rgba(0,0,0,.5);}"
  "h1{font-size:3.5rem;margin:0 0 30px 0;text-align:center}"
  "p{margin:16px 0;font-size:2rem;color:var(--muted)}"
  "label{display:block;margin-top:28px;font-size:2rem;color:var(--muted)}"
  "input{width:100%;padding:32px;margin-top:8px;margin-bottom:24px;border:none;border-radius:20px;"
  "background:rgba(255,255,255,0.1);color:var(--ink);font-size:2rem}"
  "button{width:100%;padding:36px;margin-top:28px;font-size:2.2rem;font-weight:700;border:none;"
  "border-radius:24px;background:var(--accent1);color:#000;cursor:pointer}"
  "button:hover{filter:brightness(1.15)}.danger{background:var(--danger);color:#fff}"
  "</style><script>"
  "function confirmClear(type){if(confirm('Are you sure you want to clear '+type+'?')){window.location.href='/' + type;}}"
  "</script></head><body><div class='card'>";

  page += "<h1>BushSentinel WiFi Setup</h1>";
  page += "<p><b>MAC:</b> " + mac + "</p>";
  page += "<form action='/set' method='POST'>"
          "<label>WiFi SSID</label><input name='ssid' value='" + wifiSSID + "'>"
          "<label>Password</label><input name='pass' value='" + wifiPASS + "'>"
          "<label>API Key</label><input name='api' value='" + apiKey + "'>"
          "<button type='submit'>Save</button></form>";
  page += "<form action='/connect' method='POST'><button type='submit'>Connect & Reboot</button></form>";
  page += "<button class='danger' onclick=\"confirmClear('clearlog')\">Clear Photo Log</button>";
  page += "<button class='danger' onclick=\"confirmClear('clearcreds')\">Clear Credentials</button>";
  page += "</div></body></html>";
  return page;
}

// ==== Handlers ====
void handleRoot() { server.send(200, "text/html", htmlPage(false)); }
void handleUnlock() { server.send(200, "text/html", htmlPage(true)); }
void handleSet() {
  if (server.hasArg("ssid")) wifiSSID = server.arg("ssid");
  if (server.hasArg("pass")) wifiPASS = server.arg("pass");
  if (server.hasArg("api"))  apiKey   = server.arg("api");
  saveWiFiConfig();
  server.send(200, "text/html", "<meta http-equiv='refresh' content='1;url=/unlock'><p>Saved! Redirecting...</p>");
}
void handleConnect() { server.send(200, "text/html", "<p>Rebooting...</p>"); delay(1000); ESP.restart(); }
void handleClearLog() { clearPhotoLog(); server.send(200, "text/html", "<meta http-equiv='refresh' content='2;url=/unlock'><p>Photo log cleared.</p>"); }
void handleClearCreds() { clearWiFiConfig(); server.send(200, "text/html", "<meta http-equiv='refresh' content='2;url=/unlock'><p>Credentials cleared.</p>"); }

void startAPPortal() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("BushSentinel", "");
  Serial.println("Started AP: BushSentinel");
  Serial.print("MAC: "); Serial.println(WiFi.softAPmacAddress());
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/unlock", handleUnlock);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/clearlog", handleClearLog);
  server.on("/clearcreds", handleClearCreds);
  server.begin();
  apStartMillis = millis();
}

// ==== setup() ====
void setup() {
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  pinMode(TRIGGER_PIN, INPUT_PULLDOWN);
  Serial.begin(115200);
  delay(100);

  releaseFlashHoldAfterWake();
  initFS();
  loadWiFiConfig();

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  // === Only show AP portal on cold boot or if no WiFi config ===
  if (cause == ESP_SLEEP_WAKEUP_UNDEFINED || wifiSSID == "") {
    startAPPortal();
    while (millis() - apStartMillis < 40000) {
      server.handleClient();
      if (clientConnected) apStartMillis = millis(); // extend if active
    }
    server.stop();
    WiFi.softAPdisconnect(true);
    Serial.println("AP closed, resuming normal operation...");
  }

  // === Normal wake logic ===
  if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Wake from EXT0 → capture & upload");
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\nWiFi connected");
    configTime(gmtOffset_sec, daylightOffset_s, ntpServer);
    if (!initCamera()) sleepCooldown30sOnlyTimer();
    bool ok = captureAndUpload();
    if (!ok) Serial.println("Capture/upload failed");
    esp_camera_deinit();
    sleepCooldown30sOnlyTimer();
  }
  else if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("Wake from TIMER → re-arm trigger");
    armTriggerAndSleep();
  }
  else {
    Serial.println("Cold boot → wait for trigger");
    armTriggerAndSleep();
  }
}


// ==== loop() ====
void loop() {
  if (WiFi.getMode() == WIFI_AP) {
    server.handleClient();
  }
}
