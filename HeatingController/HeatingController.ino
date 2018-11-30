
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include "time.h"
/*
   TODO: error Page
   TODO: error JSON
   TODO: error if restart request when active!

   Device controll constants
*/
const String deviceName = "HeatingController";
const String deviceDesc = "ESP32 Heating Controller";

const char deviceDataChar = '@'; // Char '@' has the most 0 lsb(s) Hex 01000000 allows for 6 devices
const int deviceCount = 3; // Max is 6
const String deviceDescList[] = {"Central Heating", "Hot Water", "Light"};
const String deviceShortList[] = {"Heating", "Water", "Light"};
const String deviceIdList[] = {"ch", "hw", "li"};
const int devicePinList[] = {16, 17, 18};
const int deviceOnBitList[] = {1, 2, 4, 8, 16, 32};

/*
   Define the pin numbers. Device Pin numbers defined above!
*/
const int activityLed = 4;
const int connectLed = 2;
const int forceAPModeIn = 5;

const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;

const int slots = 96;
const int daysInWeek = 7;
/*
   Access Point definitions
*/
const String apPassword = "password001";
IPAddress IP(192, 1, 1, 1);
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
const unsigned long msPerDay = msPerHour * 24;
const unsigned long msNtpFetchDelay = msPerSecond * 1;

const unsigned long minsPerSlot = 15;
const unsigned long minsPerHour = 60;
const unsigned long hoursPerDay = 24;
const unsigned long minsPerDay = minsPerHour * hoursPerDay;
const unsigned long secondsPerMinute = 60 ;
const unsigned long secondsPerHour = 60 * secondsPerMinute;
const unsigned long secondsPerDay = hoursPerDay * secondsPerHour;

const unsigned long msCheckDeviceDelay = msPerSecond * 5;


const String undefined = "-undefined-";
const String plainParamName = "plain";
const String trueStr = "true";
const String offStr = "OFF";
const String onStr = "ON";
const String autoStr = "AUTO";
const String falseStr = "false";
const String emptyStr = "";

/*
   Define Static HTML Elements
*/
const int daySun = 0;
const int dayMon = 1;
const int dayTue = 2;
const int dayWed = 3;
const int dayThu = 4;
const int dayFri = 5;
const int daySat = 6;

