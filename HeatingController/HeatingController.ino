
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include "time.h"
/*
   TODO: error Page
   TODO: error JSON
   TODO: error if restart request when active!

   Device controll constants
*/
const int deviceOnePin = 16;
const String deviceOneName = "ch";
const String deviceOneDesc = "Central Heating:";
const int deviceTwoPin = 17;
const String deviceTwoName = "hw";
const String deviceTwoDesc = "Hot Water:";
/*
   Define the pin numbers. Device Pin numbers defined above!
*/
const int activityLed = 4;
const int connectLed = 2;
const int forceAPModeIn = 5;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

/*
   Access Point definitions
*/
const String apPassword = "password001";
IPAddress IP(10, 10, 10, 10);
IPAddress mask = (255, 255, 255, 0);

/*
   Define the web server on port 80
*/
WebServer server(80);
const char *appId = "esp32";

/*
   Define the Preferences in Non Volatile Storage
*/
Preferences preferences;
const String rebootAPFlagName = "rebootAsAP";
const String passwordName = "pw";
const String ssidName = "ssid";

/*
   Define general constants
*/
const unsigned long msPerHalfSecond = 500;
const unsigned long msPerSecond = 1000;
const unsigned long msPerMin = 60 * msPerSecond;
const unsigned long msPerHour = 60 * msPerMin;
const unsigned long msNtpFetchDelay = msPerSecond * 3;
const String undefined = "-undefined-";
const String plainParamName = "plain";
const String trueStr = "true";
const String offStr = "OFF";
const String onStr = "ON";
const String falseStr = "false";
const String emptyStr = "";

/*
   Define Static HTML Elements
*/
const String deviceName = "HeatingController";
const String deviceDesc = "Heating Controller (ESP32)";
const String appJson = "application/json";
const String txtHtml = "text/html";
const String fg = " style=\"color:White;\" ";
const String fgerr = " style=\"color:red;\" ";
const String htmlEnd = "</body></html>";
const String factorySetHtml = "<hr><form " + fg + " action=\"/factorySettings\">"
                              "<h2>Warning:</h2>"
                              "<input type=\"submit\" value=\"Reset to HeatingController to factory settings\"></form>";

/*
   Working storage
*/
unsigned long timeToRestart = 0;
unsigned long activityLedOff = millis();
unsigned long connectLedTime = millis();
unsigned long deviceOneOffset = 0;
unsigned long deviceTwoOffset = 0;

boolean connectFlipFlop = true;
boolean accesspointMode = false;
boolean restartInApMode = false;
boolean displayConfig = false;
boolean htmlMode = true;
String deviceOneState = offStr;
String deviceTwoState = offStr;
int responseCode = 0;

/*
   Define dynamic HTML Strings
*/

String configHtmlHead() {
  if (displayConfig) {
    return "<html>"
           "<head>"
           "<title>AP: Config " + deviceName + "</title>"
           "</head>"
           "<body style=\"background-color:DarkSalmon;\"><h2 " + fg + ">AP: " + deviceDesc + "</h2>";
  } else {
    return "<html>"
           "<head><meta http-equiv=\"refresh\" content=\"10;url=/config\"/>"
           "<title>Config: " + deviceName + "</title>"
           "</head>"
           "<body style=\"background-color:DodgerBlue;\"><h2 " + fg + ">" + deviceDesc + "</h2>"
           "<hr><h3 " + fg + ">Day:Time " + getLocalTime() + "</h3>";
  }
}

String switchHtml(String cm) {
  if (accesspointMode) {
    return "";
  }
  return "<hr><h3 " + fg + ">Switch " + deviceName + " to " + cm + " mode:</h3>"
         "<input type=\"button\" onclick=\"location.href='/switch';\" value=\"Switch to " + cm + " Mode\" />";
}

String messageHtml(String msg) {
  if (msg == "") {
    return "";
  }
  if (responseCode == 0) {
    return "<hr><h3 " + fg + ">" + msg + "</h3>";
  } else {
    return "<hr><h3 " + fgerr + ">Error:" + String(responseCode) + msg + "</h3>";
  }
}

String configHtml(String msg) {
  if (displayConfig) {
    return configHtmlHead() + messageHtml(msg) + statusHtml() + switchHtml("Running") + connectionHtml() + factorySetHtml + htmlEnd;
  }
  return configHtmlHead() + messageHtml(msg) + statusHtml() + switchHtml("Config") + htmlEnd;
}

String connectionHtml() {
  return "<hr><form " + fg + " action=\"/store\">"
         "<h2>Connection:</h2>"
         "<h3>Router Name:</h3><input type=\"text\" name=\"ssid\" value=\"" + readSsid(emptyStr) + "\"><br><br>"
         "<h3>Password:</h3><input type=\"text\" name=\"pw\" value=\"\"><br><br>"
         "<input type=\"submit\" value=\"Send connection data\">"
         "</form>";
}

