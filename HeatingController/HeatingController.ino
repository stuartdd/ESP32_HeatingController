/*
   Copyright (c) 2015, Majenko Technologies
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification,
   are permitted provided that the following conditions are met:

 * * Redistributions of source code must retain the above copyright notice, this
     list of conditions and the following disclaimer.

 * * Redistributions in binary form must reproduce the above copyright notice, this
     list of conditions and the following disclaimer in the documentation and/or
     other materials provided with the distribution.

 * * Neither the name of Majenko Technologies nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
   ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
   (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
   LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
   ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>

const String ssid = "George";
const String password = "2iggy Stardust";
const char *appId = "esp32";
const boolean smartConfig = false;

WebServer server(80);
Preferences preferences;

const int activityLed = 4;
unsigned long activityLedOff = millis();
boolean connectFlipFlop = true;
const int connectLed = 2;

void setup(void) {
  Serial.begin(115200);
  delay(500);

  WiFi.disconnect(true);
  pinMode(activityLed, OUTPUT);
  pinMode(connectLed, OUTPUT);
  digitalWrite(activityLed, LOW);

  if (smartConfig) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.beginSmartConfig();
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFi.onEvent(WiFiLostIP, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);

    //Wait for SmartConfig packet from mobile
    Serial.println("Waiting for SmartConfig.");
    while (!WiFi.smartConfigDone()) {
      delay(200);
      digitalWrite(connectLed, connectFlipFlop);
      connectFlipFlop = !connectFlipFlop;
    }

    Serial.println("");
    Serial.println("SmartConfig received.");

    //Wait for WiFi to connect to AP
    Serial.println("Waiting for WiFi");
    while (WiFi.status() != WL_CONNECTED) {
      delay(200);
      digitalWrite(connectLed, connectFlipFlop);
      connectFlipFlop = !connectFlipFlop;
    }

  } else {
    WiFi.mode(WIFI_STA);
    WiFi.begin(readPreference("ssid", ssid).c_str(), readPreference("pw", password).c_str());
    WiFi.setAutoConnect(true);
    WiFi.setHostname(appId);

    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFi.onEvent(WiFiLostIP, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);

    // Wait for connection
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      digitalWrite(connectLed, connectFlipFlop);
      connectFlipFlop = !connectFlipFlop;
    }
  }
  delay(500);
  digitalWrite(connectLed, LOW);

  server.on("/", handleRoot);
  server.on("/test.svg", drawGraph);
  server.on("/store", storeData);
  server.on("/fetch", fetchData);
  server.on("/erase", eraseData);
  server.onNotFound(handleNotFound);
  server.begin();

  if (MDNS.begin(appId)) {
    Serial.println("MDNS responder started");
  }
  Serial.println("HTTP server started");
}


void loop(void) {
  unsigned long ms = millis();
  if (activityLedOff < ms) {
    digitalWrite(activityLed, LOW);
  }
  server.handleClient();
}

void setActivityLed(unsigned int delayMs) {
  digitalWrite(activityLed, HIGH);
  activityLedOff = millis() + delayMs;
}

void storeData() {
  setActivityLed(500);
  String message = "Store-->\n";
  preferences.begin(appId, false);
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) != "plain") {
      if (server.arg(i).length() > 0) {
        preferences.putString(server.argName(i).c_str(), server.arg(i));
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
      } else {
        preferences.remove(server.argName(i).c_str());
        message += " " + server.argName(i) + ": null\n";
      }
    }
  }
  preferences.end();
  Serial.print(message);
  server.send(200, "text/plain", message);
}

void fetchData() {
  setActivityLed(500);
  String message = "Fetch-->\n";
  preferences.begin(appId, false);
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) != "plain") {
      String value = preferences.getString(server.argName(i).c_str(), server.arg(i));
      message += " " + server.argName(i) + ": " + value + "\n";
    }
  }
  preferences.end();
  Serial.print(message);
  server.send(200, "text/plain", message);
}

void eraseData() {
  setActivityLed(500);
  String message = "Erase-->\n";
  preferences.begin(appId, false);
  preferences.clear();
  preferences.end();
  Serial.print(message);
  server.send(200, "text/plain", message);
}


String readPreference(String pName, String defaultValue) {
  preferences.begin(appId, false);
  String value = preferences.getString(pName.c_str(), defaultValue);
  preferences.end();
  return value;
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.print(appId);
  Serial.print(" is connected to: ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
  digitalWrite(connectLed, LOW);
}

void WiFiLostIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
  digitalWrite(connectLed, HIGH);
  Serial.print("WiFi lost connection. Reason: ");
  Serial.println(info.disconnected.reason);
}

void handleNotFound() {
  setActivityLed(500);
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

void handleRoot() {
  setActivityLed(500);
  char temp[400];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 400, "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP32 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP32!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <img src=\"/test.svg\" />\
  </body>\
</html>", hr, min % 60, sec % 60);
  server.send(200, "text/html", temp);
}

void drawGraph() {
  String out = "";
  char temp[100];
  out += "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.1\" width=\"400\" height=\"150\">\n";
  out += "<rect width=\"400\" height=\"150\" fill=\"rgb(250, 230, 210)\" stroke-width=\"1\" stroke=\"rgb(0, 0, 0)\" />\n";
  out += "<g stroke=\"black\">\n";
  int y = rand() % 130;
  for (int x = 10; x < 390; x += 10) {
    int y2 = rand() % 130;
    sprintf(temp, "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" stroke-width=\"1\" />\n", x, 140 - y, x + 10, 140 - y2);
    out += temp;
    y = y2;
  }
  out += "</g>\n</svg>\n";

  server.send(200, "image/svg+xml", out);
}