const String days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
const String daysFull[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
const String appJson = "application/json";
const String txtHtml = "text/html";
const String fontBold = " style=\"font-weight:bold;text-decoration:underline\" ";
const String fg = " style=\"color:White;\" ";
const String fgt = " style=\"color:White;border-spacing: 5px 5px;\" ";
const String bgp = " style=\"background-color:DarkSalmon;\" ";
const String boostedMessageHtml = " is <span" + bgp + ">&nbsp;BOOSTED&nbsp;</span> for ";
const String boostedHeadHtml = "<hr><h2" + fg + ">Boost (Overrides the schedule):</h2><table" + fgt + ">"
                               "<tr>"
                               "<th>Device</th><th>Now</th><th>For</th><th>Boost</th><th>Boost</th><th>Boost</th><th>Until</th>"
                               "</tr>";
const String controlHeadHtml = "<hr><h2" + fg + ">Control (ON/OFF Overrides everything):</h2><table" + fgt + ">"
                               "<tr>"
                               "<th>Device</th><th>Status</th><th>Schedule</th><th>Always</th><th>Always</th><th>Current Status</th>"
                               "</tr>";
const String scheduleHeadHtml = "<hr><h2" + fg + ">Schedule (Select a day to change):</h2> " +
                                "<table" + fgt + ">";
const String htmlEnd = "</body></html>";
const String resetHtml = "<hr><h3" + fg + ">Reset " + deviceName + ": </h3>"
                         " <input type=\"button\" onclick=\"location.href='/resetAlert';\" value=\"Reset Now\"/>";
const String factorySetHtml = "<hr><h3" + fg + ">Warning:</h3>"
                              "<input type=\"button\" onclick=\"location.href='/factorySettingsAlert';\" value=\"Reset to " + deviceName + " to factory settings\" />";

/*
   Working storage
*/
unsigned long timeToRestart = 0;
unsigned long timeToCheckDevice = 0;
unsigned long timeToCheckMinutes = 0;


unsigned long activityLedOff = millis();
unsigned long connectLedTime = millis();
unsigned long secondsReference = 0;

boolean connectFlipFlop = true;
boolean accesspointMode = false;
boolean restartInApMode = false;
boolean displayConfig = false;

unsigned long deviceOffsetMins[deviceCount];
String deviceState[deviceCount];
String deviceSlotsList[daysInWeek];
String controlStatusList[deviceCount];

/*
   Define dynamic HTML Strings
*/
String configHtml(String msg) {
  if (displayConfig) {
    return configHtmlHead(false) + controlHtml() + messageHtml(msg) + boostHtml() + switchHtml("Running") + connectionHtml() + resetHtml + factorySetHtml + setTimeHtml() + htmlEnd;
  }
  return configHtmlHead(true) + controlHtml() + messageHtml(msg) + boostHtml() + scheduleDayHtml() + switchHtml("Config") + htmlEnd;
}

String configHtmlHead(boolean refresh) {
  String m = "";
  for (int dev = 0; dev < deviceCount; dev++) {
    if (isStatusAuto(dev)) {
      if (deviceOffsetMins[dev] > 0) {
        m += "<h3 " + fg + " >" + deviceDescList[dev] + boostedMessageHtml + calcMinutesToGo(deviceOffsetMins[dev]) + ".&nbsp;" + boostButtonHtml(deviceIdList[dev] + "=0", "Remove BOOST") + "</h3>";
      }
    } else {
      m += "<h3 " + fg + " >" + deviceDescList[dev] + " is <span " + bgp + ">&nbsp;" + controlStatusList[dev] + "&nbsp;</span></h3> ";
    }
  }

  if (displayConfig) {
    return "<html><head><title>Config: " + deviceName + " </title> </head> "
           "<body style=\"background-color:DarkSalmon;\"><h1 " + fg + ">Config: " + deviceDesc + "</h1>" + m;
  } else {
    String refreshStr = "";
    if (refresh) {
      refreshStr = "<meta http-equiv=\"refresh\" content=\"10;url=/config\"/>";
    }
    return "<html><head>" + refreshStr + "<title>Main:" + deviceName + "</title></head>"
           "<body style=\"background-color:DodgerBlue;\"><h2 " + fg + ">" + deviceDesc + "</h2>"
           "<hr><h2 " + fg + ">Date: " + String(getLocalTime()) + " Day[" + String(dayFromBase()) + " " + slotToTime(getSlotForMinsOfDay()) + "]</h2>" + m;
  }
}

String controlHtml() {
  String ht = controlHeadHtml;
  for (int dev = 0; dev < deviceCount; dev++) {
    ht += "<tr>"
          "<td>" + deviceShortList[dev] + "</td>" +
          "<td align=\"center\" " + (isStatusAuto(dev) ? "" : bgp) + ">" + controlStatusList[dev] + "</td>" +
          controlButtonHtml(dev , autoStr, "AUTO") +
          controlButtonHtml(dev , onStr, "ON") +
          controlButtonHtml(dev , offStr, "OFF") + "</td><td>" + getNextEvent(dev) + "</td>"
          "</tr>";
  }
  return ht + "</table>";
}

String controlButtonHtml(int dev, String action, String desc) {
  return "<td><input type=\"button\" onclick=\"location.href='/control?" + deviceIdList[dev] + "=" + action + "';\" value=\"" + desc + "\" />";
}

String switchHtml(String cm) {
  if (accesspointMode) {
    return "";
  }
  return "<hr><h3 " + fg + ">Switch " + deviceName + " to " + cm + " mode:</h3>"
         "<input type=\"button\" onclick=\"location.href='/switch';\" value=\"Switch to " + cm + " Mode\" />";
}

String alertHtml(String msg, String b1, String b2, String a1, String a2) {
  String b1Html = "";
  if (b1 != "") {
    b1Html = "<input type=\"button\" onclick=\"location.href='/" + a1 + "';\" value=\"" + b1 + "\" /></td>";
  }
  String b2Html = "";
  if (b2 != "") {
    b2Html = "<input type=\"button\" onclick=\"location.href='/" + a2 + "';\" value=\"" + b2 + "\" /></td>";
  }
  return configHtmlHead(false) + messageHtml(msg) + b1Html + b2Html + htmlEnd;
}

String messageHtml(String msg) {
  if (msg == "") {
    return "";
  }
  return "<hr><h3 " + fg + ">Message:" + msg + "</h3>";
}


String connectionHtml() {
  return "<hr><form " + fg + " action=\"/store\">"
         "<h2>Connection:</h2>"
         "<h3>Router Name:</h3><input type=\"text\" name=\"ssid\" value=\"" + readSsid(emptyStr) + "\"><br>"
         "<h3>Password:</h3><input type=\"text\" name=\"pw\" value=\"\"><br><br>"
         "<input type=\"submit\" value=\"Send connection data\">"
         "</form>";
}

String boostHtml() {
  String ht = boostedHeadHtml;
  for (int dev = 0; dev < deviceCount; dev++) {
    if (isStatusAuto(dev)) {
      ht += "<tr>"
            "<td>" + deviceShortList[dev] + "</td>"
            "<td>" + deviceState[dev] + "</td>"
            "<td>" + calcMinutesToGo(deviceOffsetMins[dev]) + "&nbsp;</td>" +
            boostButtonHtml(deviceIdList[dev] + "=60", "1 Hour") +
            boostButtonHtml(deviceIdList[dev] + "=120", "2 Hour") +
            boostButtonHtml(deviceIdList[dev] + "=240", "4 Hour") +
            boostButtonHtml(deviceIdList[dev] + "=H24", "Midnight") +
            "</tr>";
    } else {
      ht += "<tr><td align=\"center\"" + bgp + " colspan=\"6\">" + deviceDescList[dev] + " is " + controlStatusList[dev] + ". To BOOST it set it to --></td>" + controlButtonHtml(dev , autoStr, "AUTO") + "</tr>";
    }
  }
  return ht + "</table>";
}

String boostButtonHtml(String deviceTime, String desc) {
  return "<td><input type=\"button\" onclick=\"location.href='/boost?" + deviceTime + "';\" value=\"" + desc + "\" /></td>";
}



String scheduleDayHtml() {
  String ht = scheduleHeadHtml;
  for (int dev = 0; dev < deviceCount; dev++) {
    if (isStatusAuto(dev)) {
      ht += "<tr><td>" + deviceShortList[dev] + "</td>" + scheduleDayButtonRowHtml(dev) + "</tr>";
    } else {
      ht += "<tr><td align=\"center\" " + bgp + " colspan=\"7\">" + deviceDescList[dev] + " is " + controlStatusList[dev] + ". To SCHEDULE it set it to --></td>" + controlButtonHtml(dev , autoStr, "AUTO") + "</tr>";
    }
  }
  return ht + "</table>";
}

String scheduleDayButtonRowHtml(int device) {
  return scheduleDayButtonHtml(daySun, device) +
         scheduleDayButtonHtml(dayMon, device) +
         scheduleDayButtonHtml(dayTue, device) +
         scheduleDayButtonHtml(dayWed, device) +
         scheduleDayButtonHtml(dayThu, device) +
         scheduleDayButtonHtml(dayFri, device) +
         scheduleDayButtonHtml(daySat, device);
}

String scheduleDayButtonHtml(int day, int device) {
  String s = "";
  if (day == dayFromBase()) {
    s = fontBold;
  }
  return "<td><input " + s + " type=\"button\" onclick=\"location.href='/dispDay?day=" + String(day) + "&dev=" + String(device) + "';\" value=\"" + days[day] + "\" /></td>";
}

String setTimeHtml() {
  String s = "<hr><form " + fg + " action=\"/settime\"><h2>Override Timer:</h2>";
  return s + dowDropDownHtml() + hoursDropDownHtml() + "<br><input type=\"submit\" value=\"Set Time\"></form>";
}

String dowDropDownHtml() {
  String s = "<select>";
  for (int i = 0; i< daysInWeek;i++) {
    s+="<option value=\"D"+days[i]+"\">"+daysFull[i]+"</option>";
  }
  return s + "</select>";
}
String hoursDropDownHtml() {
  String s = "<select>";
  for (int i = 0; i< hoursPerDay;i++) {
    s+="<option value=\"H"+String(i)+"\">Hour:"+String(i)+"</option>";
  }
  return s + "</select>";
}

String schedulePageHtml(int day, int dev) {
  return configHtmlHead(false) + scheduleDayHtml() + scheduleTimesHtml(day, dev) + htmlEnd;
}

String scheduleTimesHtml(int day, int dev) {
  return "<hr><h2 " + fg + " >Times for " + deviceDescList[dev] + " on " + daysFull[day] + "</h2>" +
         "<table border=\"1\" width=\"400px\"" + fg + ">" +
         scheduleTimesCbHtmlAll(day, dev) +
         "</table>" +
         "<br><input type=\"button\" onclick=\"location.href='/config';\" value=\"DONE!\"/>";
}

String scheduleTimesCbHtmlAll(int day, int dev) {
  String ofs = "";
  int offset = 0;
  for (uint8_t y = 0; y < hoursPerDay; y++) {
    ofs += "<tr>";
    for (uint8_t x = 0; x < 4; x++) {
      ofs += scheduleTimesTimeCbHtml(offset, day, dev);
      offset++;
    }
    ofs += "</tr>";
  }
  return ofs;
}

String scheduleTimesTimeCbHtml(int slot, int day, int dev) {
  String s = "<td>";
  if (isDeviceOn(slot, day, dev)) {
    s = "<td " + bgp + ">";
  }
  return s + "<input type=\"checkbox\" " + isSlotChecked(slot, day, dev) + " onclick=\"location.href='/setTime?ofs=" + slot + "&day=" + day + "&dev=" + dev + "'\">" + slotToTime(slot) + "</td>";
}


void setup(void) {

  initRestart(false, 0);
  Serial.begin(115200);
  delay(msPerHalfSecond);

  WiFi.disconnect(true);
  pinMode(forceAPModeIn, INPUT_PULLUP);

  pinMode(activityLed, OUTPUT);
  digitalWrite(activityLed, LOW);
  pinMode(connectLed, OUTPUT);

  for (int dev = 0; dev < deviceCount; dev++) {
    deviceState[dev] = offStr;
    deviceOffsetMins[dev] = 0;
    controlStatusList[dev] = readPreference("CT" + String(dev), autoStr);
    pinMode(devicePinList[dev], OUTPUT);
  }

  for (int day = 0; day < daysInWeek; day++) {
    String sl = readPreference("SL" + String(day), "");
    if (sl == "") {
      Serial.println("Slot '" + days[day] + "' Created");
      for (int i = 0; i < slots; i++) {
        sl += deviceDataChar;
      }
    }
    deviceSlotsList[day] = sl;
  }

  Serial.println("Slots defined:");
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

    Serial.print("Fetching time from NTP:");
    boolean timeNotFetched = true;
    int timeNotFetchedCount = 0;
    while (timeNotFetched) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      struct tm timeinfo;
      digitalWrite(connectLed, HIGH);
      if (!getLocalTime(&timeinfo)) {
        digitalWrite(connectLed, LOW);
        delay(msNtpFetchDelay);
        digitalWrite(connectLed, HIGH);
        secondsReference = 0;
        Serial.println("Failed");
        timeNotFetchedCount++;
        if (timeNotFetchedCount > 4) {
          performRestart(false, "NTP FAILED");
        }
      } else {
        timeNotFetched = 0;
        secondsReference = calcBaseSecondsFromLocalTime();
        timeToCheckDevice =  millis() + msCheckDeviceDelay;
        Serial.println(getLocalTime());
      }
    }
  }
  delay(msPerHalfSecond);
  server.on("/", handleNotFound);
  server.on("/store", handleStoreConfigDataAndRestart);
  server.on("/config", handleConfigHtml);
  server.on("/dispDay", handleDispDayHtml);
  server.on("/setTime", handleSetTimeHtml);
  server.on("/control", handleControlHtml);
  server.on("/boost", handleBoostHtml);
  server.on("/switch", handleSwitchHtml);
  server.on("/reset", handleResetHtml);
  server.on("/resetAlert", handleResetAlertHtml);
  server.on("/factorySettings", handleFactorySettings);
  server.on("/factorySettingsAlert", handleFactorySettingsAlert);
  server.onNotFound(handleNotFound);
  server.begin();

  digitalWrite(connectLed, LOW);
  if (accesspointMode) {
    Serial.println("HTTP Access Point started");
  } else {
    Serial.println("HTTP service started");
  }
}

