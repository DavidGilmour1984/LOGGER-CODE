#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <WebServer.h>
#include <Preferences.h>
#include "time.h"
#include "esp_sleep.h"

#include <OneWire.h>
#include <DallasTemperature.h>

// ==== PINS / SENSORS ====
const int   ADC_PIN = 36;   // Depth ADC
#define ONE_WIRE_BUS 14     // DS18B20 data pin

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// ==== GLOBAL STATE / PREFS ====
Preferences prefs;

// Calibration (raw ADC at 0 m and at 2 m)
float  calZero  = 0.0f;
float  calTwoM  = 4095.0f;

// Credentials & API (defaults)
String wifiSSID = "G2.4";
String wifiPASS = "";
String apiKey   = "";

// File names (defaults)
String localFile  = "/data.csv";              // LittleFS path
String remoteFile = "TankLogger/data.csv";    // Neocities remote path

// ==== AP SETTINGS / WEB ====
const char* AP_SSID = "TankLogger";
const char* AP_PASS = "12345678";
WebServer server(80);

// ==== TIME / NTP / DST (NZ) ====
const char* ntpServer = "pool.ntp.org";
const char* tzRule    = "NZST-12NZDT,M9.5.0,M4.1.0/3"; // NZST->NZDT rules

// CSV header
const char* CSV_HEADER = "timestamp,depth_m,temperature_C\n";

// UART2 on pins 16/17 for mirror logging
HardwareSerial Serial2Port(2);

// ---------- Utilities ----------
void logBoth(const String &msg){ Serial.print(msg); Serial2Port.print(msg); }
void logBothln(const String &msg){ Serial.println(msg); Serial2Port.println(msg); }

String currentTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "1970-01-01 00:00:00";
  char buf[25];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

// ---------- Depth / Temp ----------
float readDepth() {
  int raw = analogRead(ADC_PIN);
  float depth = 2.0f * (raw - calZero) / (calTwoM - calZero);
  if (!isfinite(depth)) depth = 0.0f;
  if (depth < 0) depth = 0;
  return depth;
}

float readTemperature() {
  sensors.requestTemperatures();
  float t = sensors.getTempCByIndex(0);
  if (t == DEVICE_DISCONNECTED_C) {
    Serial.println("[TEMP] Error: DS18B20 not found");
    return NAN;
  }
  return t;
}

// ---------- LittleFS ----------
bool createFreshCSV() {
  Serial.println("[FS] Creating fresh CSV file...");
  if (!LittleFS.begin(false)) {
    if (!LittleFS.begin(true)) {
      Serial.println("[FS] Mount failed");
      return false;
    }
  }
  LittleFS.format();
  LittleFS.begin(false);

  if (!localFile.startsWith("/")) localFile = "/" + localFile;

  File f = LittleFS.open(localFile, FILE_WRITE);
  if (!f) { Serial.printf("[FS] Failed to create %s\n", localFile.c_str()); return false; }
  f.print(CSV_HEADER);
  f.close();
  Serial.printf("[FS] New %s created with header\n", localFile.c_str());
  return true;
}

void appendRow() {
  if (!LittleFS.begin(false)) {
    if (!LittleFS.begin(true)) return;
  }
  if (!localFile.startsWith("/")) localFile = "/" + localFile;

  File f = LittleFS.open(localFile, FILE_APPEND);
  if (!f) return;

  float depth = readDepth();
  float tempC = readTemperature();
  String row = currentTimestamp() + "," + String(depth, 2) + "," + String(tempC, 2) + "\n";
  f.print(row);
  f.close();

  Serial.printf("[FS] Wrote row: %s", row.c_str());
}

void appendRowAndUpload() {
  appendRow();
  uploadToNeocities();
}

// ---------- Upload ----------
bool uploadToNeocities() {
  if (apiKey == "") {
    Serial.println("[NET] No API key set, skipping upload.");
    return false;
  }
  if (!LittleFS.begin(false)) {
    if (!LittleFS.begin(true)) return false;
  }
  if (!localFile.startsWith("/")) localFile = "/" + localFile;

  File f = LittleFS.open(localFile, FILE_READ);
  if (!f) return false;

  size_t size = f.size();
  std::unique_ptr<uint8_t[]> buf(new uint8_t[size]);
  f.read(buf.get(), size);
  f.close();

  if (remoteFile.length() == 0) remoteFile = "TankLogger/data.csv";

  String boundary = "----ESP32Boundary";
  String start =
    "--" + boundary + "\r\n"
    "Content-Disposition: form-data; name=\"" + remoteFile + "\"; filename=\"data.csv\"\r\n"
    "Content-Type: text/csv\r\n\r\n";
  String end = "\r\n--" + boundary + "--\r\n";

  size_t len = start.length() + size + end.length();
  std::unique_ptr<uint8_t[]> body(new uint8_t[len]);
  memcpy(body.get(), start.c_str(), start.length());
  memcpy(body.get() + start.length(), buf.get(), size);
  memcpy(body.get() + start.length() + size, end.c_str(), end.length());

  Serial.printf("[NET] Uploading %s as '%s'...\n", localFile.c_str(), remoteFile.c_str());
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http;
  if (!http.begin(client, "https://neocities.org/api/upload")) return false;
  http.addHeader("Authorization", String("Bearer ") + apiKey);
  http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
  int code = http.POST(body.get(), len);
  http.end();
  Serial.printf("[NET] Upload HTTP %d\n", code);
  return (code >= 200 && code < 300);
}

