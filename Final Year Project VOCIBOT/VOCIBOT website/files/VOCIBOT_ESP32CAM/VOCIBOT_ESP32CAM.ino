// ============================================================
//  VOCIBOT - ESP32-CAM Firmware
//  Streams MJPEG video over Wi-Fi so the mobile app can show
//  live camera feed in an <img> tag. No connection to Arduino.
//
//  Board  : AI-Thinker ESP32-CAM
//  Library: Arduino ESP32 core (install via Board Manager)
//
//  SETUP:
//  1. In Arduino IDE → Tools → Board → "AI Thinker ESP32-CAM"
//  2. Set WIFI_SSID and WIFI_PASSWORD below
//  3. Upload (GPIO0 to GND during upload, remove after)
//  4. Open Serial Monitor at 115200 baud → note the IP address
//  5. Enter that IP in the VOCIBOT app → Camera tab
//
//  ENDPOINTS:
//    http://<ip>/stream   MJPEG live video  (use in <img src=>)
//    http://<ip>/capture  Single JPEG snapshot
//    http://<ip>/ping     Health check → returns "CAM_ONLINE"
// ============================================================

#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ── Wi-Fi ─────────────────────────────────────────────────────
const char* SSID = "YOUR_WIFI_SSID";      // ← change this
const char* PASS = "YOUR_WIFI_PASSWORD";  // ← change this

// ── AI-Thinker ESP32-CAM pin map ─────────────────────────────
#define PWDN_GPIO  32
#define RESET_GPIO -1
#define XCLK_GPIO   0
#define SIOD_GPIO  26
#define SIOC_GPIO  27
#define Y9_GPIO    35
#define Y8_GPIO    34
#define Y7_GPIO    39
#define Y6_GPIO    36
#define Y5_GPIO    21
#define Y4_GPIO    19
#define Y3_GPIO    18
#define Y2_GPIO     5
#define VSYNC_GPIO 25
#define HREF_GPIO  23
#define PCLK_GPIO  22

// ── MJPEG stream boundary ─────────────────────────────────────
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CT =
  "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY =
  "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART =
  "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;

// ─────────────────────────────────────────────────────────────
void initCamera() {
  camera_config_t cfg;
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer   = LEDC_TIMER_0;
  cfg.pin_d0       = Y2_GPIO;
  cfg.pin_d1       = Y3_GPIO;
  cfg.pin_d2       = Y4_GPIO;
  cfg.pin_d3       = Y5_GPIO;
  cfg.pin_d4       = Y6_GPIO;
  cfg.pin_d5       = Y7_GPIO;
  cfg.pin_d6       = Y8_GPIO;
  cfg.pin_d7       = Y9_GPIO;
  cfg.pin_xclk     = XCLK_GPIO;
  cfg.pin_pclk     = PCLK_GPIO;
  cfg.pin_vsync    = VSYNC_GPIO;
  cfg.pin_href     = HREF_GPIO;
  cfg.pin_sscb_sda = SIOD_GPIO;
  cfg.pin_sscb_scl = SIOC_GPIO;
  cfg.pin_pwdn     = PWDN_GPIO;
  cfg.pin_reset    = RESET_GPIO;
  cfg.xclk_freq_hz = 20000000;
  cfg.pixel_format = PIXFORMAT_JPEG;

  // Lower resolution = faster stream on local Wi-Fi
  cfg.frame_size   = FRAMESIZE_VGA;   // 640×480
  cfg.jpeg_quality = 12;              // 0=best, 63=worst
  cfg.fb_count     = 2;

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return;
  }

  // Flip image if camera is mounted upside-down
  sensor_t* s = esp_camera_sensor_get();
  s->set_vflip(s, 0);     // 1 = flip vertical
  s->set_hmirror(s, 0);   // 1 = mirror horizontal
  s->set_brightness(s, 1);
  s->set_saturation(s, 0);

  Serial.println("Camera ready");
}

// ─────────────────────────────────────────────────────────────
//  MJPEG stream handler
// ─────────────────────────────────────────────────────────────
esp_err_t stream_handler(httpd_req_t* req) {
  camera_fb_t* fb = NULL;
  esp_err_t res   = ESP_OK;
  char part_buf[64];

  // CORS — let the browser app load from any origin
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  res = httpd_resp_set_type(req, STREAM_CT);
  if (res != ESP_OK) return res;

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) { Serial.println("Frame capture failed"); res = ESP_FAIL; break; }

    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);
    if (res != ESP_OK) break;
  }
  return res;
}

// ─────────────────────────────────────────────────────────────
//  Single JPEG capture handler
// ─────────────────────────────────────────────────────────────
esp_err_t capture_handler(httpd_req_t* req) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { httpd_resp_send_500(req); return ESP_FAIL; }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=vocibot.jpg");
  httpd_resp_send(req, (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return ESP_OK;
}

// ─────────────────────────────────────────────────────────────
//  Ping / health-check
// ─────────────────────────────────────────────────────────────
esp_err_t ping_handler(httpd_req_t* req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_sendstr(req, "CAM_ONLINE");
  return ESP_OK;
}

// ─────────────────────────────────────────────────────────────
void startServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port    = 80;

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_uri_t stream_uri = { "/stream",  HTTP_GET, stream_handler,  NULL };
    httpd_uri_t capture_uri= { "/capture", HTTP_GET, capture_handler, NULL };
    httpd_uri_t ping_uri   = { "/ping",    HTTP_GET, ping_handler,    NULL };
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    httpd_register_uri_handler(stream_httpd, &capture_uri);
    httpd_register_uri_handler(stream_httpd, &ping_uri);
    Serial.println("HTTP server started");
  }
}

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== VOCIBOT ESP32-CAM ===");

  initCamera();

  WiFi.begin(SSID, PASS);
  Serial.print("Connecting Wi-Fi");
  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 30) {
    delay(500); Serial.print('.'); tries++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nConnected!");
    Serial.print("Camera stream: http://");
    Serial.print(WiFi.localIP());
    Serial.println("/stream");
    Serial.print("Snapshot:      http://");
    Serial.print(WiFi.localIP());
    Serial.println("/capture");
    startServer();
  } else {
    Serial.println("\nWi-Fi failed! Check credentials.");
  }
}

void loop() {
  delay(1000);  // server runs on its own task
}