void loop(void) {
  unsigned long ms = millis();

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

  if (ms > timeToCheckDevice) {
    //
    // Check if the device state has changed
    //
    timeToCheckDevice = ms + msCheckDeviceDelay;
    checkDevice();
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

void handleSetTimeHtml() {
  int ofs = -1;
  int day = -1;
  int dev = -1;
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "ofs") {
      ofs = server.arg(i).toInt();
    }
    if (server.argName(i) == "day") {
      day = server.arg(i).toInt();
    }
    if (server.argName(i) == "dev") {
      dev = server.arg(i).toInt();
    }
  }
  if (ofs >= 0) {
    flipDeviceState(ofs, day, dev);
  }
  server.send(200, txtHtml, schedulePageHtml(day, dev));
}

void handleDispDayHtml() {
  startTransaction(msPerSecond);
  int day = -1;
  int dev = -1;
  for (uint8_t i = 0; i < server.args(); i++) {
    if (server.argName(i) == "day") {
      day = server.arg(i).toInt();
    }
    if (server.argName(i) == "dev") {
      dev = server.arg(i).toInt();
    }
  }
  server.send(200, txtHtml, schedulePageHtml(day, dev));
}

void handleBoostHtml() {
  startTransaction(msPerSecond);
  server.send(200, txtHtml, configHtml(processBoostArgs()));
}