// ---------- Web UI ----------
void sendHomePage() {
  String mac = WiFi.macAddress();
  float depth = readDepth();
  float tempC = readTemperature();

  String html =
    "<!DOCTYPE html><html><head><meta charset='UTF-8'/>"
    "<meta name='viewport' content='width=device-width, initial-scale=1'/>"
    "<title>Tank Logger Setup</title>"
    "<style>:root{--bg:#0e1726;--panel:rgba(255,255,255,0.08);--ink:#f5f9ff;--muted:#b0c4de;--accent1:#00e5ff;--danger:#ff6b6b;}"
    "body{margin:0;background:var(--bg);color:var(--ink);font:32px Helvetica,Arial,sans-serif;"
    "display:flex;align-items:center;justify-content:center;min-height:100vh;padding:24px}"
    ".card{background:var(--panel);padding:40px;border-radius:32px;max-width:1000px;width:100%;box-shadow:0 30px 60px rgba(0,0,0,.5);}"
    "h1{font-size:3.0rem;margin:0 0 24px 0;text-align:center}"
    "p{margin:12px 0;font-size:1.6rem;color:var(--muted)}"
    "label{display:block;margin:18px 0 8px 0;font-size:1.6rem;color:var(--muted)}"
    "input{width:80%;padding:20px;border:none;border-radius:16px;background:rgba(255,255,255,0.1);color:var(--ink);font-size:1.6rem}"
    "button{padding:18px 22px;margin-top:16px;font-size:1.6rem;font-weight:700;border:none;border-radius:16px;background:var(--accent1);color:#000;cursor:pointer}"
    "button:hover{filter:brightness(1.15)}.danger{background:var(--danger);color:#fff}"
    ".row{display:grid;grid-template-columns:auto 1fr auto;gap:12px;align-items:center;margin:10px 0}"
    ".split{display:grid;grid-template-columns:1fr 1fr;gap:16px}"
    ".split3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:16px}"
    "</style>"
    "<script>function confirmClear(path){if(confirm('Are you sure?')){window.location.href='/' + path;}}</script>"
    "</head><body><div class='card'>";

  html += "<h1>Tank Logger</h1>";
  html += "<p><b>MAC:</b> " + mac + "</p>";
  html += "<p><b>Depth:</b> " + String(depth,2) + " m | <b>Temp:</b> " + String(tempC,2) + " °C</p>";

  html += "<form action='/test'><button type='submit'>Send Test Data</button></form>";

  html += "<div class='split3'>"
          "<form action='/cal0'><button type='submit'>Set 0 m Calibration</button></form>"
          "<form action='/cal2'><button type='submit'>Set 2 m Calibration</button></form>"
          "<form action='/clearcal'><button type='submit' class='danger'>Clear Calibration</button></form>"
          "</div>";

  html += "<label>WiFi SSID</label>"
          "<form class='row' action='/setssid' method='POST'>"
          "<button type='submit'>Set</button><input name='value' value='" + wifiSSID + "'>"
          "<button type='button' class='danger' onclick=\"confirmClear('resetssid')\">Reset</button></form>";
  html += "<label>WiFi Password</label>"
          "<form class='row' action='/setpass' method='POST'>"
          "<button type='submit'>Set</button><input name='value' value='" + wifiPASS + "'>"
          "<button type='button' class='danger' onclick=\"confirmClear('resetpass')\">Reset</button></form>";
  html += "<label>API Key</label>"
          "<form class='row' action='/setapi' method='POST'>"
          "<button type='submit'>Set</button><input name='value' value='" + apiKey + "'>"
          "<button type='button' class='danger' onclick=\"confirmClear('resetapi')\">Reset</button></form>";
  html += "<label>Local Filename</label>"
          "<form class='row' action='/setlocal' method='POST'>"
          "<button type='submit'>Set</button><input name='value' value='" + localFile + "'>"
          "<button type='button' class='danger' onclick=\"confirmClear('resetlocal')\">Reset</button></form>";
  html += "<label>Remote Filename</label>"
          "<form class='row' action='/setremote' method='POST'>"
          "<button type='submit'>Set</button><input name='value' value='" + remoteFile + "'>"
          "<button type='button' class='danger' onclick=\"confirmClear('resetremote')\">Reset</button></form>";

  html += "<div class='split'>"
          "<button class='danger' onclick=\"confirmClear('resetlogs')\">Clear Logs</button>"
          "<button class='danger' onclick=\"confirmClear('clearcreds')\">Clear Preferences</button>"
          "</div>";

  html += "</div></body></html>";
  server.send(200,"text/html",html);
}

