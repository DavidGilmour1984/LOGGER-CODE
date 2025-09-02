#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

// ================== CAMERA PINOUT (AI Thinker) ==================
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

// ================== WiFi Access Point ==================
const char* ssid = "ESP32-CAM";   // Open AP, no password
WebServer server(80);

// ================== Stream Handler ==================
void handleJPGStream() {
  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (client.connected()) {
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      continue;
    }

    response = "--frame\r\n";
    response += "Content-Type: image/jpeg\r\n\r\n";
    server.sendContent(response);

    client.write(fb->buf, fb->len);
    server.sendContent("\r\n");

    esp_camera_fb_return(fb);
    delay(30); // Adjust frame rate
  }
}

void handleRoot() {
  String html = R"rawliteral(
  <!DOCTYPE html>
  <html>
  <head>
    <title>ESP32-CAM Live Stream</title>
  </head>
  <body>
    <h1>ESP32-CAM Live Stream</h1>
    <img src="/stream" />
  </body>
  </html>
  )rawliteral";
  server.send(200, "text/html", html);
}

// ================== Setup ==================
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
  
  // Recommended: start lower resolution first
  config.frame_size   = FRAMESIZE_VGA;  
  config.jpeg_quality = 12;
  config.fb_count     = 2;

  return (esp_camera_init(&config) == ESP_OK);
}

void setup() {
  Serial.begin(115200);

  // Start AP mode
  WiFi.softAP(ssid);
  Serial.println("Access Point started");
  Serial.print("Connect to SSID: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.softAPIP());

  if (!initCamera()) {
    Serial.println("Camera init failed");
    while (1);
  }

  server.on("/", handleRoot);
  server.on("/stream", HTTP_GET, handleJPGStream);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