void handleControlHtml() {
  startTransaction(msPerSecond);
  server.send(200, txtHtml, configHtml(processControlArgs()));
}

void handleResetAlertHtml() {
  startTransaction(msPerSecond);
  server.send(200, txtHtml, alertHtml("OK to Restart or CANCEL." , "OK", "CANCEL", "reset", "config"));
}

void handleResetHtml() {
  startTransaction(msPerSecond);
  initRestart(false, msPerSecond * 3);
  server.send(200, txtHtml, configHtml("Restart requested"));
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
void handleFactorySettingsAlert() {
  startTransaction(msPerSecond);
  server.send(200, txtHtml, alertHtml("OK to Reset To Factory Settings or CANCEL." , "OK", "CANCEL", "factorySettings", "config"));
}

void handleFactorySettings() {
  startTransaction(msPerHalfSecond);

  String ssid = readPreference(ssidName, emptyStr);
  String pw = readPreference(passwordName, emptyStr);

  preferences.begin(appId, false);
  preferences.clear();
  preferences.end();

  writePreference(ssidName, ssid);
  writePreference(passwordName, pw);

  initRestart(false, msPerSecond);
  server.send(200, txtHtml, configHtml("Reset to Factory Settings: Restarting..."));
}

void handleNotFound() {
  startTransaction(msPerHalfSecond);
  server.send(404, txtHtml, alertHtml("ERROR: URI Not Found: " + server.uri(), "OK", "", "config", ""));
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
  return daysFull[dayFromBase()] + " " + dd(hourOfDay()) + ":" + dd(minuteOfHour());
}

unsigned long  calcBaseSecondsFromLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0;
  }
  return (timeinfo.tm_wday * secondsPerDay) + (timeinfo.tm_hour * secondsPerHour) + (timeinfo.tm_min * secondsPerMinute) + timeinfo.tm_sec;
}

