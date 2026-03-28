#include <WiFi.h>
#include <WebServer.h>

// ================= WIFI =================
#define AP_SSID "ESP32 Relay"
#define AP_PASS ""   // set for security

WebServer server(80);

// ================= RELAY PINS =================
#define LATCH_PIN 12
#define CLOCK_PIN 13
#define DATA_PIN  14
#define OE_PIN    5

uint16_t state = 0;

// ================= WRITE =================
void updateRelays() {
  uint16_t raw = state;

  digitalWrite(LATCH_PIN, LOW);
  shiftOut(DATA_PIN, CLOCK_PIN, LSBFIRST, raw & 0xFF);
  shiftOut(DATA_PIN, CLOCK_PIN, LSBFIRST, (raw >> 8) & 0xFF);
  digitalWrite(LATCH_PIN, HIGH);
}

// ================= RELAY CONTROL =================
void setRelay(int r, bool on) {
  int bit = 15 - (r - 1); // correct mapping

  if (on) state |= (1 << bit);
  else    state &= ~(1 << bit);

  updateRelays();
}

bool getRelay(int r) {
  int bit = 15 - (r - 1);
  return (state & (1 << bit));
}

// ================= SIGNAL =================
int getSignalPercent() {
  int rssi = WiFi.RSSI();
  return constrain(map(rssi, -100, -50, 0, 100), 0, 100);
}

// ================= WEB PAGE =================
void handleRoot() {

  int signal = getSignalPercent();

  String html = R"rawliteral(
  <html>
  <head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body {
      font-family: Helvetica;
      background: #f5f7fb;
      text-align: center;
      margin: 0;
    }

    h2 {
      margin-top: 20px;
    }

    .grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 15px;
      padding: 20px;
    }

    .btn {
      font-size: 32px;
      padding: 25px;
      border-radius: 20px;
      border: none;
      color: white;
      cursor: pointer;
    }

    .on {
      background: #16a34a;
    }

    .off {
      background: #dc2626;
    }
  </style>
  </head>
  <body>
  )rawliteral";

  html += "<h2>Relay Control</h2>";
  html += "<p>Signal: " + String(signal) + "%</p>";
  html += "<div class='grid'>";

  for (int i = 1; i <= 16; i++) {
    bool on = getRelay(i);

    html += "<button class='btn ";
    html += (on ? "on" : "off");
    html += "' onclick=\"toggle(" + String(i) + ")\">R" + String(i) + "</button>";
  }

  html += "</div>";

  html += R"rawliteral(
  <script>
    function toggle(r){
      fetch("/toggle?r=" + r)
      .then(() => location.reload());
    }
  </script>
  </body></html>
  )rawliteral";

  server.send(200, "text/html", html);
}

// ================= TOGGLE =================
void handleToggle() {
  int r = server.arg("r").toInt();

  bool current = getRelay(r);
  setRelay(r, !current);

  server.send(200, "text/plain", "OK");
}

// ================= SETUP =================
void setup() {
  Serial.begin(9600);

  pinMode(LATCH_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(DATA_PIN, OUTPUT);
  pinMode(OE_PIN, OUTPUT);

  // ===== SAFE START =====
  digitalWrite(OE_PIN, HIGH);

  digitalWrite(DATA_PIN, LOW);
  digitalWrite(CLOCK_PIN, LOW);
  digitalWrite(LATCH_PIN, LOW);

  state = 0;
  updateRelays();

  delay(100);

  digitalWrite(OE_PIN, LOW);

  // ===== WIFI =====
  WiFi.softAP(AP_SSID, AP_PASS);

  server.on("/", handleRoot);
  server.on("/toggle", handleToggle);

  server.begin();

  Serial.println("AP Started");
  Serial.println(WiFi.softAPIP());
}

// ================= LOOP =================
void loop() {
  server.handleClient();
}