// ---------- Handlers ----------
void handleRoot(){ sendHomePage(); }
void handleTest(){ appendRowAndUpload(); server.sendHeader("Location","/"); server.send(303); }
void handleUpload(){ uploadToNeocities(); server.sendHeader("Location","/"); server.send(303); }

void handleCal0(){ calZero=analogRead(ADC_PIN); prefs.putFloat("calZero",calZero); server.sendHeader("Location","/"); server.send(303); }
void handleCal2(){ calTwoM=analogRead(ADC_PIN); prefs.putFloat("calTwoM",calTwoM); server.sendHeader("Location","/"); server.send(303); }
void handleClearCal(){ calZero=0; calTwoM=4095; prefs.putFloat("calZero",calZero); prefs.putFloat("calTwoM",calTwoM); server.sendHeader("Location","/"); server.send(303); }

// Editable field handlers
void handleSetSSID(){ if(server.hasArg("value")){ wifiSSID=server.arg("value"); prefs.putString("ssid",wifiSSID);} server.sendHeader("Location","/"); server.send(303);}
void handleResetSSID(){ wifiSSID="G2.4"; prefs.putString("ssid",wifiSSID); server.sendHeader("Location","/"); server.send(303);}
void handleSetPASS(){ if(server.hasArg("value")){ wifiPASS=server.arg("value"); prefs.putString("pass",wifiPASS);} server.sendHeader("Location","/"); server.send(303);}
void handleResetPASS(){ wifiPASS="DhH2TuTYRVtQgYLF"; prefs.putString("pass",wifiPASS); server.sendHeader("Location","/"); server.send(303);}
void handleSetAPI(){ if(server.hasArg("value")){ apiKey=server.arg("value"); prefs.putString("api",apiKey);} server.sendHeader("Location","/"); server.send(303);}
void handleResetAPI(){ apiKey="8841216e260eef9d5883dd8f6da93ffd"; prefs.putString("api",apiKey); server.sendHeader("Location","/"); server.send(303);}
void handleSetLocal(){ if(server.hasArg("value")){ localFile=server.arg("value"); prefs.putString("localFile",localFile);} server.sendHeader("Location","/"); server.send(303);}
void handleResetLocal(){ localFile="/data.csv"; prefs.putString("localFile",localFile); server.sendHeader("Location","/"); server.send(303);}
void handleSetRemote(){ if(server.hasArg("value")){ remoteFile=server.arg("value"); prefs.putString("remoteFile",remoteFile);} server.sendHeader("Location","/"); server.send(303);}
void handleResetRemote(){ remoteFile="TankLogger/data.csv"; prefs.putString("remoteFile",remoteFile); server.sendHeader("Location","/"); server.send(303);}

// New handlers for missing buttons
void handleResetLogs(){
  if (!LittleFS.begin(false)) {
    if (!LittleFS.begin(true)) {
      server.send(500, "text/plain", "FS mount failed");
      return;
    }
  }
  if (LittleFS.exists(localFile)) {
    LittleFS.remove(localFile);
    Serial.println("[FS] Logs cleared");
  }
  createFreshCSV();
  server.sendHeader("Location","/");
  server.send(303);
}

void handleClearCreds(){
  prefs.clear();
  wifiSSID="G2.4";
  wifiPASS="";
  apiKey="";
  localFile="/data.csv";
  remoteFile="TankLogger/data.csv";
  Serial.println("[CFG] Preferences cleared, defaults restored");
  server.sendHeader("Location","/");
  server.send(303);
}