unsigned long secondsFromBase() {
  return secondsReference + (millis() / msPerSecond);
}

unsigned long minutesAtStartOfDay() {
  return dayFromBase() * minsPerDay;
}

unsigned long minutesFromBase() {
  return secondsFromBase() / secondsPerMinute;
}

unsigned long hourFromBase() {
  return secondsFromBase() / secondsPerHour;
}

unsigned long dayFromBase() {
  return secondsFromBase() / secondsPerDay;
}

unsigned long hourOfDay() {
  return hourFromBase() - (dayFromBase() * hoursPerDay);
}

unsigned long minuteOfDay() {
  return minutesFromBase() - (dayFromBase() * minsPerDay);
}

unsigned long minuteOfHour() {
  return minutesFromBase() - (hourFromBase() * minsPerHour);
}

unsigned long getSlotForMinsOfDay() {
  return minuteOfDay() / 15;
}

boolean isStatusAuto(int dev) {
  return (controlStatusList[dev] == autoStr);
}

boolean isStatusOn(int dev) {
  return (controlStatusList[dev] == onStr);
}

boolean isStatusOff(int dev) {
  return (controlStatusList[dev] == offStr);
}

//
// Double Digits!
//
String dd(int d) {
  if (d < 10) {
    return "0" + String(d);
  }
  return String(d);
}


