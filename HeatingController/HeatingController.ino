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


const char *appId = "esp32";
const String rebootAPFlagName = "rebootAsAP";
const String passwordName = "pw";
const String ssidName = "ssid";
const String apPassword = "password001";
const String deviceName = "HeatingController";
const String undefined = "-undefined-";
const String plainParamName = "plain";
const String trueStr = "true";
const String falseStr = "false";
const String emptyStr = "";
const String appJson = "application/json";
const int activityLed = 4;
const int connectLed = 2;
const int forceAPModeIn = 5;

const String configHtmlHead = "<html><body><h2>" + deviceName + "</h2>";
const String formStoreHtml = "<form action=\"/store\"><h2>Connection:</h2>";
const String ssidHtml_1 = "Router Name:<br><input type=\"text\" name=\"" + ssidName + "\" value=\"";
const String ssidHtml_2 = "\"><br><br>";
const String pwHtml = "Password:<br><input type=\"text\" name=\"" + passwordName + "\" value=\"\"><br><br>";
const String submitStoreHtml = "<input type=\"submit\" value=\"Send connection data to " + deviceName + "\">";
const String formEndHtml = "</form><hr>";
const String htmlEnd = "</body></html>";
const String fsFormHtml = "<form action=\"/factorySettings\"><h2>Warning:</h2>";
const String submitFsHtml = "<input type=\"submit\" value=\"Reset to " + deviceName + " to factory settings\">";


IPAddress IP(192, 168, 4, 15);
IPAddress mask = (255, 255, 255, 0);
WebServer server(80);
Preferences preferences;

unsigned long activityLedOff = millis();
unsigned long connectLedOff = millis();
unsigned long timeToRestart = 0;

boolean connectFlipFlop = true;
boolean accesspointMode = false;

void handleConfig() {
  setActivityLed(500);
  server.send(200, "text/html", (configHtmlHead + formStoreHtml + ssidHtml_1 + readSsid(emptyStr) + ssidHtml_2 + pwHtml + submitStoreHtml + formEndHtml + fsFormHtml + submitFsHtml + formEndHtml + htmlEnd).c_str());
}

void setup(void) {
  timeToRestart = 0;
  Serial.begin(115200);
  delay(500);

  WiFi.disconnect(true);
  pinMode(activityLed, OUTPUT);
  pinMode(connectLed, OUTPUT);
  pinMode(forceAPModeIn, INPUT_PULLUP);

  digitalWrite(activityLed, LOW);

  Serial.println();
  Serial.print("AP FLAG: ");
  Serial.println(readPreference(rebootAPFlagName, falseStr));
  if (readPreference(rebootAPFlagName, falseStr) == trueStr) {
    accesspointMode = true;
    timeToRestart = millis() + 60000;
    writePreference(rebootAPFlagName, emptyStr);
    Serial.println("AP FLAG: Cleared");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(deviceName.c_str(), apPassword.c_str());
    WiFi.softAPConfig(IP, IP, mask);
    server.begin();
    Serial.print("Access Point ");
    Serial.print(deviceName);
    Serial.println(" Started.");
    Serial.print("IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("MAC:");
    Serial.println(WiFi.softAPmacAddress());
  } else {
    accesspointMode = false;
    Serial.print("SSID:");
    Serial.println(readSsid(undefined));
    Serial.print("PW:");
    Serial.println(readPreference(passwordName, undefined));
    WiFi.mode(WIFI_STA);
    WiFi.begin(readSsid(undefined).c_str(), readPreference(passwordName, undefined).c_str());
    WiFi.setAutoConnect(true);
    WiFi.setHostname(appId);
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFi.onEvent(WiFiLostIP, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);
    // Wait for connection
    int count = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (count > 20) {
        performRestart(true, "CONFIG");
      }
      count++;
      delay(400);
      digitalWrite(connectLed, connectFlipFlop);
      connectFlipFlop = !connectFlipFlop;
    }
  }
  delay(500);
  digitalWrite(connectLed, LOW);

  server.on("/", handleNotFound);
  server.on("/store", storeConfigDataRestart);
  server.on("/config", handleConfig);
  server.on("/factorySettings", factorySettings);
  if (!accesspointMode) {
    server.on("/fetch", fetchData);
  }
  server.onNotFound(handleNotFound);
  server.begin();

  //  if (MDNS.begin(appId)) {
  //    Serial.println("MDNS responder started");
  //  }
  if (accesspointMode) {
    Serial.println("HTTP Access Point started");
  } else {
    Serial.println("HTTP service started");
  }
}