// ---------- Setup / Loop ----------
void setup(){
  Serial.begin(115200);
  Serial2Port.begin(9600, SERIAL_8N1, 16, 17);

  Serial.println("\n[BOOT] TankLogger starting...");
  Serial2Port.println("\n[BOOT] TankLogger starting...");

  prefs.begin("depthlogger", false);
  calZero=prefs.getFloat("calZero",0.0);
  calTwoM=prefs.getFloat("calTwoM",4095.0);
  wifiSSID=prefs.getString("ssid","G2.4");
  wifiPASS=prefs.getString("pass","");
  apiKey=prefs.getString("api","");
  localFile=prefs.getString("localFile","/data.csv");
  remoteFile=prefs.getString("remoteFile","TankLogger/data.csv");

  Serial.printf("[CFG] Loaded calibration: Zero=%.1f TwoM=%.1f\n", calZero, calTwoM);
  Serial.printf("[CFG] WiFi SSID='%s' passLen=%d APIkeyLen=%d\n", wifiSSID.c_str(), wifiPASS.length(), apiKey.length());
  Serial.printf("[CFG] Local='%s' Remote='%s'\n", localFile.c_str(), remoteFile.c_str());

  pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
  sensors.begin();
  Serial.println("[TEMP] Dallas sensor initialized");

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID,AP_PASS);
  Serial.printf("[NET] Access Point started: SSID='%s' PASS='%s' IP=%s\n", 
                AP_SSID, AP_PASS, WiFi.softAPIP().toString().c_str());

  if(wifiSSID!=""){ 
    Serial.printf("[NET] Attempting STA connect to '%s'...\n", wifiSSID.c_str()); 
    WiFi.begin(wifiSSID.c_str(),wifiPASS.c_str()); 
  }

  server.on("/",handleRoot);
  server.on("/test",handleTest);
  server.on("/upload",handleUpload);
  server.on("/cal0",handleCal0);
  server.on("/cal2",handleCal2);
  server.on("/clearcal",handleClearCal);

  server.on("/setssid",HTTP_POST,handleSetSSID);
  server.on("/resetssid",handleResetSSID);
  server.on("/setpass",HTTP_POST,handleSetPASS);
  server.on("/resetpass",handleResetPASS);
  server.on("/setapi",HTTP_POST,handleSetAPI);
  server.on("/resetapi",handleResetAPI);
  server.on("/setlocal",HTTP_POST,handleSetLocal);
  server.on("/resetlocal",handleResetLocal);
  server.on("/setremote",HTTP_POST,handleSetRemote);
  server.on("/resetremote",handleResetRemote);

  server.on("/resetlogs",handleResetLogs);
  server.on("/clearcreds",handleClearCreds);

  server.begin();
  Serial.println("[WEB] Web server started");

  configTime(0,0,ntpServer); setenv("TZ",tzRule,1); tzset();
  Serial.println("[TIME] Time config set, syncing...");

  if(!LittleFS.begin(false)) {
    Serial.println("[FS] Mount failed, creating fresh CSV...");
    createFreshCSV();
  } else {
    Serial.println("[FS] LittleFS mounted OK");
  }
}

void loop(){
  server.handleClient();

  static bool didTopOfHour=false;
  static unsigned long awakeStart=millis();
  static unsigned long lastReport=0;
  static unsigned long lastClientCheck=0;

  if(millis()-lastReport>5000){
    lastReport=millis();
    float d=readDepth();
    float t=readTemperature();
    long uptime = millis()/1000;
    long left=(long)(120000-(millis()-awakeStart))/1000;
    if(left<0) left=0;
    Serial.printf("[SYS] Uptime=%lus, awake left=%lds | Depth=%.2fm Temp=%.2fC\n",
                  uptime, left, d, t);
  }

  struct tm ti;
  if(!didTopOfHour && getLocalTime(&ti)){
    if(ti.tm_min==0){
      Serial.printf("[LOG] Top of hour (%02d:%02d:%02d), logging now\n", ti.tm_hour, ti.tm_min, ti.tm_sec);
      didTopOfHour=true;
      appendRowAndUpload();
    }
  }

  if(millis()-lastClientCheck>15000){
    lastClientCheck=millis();
    wl_status_t st = WiFi.status();
    if(st==WL_CONNECTED){
      Serial.printf("[NET] STA connected IP=%s RSSI=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      Serial.printf("[NET] STA not connected (status=%d)\n", st);
    }
  }

  if(millis()-awakeStart>120000){
    Serial.println("[SYS] Awake window ended, preparing to sleep...");
    WiFi.disconnect(true); WiFi.mode(WIFI_OFF); btStop();
    delay(100);
    Serial.println("[NET] WiFi + BT shut down");

    struct tm timeinfo; getLocalTime(&timeinfo);
    int secsPast=timeinfo.tm_min*60+timeinfo.tm_sec;
    int secsToHour=3600-secsPast;
    int sleepSecs=secsToHour-120; 
    if(sleepSecs<10) sleepSecs=10;

    Serial.printf("[SYS] Will sleep for %d seconds\n", sleepSecs);
    Serial.printf("[SYS] Expected wake-up around %02d:%02d\n",
                  (timeinfo.tm_hour + (timeinfo.tm_min + sleepSecs/60)/60)%24,
                  (timeinfo.tm_min + sleepSecs/60)%60);

    for(int i=10;i>0;i--){
      Serial.printf("[SYS] Entering deep sleep in %d...\n", i);
      delay(1000);
    }

    esp_sleep_enable_timer_wakeup((uint64_t)sleepSecs*1000000ULL);
    Serial.println("[SYS] Going into deep sleep now!");
    esp_deep_sleep_start();
  }
}