String getNextEvent(int dev) {
  int day = dayFromBase();
  int forSlot = getSlotForMinsOfDay();
  boolean flagOn = false;
  for (int slot = 0; slot < forSlot + 1; slot++) {
    if ((deviceSlotsList[day].charAt(slot) & deviceOnBitList[dev]) != 0) {
      flagOn = ! flagOn;
    }
  }
  String details = deviceShortList[dev] + " is ";
  if (flagOn) {
    details += "ON. ";
  } else {
    details += "OFF. ";
  }
  int nextSlot = 0;
  for (int slot = forSlot + 1; slot < slots + 1; slot++) {
    if ((deviceSlotsList[day].charAt(slot) & deviceOnBitList[dev]) != 0) {
      nextSlot = slot;
      break;
    }
  }
  if (flagOn) {
    if (nextSlot == 0) {
      details += "OFF at Midnight";
    } else {
      details += "OFF at " + slotToTime(nextSlot);
    }
  } else {
    if (nextSlot > 0) {
      details += "ON at " + slotToTime(nextSlot);
    }
  }
  return details;
}

String calcMinutesToGo(unsigned long mins) {
  unsigned long diff = (mins - minuteOfDay()) + 1;
  if ((mins == 0) || (diff < 0)) {
    return "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;?&nbsp;&nbsp;&nbsp;&nbsp;";
  }
  int h = diff / 60;
  int m = diff - (h * 60);
  if (h == 0) {
    return dd(m) + "mins&nbsp;";
  }
  return String(h) + "h " + dd(m) + "m";
}

