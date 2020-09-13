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
uint8_t fg_red = 0xff;
uint8_t fg_green = 0xff;
uint8_t fg_blue = 0xff;
uint8_t bg_red = 0x00;
uint8_t bg_green = 0x00;
uint8_t bg_blue = 0x00;

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
"    <p>Foreground<p>\n"
"    <label for=\"fg_red\">Red</label>\n"
"    <input type=\"text\" id=\"fg_red\" name=\"fg_red\" value=\"{fg_red}\" /><br />\n"
"    <label for=\"fg_green\">Green</label>\n"
"    <input type=\"text\" id=\"fg_green\" name=\"fg_green\" value=\"{fg_green}\" /><br />\n"
"    <label for=\"fg_blue\">Blue</label>\n"
"    <input type=\"text\" id=\"fg_blue\" name=\"fg_blue\" value=\"{fg_blue}\" /><br />\n"
"    <p>Background<p>\n"
"    <label for=\"bg_red\">Red</label>\n"
"    <input type=\"text\" id=\"bg_red\" name=\"bg_red\" value=\"{bg_red}\" /><br />\n"
"    <label for=\"bg_green\">Green</label>\n"
"    <input type=\"text\" id=\"bg_green\" name=\"bg_green\" value=\"{bg_green}\" /><br />\n"
"    <label for=\"bg_blue\">Blue</label>\n"
"    <input type=\"text\" id=\"bg_blue\" name=\"bg_blue\" value=\"{bg_blue}\" /><br />\n"
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
  html.replace("{value}", text);
  html.replace("{fg_red}", String(fg_red));
  html.replace("{fg_green}", String(fg_green));
  html.replace("{fg_blue}", String(fg_blue));
  html.replace("{bg_red}", String(bg_red));
  html.replace("{bg_green}", String(bg_green));
  html.replace("{bg_blue}", String(bg_blue));
  html.replace("{ip_addr}", ip_addr());
  server.send(200, "text/html", html);
}

void handleRootPost() {
  if (server.args() == 0) {
    return server.send(500, "text/plain", "BAD ARGS");
  }
  text = server.arg(0);
  fg_red = server.arg(1).toInt();
  fg_green = server.arg(2).toInt();
  fg_blue = server.arg(3).toInt();
  bg_red = server.arg(4).toInt();
  bg_green = server.arg(5).toInt();
  bg_blue = server.arg(6).toInt();
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

// the setup() method runs once, when the sketch starts
void setup() {
  // initialize the digital pin as an output.

  Serial.begin(115200);

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

  matrix.addLayer(&backgroundLayer); 
  matrix.addLayer(&scrollingLayer); 
  matrix.addLayer(&indexedLayer); 
  matrix.begin();

  matrix.setBrightness(defaultBrightness);

  scrollingLayer.setOffsetFromTop(defaultScrollOffset);

  backgroundLayer.enableColorCorrection(true);
}

// the loop() method runs over and over again,
// as long as the board has power
void loop() {
    int i, j;
    unsigned long time_now_ms;

    // clear screen
    backgroundLayer.fillScreen({bg_red, bg_green, bg_blue});
    backgroundLayer.swapBuffers();

    // indexedLayer.setColor({0xff, 0xff, 0xff});
    // indexedLayer.setFont(font3x5);
    // indexedLayer.drawString(1, 1, 1, "Liberty");
    // indexedLayer.swapBuffers();

    time_now_ms = millis();
    if (time_now_ms > (text_start_ms + text_duration_ms)) {
      scrollingLayer.setColor({fg_red, fg_green, fg_blue});
      scrollingLayer.setMode(wrapForward);
      scrollingLayer.setSpeed(scroll_pixels_per_second);
      scrollingLayer.setFont(font6x10);
      scrollingLayer.start(text.c_str(), 1);
      text_start_ms = time_now_ms;
      text_duration_ms = ((text.length() * 7 + 48) * 1000) / scroll_pixels_per_second;
    }

    server.handleClient();
}
