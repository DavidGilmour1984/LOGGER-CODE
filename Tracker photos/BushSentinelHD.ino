/**** BushSentinel ESP32 Wrover + OV5640 ****/
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
const char* REMOTE_DIR = "BushSentinelHD";
const char* JSON_PATH  = "/photos.json";

const char* ntpServer        = "pool.ntp.org";
const long  gmtOffset_sec    = 12 * 3600;
const int   daylightOffset_s = 0;

#define FLASH_LED_PIN      25   // onboard LED (Wrover)
#define TRIGGER_PIN        13   // RTC-capable pin for wake trigger
#define COOLDOWN_US (30ULL * 1000000ULL)  // 30s cooldown

// ==== OV5640 Pin Map (QY5640 REV1.2.x) ====
#define CAM_SIOC   23
#define CAM_SIOD   22
#define CAM_XCLK   15
#define CAM_VSYNC  18
#define CAM_HREF   36
#define CAM_PCLK   26
#define CAM_D0      2
#define CAM_D1     14
#define CAM_D2     35
#define CAM_D3     12
#define CAM_D4     27
#define CAM_D5     33
#define CAM_D6     34
#define CAM_D7     39
#define CAM_RESET   5
#define CAM_PWDN   -1

// ==== GLOBALS ====
WebServer server(80);
String wifiSSID = "";
String wifiPASS = "";
unsigned long apStartMillis = 0;
unsigned long lastClientMillis = 0;

// ==== Filesystem Helpers ====
bool initFS() {
  return LittleFS.begin(true);
}

void loadWiFiConfig() {
  if (!LittleFS.exists("/config.json")) return;
  File f = LittleFS.open("/config.json", "r");
  if (!f) return;
  DynamicJsonDocument doc(512);
  if (deserializeJson(doc, f) == DeserializationError::Ok) {
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
  if (f) { f.print("[]"); f.close(); }
}

// ==== JSON photo index ====
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

  config.pin_d0       = CAM_D0;
  config.pin_d1       = CAM_D1;
  config.pin_d2       = CAM_D2;
  config.pin_d3       = CAM_D3;
  config.pin_d4       = CAM_D4;
  config.pin_d5       = CAM_D5;
  config.pin_d6       = CAM_D6;
  config.pin_d7       = CAM_D7;
  config.pin_xclk     = CAM_XCLK;
  config.pin_pclk     = CAM_PCLK;
  config.pin_vsync    = CAM_VSYNC;
  config.pin_href     = CAM_HREF;
  config.pin_sccb_sda = CAM_SIOD;
  config.pin_sccb_scl = CAM_SIOC;
  config.pin_pwdn     = CAM_PWDN;
  config.pin_reset    = CAM_RESET;

  config.xclk_freq_hz = 10000000; // OV5640 stable 10–12 MHz
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_QSXGA; // 2592x1944
    config.jpeg_quality = 5;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_PSRAM;
  } else {
    config.frame_size   = FRAMESIZE_SVGA;
    config.jpeg_quality = 10;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }
  return true;
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
  esp_sleep_enable_ext0_wakeup((gpio_num_t)TRIGGER_PIN, 1);
  holdFlashLowAndEnableDeepSleepHold();
  esp_deep_sleep_start();
}
void sleepCooldown30sOnlyTimer() {
  esp_sleep_enable_timer_wakeup(COOLDOWN_US);
  holdFlashLowAndEnableDeepSleepHold();
  esp_deep_sleep_start();
}