String slotToTime(int slot) {
  int h = (slot * minsPerSlot) / 60;
  int m = (slot * minsPerSlot) % 60;
  return dd(h) + ":" + dd(m) + "&nbsp;";
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



boolean isDeviceOn(int maxSlot, int day, int device) {
  boolean flagOn = false;
  for (int slot = 0; slot < maxSlot + 1; slot++) {
    if ((deviceSlotsList[day].charAt(slot) & deviceOnBitList[device]) != 0) {
      flagOn = ! flagOn;
    }
  }
  return flagOn;
}

void flipDeviceState(int slot, int day, int device) {
  deviceSlotsList[day].setCharAt(slot, deviceSlotsList[day].charAt(slot) ^ deviceOnBitList[device]);
  writePreference("SL" + String(day), deviceSlotsList[day]);
}


String isSlotChecked(int slot, int day, int device) {
  if ((deviceSlotsList[day].charAt(slot) & deviceOnBitList[device]) != 0) {
    return "checked";
  }
  return "";
}

/*
   Set activity led. Extends the on time so it can be seen.
*/
void startTransaction(unsigned int delayMs) {
  digitalWrite(activityLed, HIGH);
  activityLedOff = millis() + delayMs;
  if (accesspointMode) {
    timeToRestart = millis() + msPerMin;
  }
}

String processControlArgs() {
  for (uint8_t i = 0; i < server.args(); i++) {
    String divName = server.argName(i);
    String divVal = server.arg(i);
    for (int dev = 0; dev < deviceCount; dev++) {
      if (divName == deviceIdList[dev]) {
        controlStatusList[dev] = divVal;
        writePreference("CT" + String(dev), divVal);
        break;
      }
    }
  }
  checkDevice();
  return "";

}

/*
   Interpret device id and actions from request query parameters.
   Calculates offsets in MS for the device OFF
*/
String processBoostArgs() {
  int minutesNow = minuteOfDay();
  for (uint8_t i = 0; i < server.args(); i++) {

    String divName = server.argName(i);
    String divVal = server.arg(i);

    int divAbsolute;
    if (divVal.startsWith("O")) {
      //
      // H prefix device is OFF
      //
      divAbsolute = -1;
    } else {
      if (divVal.startsWith("H")) {
        //
        // H prefix is on until a specific hour
        //(hour * minsPerHour) + mins
        
        divAbsolute = divVal.substring(1).toInt() * minsPerHour;
      } else {
        //
        // No prefix is on until for the next n minutes
        //
        divAbsolute = (minutesNow + divVal.toInt()) - 1;
      }
      if (divAbsolute < 0) {
        divAbsolute = 0;
      }
    }
    for (int dev = 0; dev < deviceCount; dev++) {
      if (divName == deviceIdList[dev]) {
        deviceOffsetMins[dev] = divAbsolute;
        break;
      }
    }
  }
  checkDevice();
  return "";
}

/*
   Check for a boosted device making sure it is off or on
*/
void checkDevice() {
  int day = dayFromBase();
  int slot = getSlotForMinsOfDay();
  for (int dev = 0; dev < deviceCount; dev++) {
    boolean out = false;
    if (isStatusOff(dev)) {
      //
      // Override device should be OFF
      //
      deviceOffsetMins[dev] = 0;
      out = false;
    } else {
      if (isStatusOn(dev)) {
        //
        // Override device should be ON
        //
        deviceOffsetMins[dev] = 0;
        out = true;
      } else {
        if (deviceOffsetMins[dev] == 0) {
          //
          // Not Boosted so AUTO
          //         
          out = isDeviceOn(slot, day, dev);
        } else {
          //
          // BOOSTED
          //
          if (minuteOfDay() > deviceOffsetMins[dev]) {
            //
            // BOOSTED and Timed out!
            //
            out = false;
            deviceOffsetMins[dev] = 0;
          } else {
            //
            // BOOSTED with time to go!
            //
            out = true;
          }
        }
      }
    }
    if (out) {
      deviceState[dev] = onStr;
      digitalWrite(devicePinList[dev], HIGH);
    } else {
      deviceState[dev] = offStr;
      digitalWrite(devicePinList[dev], LOW);
    }
  }
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