void loop(void) {
  unsigned long ms = millis();
  checkRestart(ms);
  if (accesspointMode) {
    if (connectLedOff < ms) {
      digitalWrite(connectLed, connectFlipFlop);
      connectFlipFlop = !connectFlipFlop;
      connectLedOff = millis() + 100;
    }
  } else {
    if (activityLedOff < ms) {
      digitalWrite(activityLed, LOW);
    }
  }
  server.handleClient();
}

void checkRestart(unsigned long tim) {
  if (timeToRestart > 0) {
    if (timeToRestart < tim) {
      performRestart(false, "TIMEOUT");
    }
  }
  if (digitalRead(forceAPModeIn) == LOW) {
    performRestart(true, "BUTTON");
  }
}

void performRestart(boolean apMode, String desc) {
  if (apMode) {
    writePreference(rebootAPFlagName, trueStr);
  } else {
    writePreference(rebootAPFlagName, emptyStr);
  }
  Serial.println("RESTART-" + desc);
  ESP.restart();
}

void setActivityLed(unsigned int delayMs) {
  digitalWrite(activityLed, HIGH);
  activityLedOff = millis() + delayMs;
}

void storeConfigDataRestart() {
  setActivityLed(500);
  String message = jsonNVP("action", "store") + ",";
  preferences.begin(appId, false);
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) != plainParamName) {
      if (server.arg(i).length() > 0) {
        preferences.putString(server.argName(i).c_str(), server.arg(i));
        if (server.argName(i) != passwordName) {
          message += jsonNVP(server.argName(i), server.arg(i)) + ",";
        } else {
          message += jsonNVP(server.argName(i), "*") + ",";
        }
      } else {
        preferences.remove(server.argName(i).c_str());
        message += jsonNVP(server.argName(i), "") + ",";
      }
    }
  }
  preferences.end();
  timeToRestart = millis() + 500;
  server.send(200, appJson, jsonResponse(message, "OK"));
}

void fetchData() {
  setActivityLed(500);
  String message = "Fetch-->\n";
  preferences.begin(appId, false);
  for (uint8_t i = 0; i < server.args(); i++) {
    if ((server.argName(i) != plainParamName) && (server.argName(i) != passwordName)) {
      String value = preferences.getString(server.argName(i).c_str(), server.arg(i));
      message += " " + server.argName(i) + ": " + value + "\n";
    }
  }
  preferences.end();
  Serial.print(message);
  server.send(200, "text/plain", message);
}

void factorySettings() {
  setActivityLed(500);
  if (accesspointMode) {
    preferences.begin(appId, false);
    preferences.clear();
    preferences.end();
    server.send(200, appJson, actionResponse("Erase", "OK", "Factory Settings"));
    timeToRestart = millis() + 500;
  } else {
    server.send(401, appJson, actionResponse("Error", "not authorised", "Local (AP) mode only"));
  }
}


String readPreference(String pName, String defaultValue) {
  preferences.begin(appId, false);
  String value = preferences.getString(pName.c_str(), defaultValue);
  preferences.end();
  return value;
}

void writePreference(String pName, String val) {
  preferences.begin(appId, false);
  if (val.length() > 0) {
    preferences.putString(pName.c_str(), val);
  } else {
    preferences.remove(pName.c_str());
  }
  preferences.end();
}

String readSsid(String defaultStr) {
  return readPreference(ssidName, defaultStr);
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
  Serial.print(appId);
  Serial.print(" is connected to: ");
  Serial.println(readSsid(undefined));
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

String actionResponse(String actionStr, String valueStr, String noteStr) {
  return "{" + jsonNVP("device", deviceName) + "," + jsonNVP("action", actionStr) + "," + jsonNVP("value", valueStr) + "," + jsonNVP("note", noteStr) + "}";
}

String jsonResponse(String dataStr, String statusStr) {
  return "{" + jsonNVP("device", deviceName) + "," + dataStr + jsonNVP("status", statusStr) + "}";
}

String jsonNVP(String nameStr, String valueStr) {
  if (valueStr.length() == 0) {
    return "\"" + nameStr + "\":null";
  }
  return "\"" + nameStr + "\":\"" + valueStr + "\"";
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
