/*
 * Copyright 2020 Matt Liberty
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <MatrixHardware_ESP32_V0.h>
#include <SmartMatrix3.h>
#include "wifi_credentials.h"
#include "colorwheel.c"
#include "gimpbitmap.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>

#define MDNS_NAME "led_panel"

unsigned long text_duration_ms = 3000;  // Adjusted automatically depending upon text
unsigned long text_start_ms = 0;
uint32_t fg_color = 0x00ffffff;
uint32_t bg_color = 0x00000000;

String text("http://" MDNS_NAME ".local/");
unsigned char scroll_pixels_per_second = 30;

WebServer server(80);

#define COLOR_DEPTH 24                  // known working: 24, 48 - If the sketch uses type `rgb24` directly, COLOR_DEPTH must be 24
const uint8_t kMatrixWidth = 32;        // known working: 32, 64, 96, 128
const uint8_t kMatrixHeight = 16;       // known working: 16, 32, 48, 64
const uint8_t kRefreshDepth = 36;       // known working: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save memory, more to keep from dropping frames and automatically lowering refresh rate
const uint8_t kPanelType = SMARTMATRIX_HUB75_16ROW_MOD8SCAN;   // use SMARTMATRIX_HUB75_16ROW_MOD8SCAN for common 16x32 panels
const uint8_t kMatrixOptions = (SMARTMATRIX_OPTIONS_NONE);      // see http://docs.pixelmatix.com/SmartMatrix for options
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);
SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);
SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);
SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);

const int defaultBrightnessPercent = 50;  // 0: off, 100: full brightness
const int defaultBrightness = (defaultBrightnessPercent*255)/100;
const int defaultScrollOffset = 3;


void drawBitmap(int16_t x, int16_t y, const gimp32x32bitmap* bitmap) {
  for(unsigned int i=0; i < bitmap->height; i++) {
    for(unsigned int j=0; j < bitmap->width; j++) {
      rgb24 pixel = { bitmap->pixel_data[(i*bitmap->width + j)*3 + 0],
                      bitmap->pixel_data[(i*bitmap->width + j)*3 + 1],
                      bitmap->pixel_data[(i*bitmap->width + j)*3 + 2] };

      backgroundLayer.drawPixel(x + j, y + i, pixel);
    }
  }
}

const char * root_page = 
"<html>\n"
"<head>\n"
"  <title>LED Panel</title>\n"
"</head>\n"
"<body>\n"
"  <h1>LED Panel Control</h1>\n"
"  <form action=\"/\" method=\"post\">\n"
"    <label for=\"text\">Text</label>\n"
"    <input type=\"text\" id=\"text\" name=\"text\" value=\"{value}\" /><br />\n"
"    <label for=\"fg_color\">Foreground</label>\n"
"    <input type=\"color\" id=\"fg_color\" name=\"fg_color\" value=\"{fg_color}\"><br />\n"
"    <label for=\"bg_color\">Background</label>\n"
"    <input type=\"color\" id=\"bg_color\" name=\"bg_color\" value=\"{bg_color}\"><br />\n"
"    <input type=\"submit\" value=\"Submit\">\n"
"  </form>\n"
"  <p>http://{ip_addr}/</p>\n"
"</body>\n"
"</html>\n";


String ip_addr() {
  IPAddress ip = WiFi.localIP();
  return String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
}

void handleRootGet() {
  String html = root_page;
  char color[32];
  html.replace("{value}", text);
  snprintf(color, sizeof(color), "#%06x", fg_color & 0xffffff);
  html.replace("{fg_color}", color);
  snprintf(color, sizeof(color), "#%06x", bg_color & 0xffffff);
  html.replace("{bg_color}", color);
  html.replace("{ip_addr}", ip_addr());
  server.send(200, "text/html", html);
}

void handleRootPost() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  text = server.arg(0);
  fg_color = (uint32_t) strtol(server.arg(1).c_str() + 1, 0, 16);
  bg_color = (uint32_t) strtol(server.arg(2).c_str() + 1, 0, 16);
  handleRootGet();
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup_wifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(wifi_ssid);
  Serial.print("IP address: ");
  Serial.println(ip_addr());

  if (MDNS.begin("led_panel")) {
    Serial.println("MDNS responder started");
  }

  server.on("/", HTTP_GET, handleRootGet);
  server.on("/", HTTP_POST, handleRootPost);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void setup_led_panel() {
  matrix.addLayer(&backgroundLayer); 
  matrix.addLayer(&scrollingLayer); 
  matrix.addLayer(&indexedLayer); 
  matrix.begin();

  matrix.setBrightness(defaultBrightness);

  scrollingLayer.setOffsetFromTop(defaultScrollOffset);

  backgroundLayer.enableColorCorrection(true);
  backgroundLayer.fillScreen({0x00, 0x00, 0x00});
  backgroundLayer.swapBuffers();
}

// the setup() method runs once, when the sketch starts
void setup() {
  Serial.begin(115200);
  setup_led_panel();
  setup_wifi();
}

// the loop() method runs over and over again,
// as long as the board has power
void loop() {
    int i, j;
    unsigned long time_now_ms;

    // clear screen
    backgroundLayer.fillScreen({(bg_color >> 16) & 0xff, (bg_color >> 8) & 0xff, (bg_color >> 0) & 0xff});
    backgroundLayer.swapBuffers();

    // indexedLayer.setColor({0xff, 0xff, 0xff});
    // indexedLayer.setFont(font3x5);
    // indexedLayer.drawString(1, 1, 1, "Liberty");
    // indexedLayer.swapBuffers();

    time_now_ms = millis();
    if (time_now_ms > (text_start_ms + text_duration_ms)) {
      scrollingLayer.setColor({(fg_color >> 16) & 0xff, (fg_color >> 8) & 0xff, (fg_color >> 0) & 0xff});
      scrollingLayer.setMode(wrapForward);
      scrollingLayer.setSpeed(scroll_pixels_per_second);
      scrollingLayer.setFont(font6x10);
      scrollingLayer.start(text.c_str(), 1);
      text_start_ms = time_now_ms;
      text_duration_ms = ((text.length() * 7 + 48) * 1000) / scroll_pixels_per_second;
    }

    server.handleClient();
}