String statusHtml() {
  return "<hr><table " + fg + ">"
         "<tr>"
         "<th>Device</th><th>Status</th><th>For</th><th>Clear</th></th><th>Boost</th><th>Boost</th>"
         "</tr><tr>"
         "<td>Central Heating:</td>"
         "<td>" + deviceOneState + "</td>"
         "<td>" + calcMinutes(deviceOneOffset) + "</td>" +
         deviceButtonHtml("ch=0", "Off") +
         deviceButtonHtml("ch=60", "1 Hour") +
         deviceButtonHtml("ch=120", "2 Hour") +
         "</tr><tr>"
         "<td>Hot Water</td>"
         "<td>" + deviceTwoState + "</td>"
         "<td>" + calcMinutes(deviceTwoOffset) + "</td>" +
         deviceButtonHtml("hw=0", "Off") +
         deviceButtonHtml("hw=60", "1 Hour") +
         deviceButtonHtml("hw=120", "2 Hour") +
         "</tr>"
         "</table>";
}

String deviceButtonHtml(String devtim, String desc) {
  return "<td><input type=\"button\" onclick=\"location.href='/device?" + devtim + "';\" value=\"" + desc + "\" /></td>";
}

void setup(void) {
  initRestart(false, 0);

  Serial.begin(115200);
  delay(msPerHalfSecond);

  WiFi.disconnect(true);
  pinMode(forceAPModeIn, INPUT_PULLUP);

  pinMode(activityLed, OUTPUT);
  pinMode(connectLed, OUTPUT);
  pinMode(deviceOnePin, OUTPUT);
  pinMode(deviceTwoPin, OUTPUT);

  digitalWrite(activityLed, LOW);

  digitalWrite(deviceOnePin, LOW);
  deviceOneState = offStr;
  digitalWrite(deviceTwoPin, LOW);
  deviceTwoState = offStr;

  Serial.println();
  Serial.print("AP FLAG: ");
  Serial.println(readPreference(rebootAPFlagName, falseStr));
  if (readPreference(rebootAPFlagName, falseStr) == trueStr) {
    accesspointMode = true;
    displayConfig = true;
    initRestart(false, msPerMin * 3);
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
    displayConfig = false;
    Serial.print("SSID:");
    Serial.println(readSsid(undefined));
    WiFi.mode(WIFI_STA);
    WiFi.begin(readSsid(undefined).c_str(), readPreference(passwordName, undefined).c_str());
    WiFi.setAutoConnect(true);
    WiFi.setHostname(appId);
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::SYSTEM_EVENT_STA_GOT_IP);
    WiFi.onEvent(WiFiLostIP, WiFiEvent_t::SYSTEM_EVENT_STA_DISCONNECTED);
    // Wait for connection
    int count = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (count > 30) {
        performRestart(true, "CONFIG");
      }
      count++;
      delay(400);
      digitalWrite(connectLed, connectFlipFlop);
      connectFlipFlop = !connectFlipFlop;
    }

    digitalWrite(connectLed, HIGH);
    Serial.print("Fetching time from NTP:");
    boolean timeNotFetched = true;
    while (timeNotFetched) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        Serial.println("Failed");
        delay(msNtpFetchDelay);
      } else {
        timeNotFetched = 0;
        digitalWrite(connectLed, LOW);
        Serial.println(getLocalTime());
      }
    }
  }

  delay(msPerHalfSecond);
  server.on("/", handleNotFound);
  server.on("/store", handleStoreConfigDataAndRestart);
  server.on("/config", handleConfigHtml);
  server.on("/device", handleDeviceHtml);
  server.on("/switch", handleSwitchHtml);
  server.on("/factorySettings", handleFactorySettings);
  server.onNotFound(handleNotFound);
  server.begin();

  if (accesspointMode) {
    Serial.println("HTTP Access Point started");
  } else {
    Serial.println("HTTP service started");
  }
  digitalWrite(connectLed, LOW);
}

void loop(void) {
  unsigned long ms = millis();
  checkDevice(ms);

  if (timeToRestart != 0) {
    if (ms > timeToRestart) {
      performRestart(restartInApMode, "TIMEOUT");
    }
  }

  if (accesspointMode) {
    if (connectLedTime < ms) {
      digitalWrite(connectLed, connectFlipFlop);
      connectFlipFlop = !connectFlipFlop;
      if (connectFlipFlop) {
        connectLedTime = millis() + 600;
      } else {
        connectLedTime = millis() + 100;
      }
    }
  }

  if (ms > activityLedOff) {
    digitalWrite(activityLed, LOW);
  }

  if (digitalRead(forceAPModeIn) == LOW) {
    performRestart(true, "BUTTON");
  }

  server.handleClient();
}


/*
   ---------------------------------------------------------------------------------
   Handler methods
   ---------------------------------------------------------------------------------
*/
void handleConfigHtml() {
  startTransaction(msPerSecond);
  server.send(200, "text/html", configHtml(""));
}

void handleDeviceHtml() {
  startTransaction(msPerSecond);
  server.send(200, txtHtml, configHtml(processDeviceArgs()));
}