// ==== HTML Portal ====
String htmlPage(bool showSetup) {
  String mac = WiFi.macAddress();
  if (!showSetup) {
    // keypad lock
    return R"rawliteral(
      <!DOCTYPE html><html><head><meta charset='UTF-8'/>
      <meta name='viewport' content='width=device-width, initial-scale=1'/>
      <title>Enter Code</title>
      <style>
      body{margin:0;background:#0e1726;color:#f5f9ff;font:48px Helvetica,Arial,sans-serif;
      display:flex;flex-direction:column;align-items:center;justify-content:center;height:100vh}
      h1{font-size:4rem;margin-bottom:40px}
      #code{margin-bottom:40px;font-size:3rem;letter-spacing:12px}
      .grid{display:grid;grid-template-columns:repeat(3,1fr);gap:20px;width:90%;max-width:600px}
      button{font-size:3rem;padding:40px;border:none;border-radius:20px;background:#00e5ff;color:#000;cursor:pointer}
      </style><script>
      let input='';
      function press(k){input+=k;document.getElementById('code').innerText=input;
      if(input==='1379#'){window.location.href='/unlock';}
      if(input.length>10){input='';document.getElementById('code').innerText='';}}
      </script></head><body>
      <h1>Enter Code</h1><div id='code'></div>
      <div class='grid'>
      <button onclick="press('1')">1</button><button onclick="press('2')">2</button><button onclick="press('3')">3</button>
      <button onclick="press('4')">4</button><button onclick="press('5')">5</button><button onclick="press('6')">6</button>
      <button onclick="press('7')">7</button><button onclick="press('8')">8</button><button onclick="press('9')">9</button>
      <button onclick="press('*')">*</button><button onclick="press('0')">0</button><button onclick="press('#')">#</button>
      </div></body></html>
    )rawliteral";
  }

  // setup page
  String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'/>"
  "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
  "<title>BushSentinel Setup</title>"
  "<style>:root{--bg:#0e1726;--panel:rgba(255,255,255,0.08);--ink:#f5f9ff;--muted:#b0c4de;--accent1:#00e5ff;--danger:#ff6b6b;}"
  "body{margin:0;background:var(--bg);color:var(--ink);font:32px Helvetica,Arial,sans-serif;"
  "display:flex;align-items:center;justify-content:center;height:100vh}"
  ".card{background:var(--panel);padding:80px;border-radius:32px;max-width:900px;width:100%;box-shadow:0 30px 60px rgba(0,0,0,.5);}"
  "h1{font-size:3.5rem;margin:0 0 30px 0;text-align:center}"
  "p{margin:16px 0;font-size:2rem;color:var(--muted)}"
  "label{display:block;margin-top:28px;font-size:2rem;color:var(--muted)}"
  "input{width:100%;padding:32px;margin-top:8px;margin-bottom:24px;border:none;border-radius:20px;background:rgba(255,255,255,0.1);color:var(--ink);font-size:2rem}"
  "button{width:100%;padding:36px;margin-top:28px;font-size:2.2rem;font-weight:700;border:none;border-radius:24px;background:var(--accent1);color:#000;cursor:pointer}"
  "button:hover{filter:brightness(1.15)}.danger{background:var(--danger);color:#fff}</style>"
  "<script>function confirmClear(type){if(confirm('Clear '+type+'?')){window.location.href='/' + type;}}</script>"
  "</head><body><div class='card'>";
  page += "<h1>BushSentinel WiFi Setup</h1>";
  page += "<p><b>MAC:</b> " + mac + "</p>";
  page += "<form action='/set' method='POST'><label>WiFi SSID</label><input name='ssid' value='" + wifiSSID + "'>"
          "<label>Password</label><input name='pass' value='" + wifiPASS + "'>"
          "<label>API Key</label><input name='api' value='" + apiKey + "'>"
          "<button type='submit'>Save</button></form>";
  page += "<form action='/connect' method='POST'><button type='submit'>Connect & Reboot</button></form>";
  page += "<button class='danger' onclick=\"confirmClear('clearlog')\">Clear Photo Log</button>";
  page += "<button class='danger' onclick=\"confirmClear('clearcreds')\">Clear Credentials</button>";
  page += "</div></body></html>";
  return page;
}

// ==== Web Handlers ====
void handleRoot() { lastClientMillis = millis(); server.send(200, "text/html", htmlPage(false)); }
void handleUnlock() { lastClientMillis = millis(); server.send(200, "text/html", htmlPage(true)); }
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
  server.on("/", handleRoot);
  server.on("/unlock", handleUnlock);
  server.on("/set", HTTP_POST, handleSet);
  server.on("/connect", HTTP_POST, handleConnect);
  server.on("/clearlog", handleClearLog);
  server.on("/clearcreds", handleClearCreds);
  server.begin();
  apStartMillis = millis();
  lastClientMillis = millis();
}

// ==== setup() ====
void setup() {
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);
  Serial.begin(115200);
  delay(100);

  releaseFlashHoldAfterWake();
  initFS();
  loadWiFiConfig();

  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

  // === Only show AP portal on cold boot or if no WiFi config ===
  if (cause == ESP_SLEEP_WAKEUP_UNDEFINED || wifiSSID == "") {
    startAPPortal();
    while (millis() - lastClientMillis < 40000) {
      server.handleClient();
    }
    server.stop();
    WiFi.softAPdisconnect(true);
    Serial.println("AP closed, resuming normal operation...");
  }

  // === Timed capture every 60s ===
  Serial.println("Wake → capture & upload (1 min interval)");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPASS.c_str());
  while (WiFi.status() != WL_CONNECTED) { 
    delay(500); 
    Serial.print("."); 
  }
  Serial.println("\nWiFi connected");

  configTime(gmtOffset_sec, daylightOffset_s, ntpServer);

  if (!initCamera()) {
    Serial.println("Camera init failed");
  } else {
    bool ok = captureAndUpload();
    if (!ok) Serial.println("Capture/upload failed");
    esp_camera_deinit();
  }

  // Sleep for 60 seconds
  Serial.println("Sleeping for 60 seconds...");
  esp_sleep_enable_timer_wakeup(60ULL * 1000000ULL);
  holdFlashLowAndEnableDeepSleepHold();
  esp_deep_sleep_start();
}

// ==== loop() ====
void loop() {
  if (WiFi.getMode() == WIFI_AP) {
    server.handleClient();
  }
}