void handleSwitchHtml() {
  startTransaction(msPerSecond);
  displayConfig = !displayConfig;
  if (displayConfig) {
    server.send(200, "text/html", configHtml("Switched to Config Mode"));
  } else {
    server.send(200, "text/html", configHtml("Switched to Running Mode"));
  }
}
/*
  Request Handler Method - used to store data in preferences from http request query parameters
  For example /store?name1=value1&name2=value2
*/
void handleStoreConfigDataAndRestart() {
  startTransaction(msPerHalfSecond);
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
  initRestart(false, msPerSecond);
  server.send(200, txtHtml, configHtml("Data stored: Restarting..."));
}


/*
   Request Handler Method - used to read data in preferences from http request query parameters
   For example /factorySettings
*/
void handleFactorySettings() {
  startTransaction(msPerHalfSecond);
  preferences.begin(appId, false);
  preferences.clear();
  preferences.end();
  initRestart(true, msPerSecond);
  server.send(200, txtHtml, configHtml("Reset to Factory Settings: Restarting..."));
}

void handleNotFound() {
  startTransaction(msPerHalfSecond);
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
/*
   ---------------------------------------------------------------------------------
   Preferences (Non volatile storage) management.
   Simple read of a preference
   Simple write to preferences
   ---------------------------------------------------------------------------------
*/

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

/*
   Read the ssid from preferences
*/
String readSsid(String defaultStr) {
  return readPreference(ssidName, defaultStr);
}


/*
   ---------------------------------------------------------------------------------
   Methods to build JSON Strings
   ---------------------------------------------------------------------------------
*/
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

/*
   ---------------------------------------------------------------------------------
   Utility methoda
   ---------------------------------------------------------------------------------
*/


/*
   struct tm
  {
  int tm_sec;
  int tm_min;
  int tm_hour;
  int tm_mday;
  int tm_mon;
  int tm_year;
  int tm_wday;
  int tm_yday;
  int tm_isdst;
  #ifdef __TM_GMTOFF
  long  __TM_GMTOFF;
  #endif
  #ifdef __TM_ZONE
  const char *__TM_ZONE;
  #endif
  };
*/
String getLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "?: ?:?:?";
  }
  return String(timeinfo.tm_wday) + ": " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec);
}


/*
   Check for a boosted device making sure it is off or on
*/
void checkDevice(unsigned long tim) {
  if (deviceOneOffset > 0) {
    if (deviceOneOffset > tim) {
      digitalWrite(deviceOnePin, HIGH);
      deviceOneState = onStr;
    } else {
      digitalWrite(deviceOnePin, LOW);
      deviceOneState = offStr;
      deviceOneOffset = 0;
    }
  }
  if (deviceTwoOffset > 0) {
    if (deviceTwoOffset > tim) {
      digitalWrite(deviceTwoPin, HIGH);
      deviceTwoState = onStr;
    } else {
      digitalWrite(deviceTwoPin, LOW);
      deviceTwoState = offStr;
      deviceTwoOffset = 0;
    }
  }
}

void initRestart(boolean apMode, unsigned long delayRestart) {
  if (delayRestart == 0) {
    timeToRestart = 0;
    restartInApMode = false;
  } else {
    restartInApMode = apMode;
    timeToRestart = millis() + delayRestart;
  }
}


void performRestart(boolean resInApMode, String desc) {
  if (resInApMode) {
    writePreference(rebootAPFlagName, trueStr);
    Serial.println("RESTART to AP - " + desc);
  } else {
    writePreference(rebootAPFlagName, emptyStr);
    Serial.println("RESTART to STA - " + desc);
  }
  ESP.restart();
}


/*
   Set activity led. Extends the on time so it can be seen.
*/
void startTransaction(unsigned int delayMs) {
  digitalWrite(activityLed, HIGH);
  responseCode = 0;
  activityLedOff = millis() + delayMs;
  if (accesspointMode) {
    timeToRestart = millis() + msPerMin;
  }
}

/*
   Interpret device id and actions from request query parameters.
   Calculates offsets in MS for the device OFF
*/
String processDeviceArgs() {
  for (uint8_t i = 0; i < server.args(); i++) {
    String divName = server.argName(i);
    unsigned int divOffset = millis() + (server.arg(i).toInt() * msPerSecond);
    if (divName == deviceOneName) {
      deviceOneOffset = divOffset;
      Serial.println("Div:" + deviceOneName + "=" + deviceOneOffset);
    }
    if (divName == deviceTwoName) {
      deviceTwoOffset = divOffset;
      Serial.println("Div:" + deviceTwoName + "=" + deviceTwoOffset);
    }
  }
  checkDevice(millis());
  return deviceOneDesc + " " + deviceOneState + ". " + deviceTwoDesc + " " + deviceTwoState + ".";
}

void setErrorData(int code) {
  responseCode = code;
}

String calcMinutes(unsigned long ms) {
  if (ms == 0) {
    return "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;?&nbsp;&nbsp;&nbsp;&nbsp;";
  }
  unsigned long diff = ms - millis();
  if (diff < 0) {
    diff = 0;
  }
  return String(diff / msPerSecond) + " Mins&nbsp;";
}
/*
   ---------------------------------------------------------------------------------
   Events for the HTTP Server when it connects or dis-connects from the router
   ---------------------------------------------------------------------------------
*/
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
