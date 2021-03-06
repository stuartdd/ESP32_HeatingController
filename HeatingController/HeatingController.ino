
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
const String deviceName = "Heating Controller";
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
const String secondsStoreName = "secondsStore";

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

const String docType = "<!DOCTYPE html>";
const String viewport = "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
const String days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
const String daysFull[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
const String appJson = "application/json";
const String txtHtml = "text/html";

const String boostedMessageHtml = " <span class=\"BO\">&nbsp;BOOSTED&nbsp;</span> for ";
const String boostedHeadHtml = "<hr><h2>Boost:Overrides schedule:</h2><table>"
                               "<tr>"
                               "<th>Device</th><th>Boost</th><th>For</th><th>Boost</th><th>Boost</th><th>Boost</th><th>Until</th>"
                               "</tr>";
const String controlHeadHtml = "<hr><h2>Control:Overrides everything:</h2>"
                               "<table>"
                               "<tr>"
                               "<th>Device</th><th>Status</th><th></th><th></th><th></th>"
                               "</tr>";
const String scheduleHeadHtml = "<hr><h2>Schedule:Select a day:</h2>";
const String noScheduleHeadHtml = "<hr><h2>Schedule requires time set:</h2>";
const String noTimeHtml = "<hr><h2>Time unavailable until set:</h2>";
const String htmlEnd = "</body></html>";
const String resetHtml = "<hr><h2>Reset:</h2>"
                         " <input class=\"B1\" type=\"button\" onclick=\"location.href='/resetAlert';\" value=\"Reset Now\"/>";
const String configButtonHtml = "<hr><h2>Settings:</h2>"
                                " <input class=\"B1\" type=\"button\" onclick=\"location.href='/config';\" value=\"Go to Settings\"/>";
const String mainButtonHtml = "<hr><h2>Main Page:</h2>"
                              " <input class=\"B1\" type=\"button\" onclick=\"location.href='/';\" value=\"Return to Main Page\"/>";
const String factorySetHtml = "<hr><h2>Factory Reset:</h2>"
                              "<input class=\"B1\" type=\"button\" onclick=\"location.href='/factorySettingsAlert';\" value=\"Reset " + deviceName + " to factory settings\" />";

const String connectionDataHeadHtml = "<hr><h2>Connection Data:</h2><input class=\"B1\" type=\"button\" onclick=\"location.href='/conndatapage';\" value=\"Set Connection Data\"/>";
const String setTimeHeadHtml = "<hr><h2>Time and Date:</h2><input class=\"B1\" type=\"button\" onclick=\"location.href='/settimepage';\" value=\"Set Day and Time\"/>";
/*
   Never set directly use setSecondsReference(n);
*/
unsigned long __SecondsReference = 0;
/*
   Working storage
*/
unsigned long timeToRestart = 0;
unsigned long timeToCheckDevice = 0;
unsigned long timeToCheckMinutes = 0;


unsigned long activityLedOff = millis();
unsigned long connectLedTime = millis();
boolean timeSetByNTP = false;

boolean connectFlipFlop = true;
boolean restartInApMode = false;
boolean accesspointMode = false;
boolean timeIsSet = false;

unsigned long deviceOffsetMins[deviceCount];
String deviceState[deviceCount];
String deviceSlotsList[daysInWeek];
String controlStatusList[deviceCount];

/*
   Define dynamic HTML Strings
*/

String css() {
  return "html body {background-color: " + choose(accesspointMode , "DarkGreen" , "DodgerBlue") + ";"
         "color: white;"
         "width: 100%;"
         "height: 100%;"
         "margin: 0;"
         "padding: 0;"
         "}"
         "h2 {color: white;}"
         "table {"
         "text-align: center;"
         "text-decoration: none;"
         "color: white;"
         "}"
         ".B1 {"
         "background-color: white;"
         "color: black;"
         "border: none;"
         "padding: 7px 15px;"
         "text-align: center;"
         "text-decoration: none;"
         "display: inline-block;"
         "}"
         ".T2 {width: 400px;}"
         ".T3 td {border: 1px solid black; border-collapse: collapse;}"
         ".IN {color: black;}"
         ".HI {background-color: red;color: white;}"
         ".GN {background-color: green;color: white;}"
         ".BO {background-color: yellow;color: black;}"
         ".ON {background-color: yellow;color: black;}"
         ".TU {font-weight:bold;text-decoration:underline;}";
}

String mainPageHtml(String msg) {
  return configHtmlHead(true, "Main") + controlHtml() + messageHtml(msg) + boostHtml()  + scheduleDayHtml() + configButtonHtml + htmlEnd;
}

String configPageHtml(String msg) {
  return configHtmlHead(false, "Config")  + setTimeHeadHtml + connectionDataHeadHtml + resetHtml + factorySetHtml + mainButtonHtml + htmlEnd;
}

String configHtmlHead(boolean refresh, String title) {
  String m = "";
  for (int dev = 0; dev < deviceCount; dev++) {
    if (isStatusAuto(dev)) {
      if (deviceOffsetMins[dev] > 0) {
        m += "<h3>" + deviceDescList[dev] + boostedMessageHtml + calcMinutesToGo(deviceOffsetMins[dev]) + "&nbsp;" + boostButtonHtml(deviceIdList[dev] + "=0", "CANCEL", true) + "</h3>";
      } else {
        m += "<h3>" + deviceDescList[dev] + " is: <span class=\"GN\">&nbsp;" + deviceState[dev] + "&nbsp;</span></h3> ";
      }
    } else {
      m += "<h3>" + deviceDescList[dev] + " is: <span class=\"HI\">&nbsp;" + deviceState[dev] + "&nbsp;</span></h3> ";
    }
  }

  String dateStr = "";
  if (timeIsSet) {
    dateStr = "<hr><h2>Date: " + getDateTimeString() + "</h2>" + m;
  } else {
    dateStr = noTimeHtml;
  }

  String refreshStr = "";
  if (refresh && timeIsSet) {
    refreshStr = "<meta http-equiv=\"refresh\" content=\"10; url=/\">";
  }

  return docType + "<html><head><style>" + css() + "</style>" + refreshStr +
         "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" /><title>" + title + "-" + deviceName + " </title></head><body><h2>" + deviceDesc + "</h2>" + dateStr;
}

String controlHtml() {
  String ht = controlHeadHtml;
  for (int dev = 0; dev < deviceCount; dev++) {
    ht += "<tr>"
          "<td>" + deviceShortList[dev] + "</td>" +
          "<td " + (isStatusAuto(dev) ? "" : "class=\"HI\"") + ">" + controlStatusList[dev] + "</td>" +
          controlButtonHtml(dev , autoStr, "AUTO", timeIsSet) +
          controlButtonHtml(dev , onStr, "ON", true) +
          controlButtonHtml(dev , offStr, "OFF", true) +
          "</tr>";
  }
  return ht + "</table>";
}

String controlButtonHtml(int dev, String action, String desc, boolean include) {
  return "<td><input class=\"B1\" " + choose(include , "" , "disabled") + " type=\"button\" onclick=\"location.href='/control?" + deviceIdList[dev] + "=" + action + "';\" value=\"" + choose(include , desc , "N/A") + "\" /></td>";
}

String alertHtml(String msg, String b1, String b2, String a1, String a2) {
  String b1Html = "";
  if (b1 != "") {
    b1Html = "<input class=\"B1\" type=\"button\" onclick=\"location.href='/" + a1 + "';\" value=\"" + b1 + "\" />";
  }
  String b2Html = "";
  if (b2 != "") {
    b2Html = "<input class=\"B1\" type=\"button\" onclick=\"location.href='/" + a2 + "';\" value=\"" + b2 + "\" />";
  }
  return configHtmlHead(false, "Alert") + messageHtml(msg) + b1Html + "<hr>" + b2Html + htmlEnd;
}

String messageHtml(String msg) {
  if (msg == "") {
    return "";
  }
  return "<hr><h2>Alert: " + msg + "</h2>";
}

String setTimeFormHtml() {
  return configHtmlHead(false, "Set Day & Time") + "<hr><form action=\"/configtime\"><h2>Set the Day and Time:</h2>" + dowDropDownHtml() + timeInputHtml() +
         "<br/><br/><input class=\"B1\" type=\"submit\" value=\"Set Time\"></form>"
         "<hr><input class=\"B1\" type=\"button\" onclick=\"location.href='/config';\" value=\"Cancel\">" + htmlEnd;
}

String connectionFormHtml() {
  return configHtmlHead(false, "Network Connection") +
         "<hr><form action=\"/store\">"
         "<h2>Set Network Connection:</h2>"
         "<h3>Router Name:</h3>"
         "<input class=\" IN\" type=\"text\" name=\"ssid\" value=\"" + readSsid(emptyStr) + "\"><br>"
         "<h3>Password:</h3>"
         "<input class=\"IN\" type=\"text\" name=\"pw\" value=\"\"><br><br>"
         "<input class=\"B1\" type=\"submit\" value=\"Send connection data\">"
         "</form>"
         "<hr><input class=\"B1\" type=\"button\" onclick=\"location.href='/config';\" value=\"Cancel\">" +  htmlEnd;
}

String boostHtml() {
  String ht = boostedHeadHtml;
  for (int dev = 0; dev < deviceCount; dev++) {
    if (isStatusAuto(dev)) {
      ht += "<tr>"
            "<td>" + deviceShortList[dev] + "</td>"
            "<td>" + (deviceOffsetMins[dev] > 0 ? "ON" : "OFF") + "</td>"
            "<td>" + calcMinutesToGo(deviceOffsetMins[dev]) + "&nbsp;</td>" +
            boostButtonHtml(deviceIdList[dev] + "=60", "1 Hour", true) +
            boostButtonHtml(deviceIdList[dev] + "=120", "2 Hour", true) +
            boostButtonHtml(deviceIdList[dev] + "=240", "4 Hour", true) +
            boostButtonHtml(deviceIdList[dev] + "=H24", "Midnight", timeIsSet) +
            "</tr>";
    } else {
      ht += "<tr><td colspan=\"3\">" + deviceShortList[dev] + " is " + controlStatusList[dev] + ".</td><td colspan=\"4\">To BOOST it set it to AUTO:</td></tr>";
    }
  }
  return ht + "</table>";
}

String boostButtonHtml(String deviceTime, String desc, boolean include) {
  return "<td><input class=\"B1\" " + choose(include, "" , "disabled") + " type=\"button\" onclick=\"location.href='/boost?" + deviceTime + "';\" value=\"" + choose(include , desc , "N/A") + "\" /></td>";
}

String scheduleDayHtml() {
  if (timeIsSet) {
    String ht = scheduleHeadHtml + "<table>";
    for (int dev = 0; dev < deviceCount; dev++) {
      if (isStatusAuto(dev)) {
        ht += "<tr><td>" + deviceShortList[dev] + "</td>" + scheduleDayButtonRowHtml(dev) + "</tr>";
      } else {
        ht += "<tr><td colspan=\"3\">" + deviceShortList[dev] + " is " + controlStatusList[dev] + ".</td><td colspan=\"5\">To SCHEDULE it set it to AUTO:</td></tr>";
      }
    }
    return ht + "</table>";
  } else {
    return noScheduleHeadHtml;
  }
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
    s = " TU";
  }
  return "<td><input class=\"B1" + s + "\" type=\"button\" onclick=\"location.href='/dispDay?day=" + String(day) + "&dev=" + String(device) + "';\" value=\"" + days[day] + "\" /></td>";
}


String dowDropDownHtml() {
  String s = "<h3>Select a DAY:</h3><select class=\"IN\" name=\"day\">";
  for (int i = 0; i < daysInWeek; i++) {
    s += "<option value=\"" + String(i) + "\">" + daysFull[i] + "</option>";
  }
  return s + "</select>";
}

String timeInputHtml() {
  return "<h3>Enter Time 'hh mm'</h3><input class=\"IN\" width=\"8\" type=\"text\" name=\"time\" value=\"\">";
}

String schedulePageHtml(int day, int dev) {
  return configHtmlHead(false, "Schedule") + scheduleTimesHtml(day, dev) + htmlEnd;
}

String scheduleTimesHtml(int day, int dev) {
  return "<hr><h2>Times for " + deviceDescList[dev] + " on " + daysFull[day] + "</h2>"
         "<input class=\"B1\" type=\"button\" onclick=\"location.href='/';\" value=\"DONE!\"/>"
         "</br><table class=\"T2 T3\">" +
         scheduleTimesCbHtmlAll(day, dev) +
         "</table>";
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
    s = "<td class=\"ON\">";
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

  timeSetByNTP = false;

  setSecondsReference(readSecondsRefPreference());
  if (timeIsSet) {
    Serial.println("Time read:" + getDateTimeString());
  } else {
    Serial.println("Time NOT read from preferences");
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
    initRestart(false, msPerMin * 3);
    writePreference(rebootAPFlagName, emptyStr);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(deviceName.c_str(), apPassword.c_str());
    WiFi.softAPConfig(IP, IP, mask);
    server.begin();
    Serial.print("STARTING ACCESS POINT for ");
    Serial.print(deviceName);
    Serial.print(" on IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("MAC:");
    Serial.println(WiFi.softAPmacAddress());
  } else {
    accesspointMode = false;
    Serial.print("SSID[");
    Serial.print(readSsid(undefined));
    Serial.println("]");
    Serial.print("PW[");
    Serial.print(readPreference(passwordName, undefined));
    Serial.println("]");
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
        performRestart(true, "FAILED TO CONNECT TO ROUTER!");
      }
      if (digitalRead(forceAPModeIn) == LOW) {
        performRestart(true, "BUTTON");
      }
      count++;
      delay(500);
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
        setSecondsReference(0);
        Serial.println("Failed");
        timeNotFetchedCount++;
        if (timeNotFetchedCount > 4) {
          performRestart(false, "NTP FAILED");
        }
      } else {
        timeNotFetched = false;
        timeSetByNTP = true;
        clearSecondsRefPreference();
        setSecondsReference(calcBaseSecondsFromLocalTime());
      }
    }
  }
  delay(msPerHalfSecond);
  server.on("/", handleMainHtml);
  server.on("/store", handleStoreConfigDataAndRestart);
  server.on("/configtime", handleConfigTimeHtml);
  server.on("/config", handleConfigHtml);
  server.on("/dispDay", handleDispDayHtml);
  server.on("/setTime", handleSetTimeHtml);
  server.on("/control", handleControlHtml);
  server.on("/boost", handleBoostHtml);
  server.on("/reset", handleResetHtml);
  server.on("/conndatapage", handleConnectionFormHtml);
  server.on("/settimepage", handleSetTimeFormHtml);
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

  timeToCheckDevice =  millis() + msCheckDeviceDelay;
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

void handleConfigTimeHtml() {
  Serial.println("SetTime:");
  int day = -1;
  int h = -1;
  int m = -1;
  for (uint8_t i = 0; i < server.args(); i++) {
    String val = server.arg(i);
    Serial.println(server.argName(i) + "=" + val);
    if (server.argName(i) == "day") {
      day = readNumberFromString(val, 1, day);
    }
    if (server.argName(i) == "time") {
      h = readNumberFromString(val, 1, h);
      m = readNumberFromString(val, 2, m);
    }
  }
  if ((day >= 0) && (day < 7) && (h >= 0) && (m >= 0)) {
    setSecondsReference(calcBaseSecondsFromDayHourMin(day, h, m, 0));
    if (timeIsSet) {
      writeSecondsRefPreference();
    }
    server.send(200, txtHtml, mainPageHtml("Time and Date set:" + getDateTimeString()));
  }
  server.send(200, "text/html", alertHtml("Error parsing date and time" , "", "OK", "", "config"));
}

void handleConfigHtml() {
  startTransaction(msPerSecond);
  server.send(200, "text/html", configPageHtml(""));
}

void handleMainHtml() {
  startTransaction(msPerSecond);
  server.send(200, "text/html", mainPageHtml(""));
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
  server.send(200, txtHtml, mainPageHtml(processBoostArgs()));
}

void handleControlHtml() {
  startTransaction(msPerSecond);
  server.send(200, txtHtml, mainPageHtml(processControlArgs()));
}

void handleResetAlertHtml() {
  startTransaction(msPerSecond);
  server.send(200, txtHtml, alertHtml("OK to Restart or CANCEL." , "OK", "CANCEL", "reset", "config"));
}

void handleResetHtml() {
  Serial.println("Handle Restart");
  startTransaction(msPerSecond);
  initRestart(false, msPerSecond * 3);
  server.send(200, txtHtml, mainPageHtml("Restart requested"));
}

void handleConnectionFormHtml() {
  startTransaction(msPerSecond);
  server.send(200, txtHtml, connectionFormHtml());
}

void handleSetTimeFormHtml() {
  startTransaction(msPerSecond);
  server.send(200, txtHtml, setTimeFormHtml());
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
  server.send(200, txtHtml, mainPageHtml("Data stored: Restarting..."));
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
  Serial.println("RESET FACTORY SETTINGS");
  String ssid = readPreference(ssidName, emptyStr);
  String pw = readPreference(passwordName, emptyStr);

  preferences.begin(appId, false);
  preferences.clear();
  preferences.end();

  writePreference(ssidName, ssid);
  writePreference(passwordName, pw);
  clearSecondsRefPreference();

  initRestart(false, msPerSecond);
  server.send(200, txtHtml, mainPageHtml("Reset to Factory Settings: Restarting..."));
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

void updateSecondsRefPreference() {
  if (readSecondsRefPreference() > 0) {
    writeSecondsRefPreference();
  } else {
    clearSecondsRefPreference();
  }
}

void writeSecondsRefPreference() {
  if ((timeIsSet) && (!timeSetByNTP)) {
    writePreference(secondsStoreName, String(__SecondsReference + (millis() / msPerSecond)));
    Serial.println("Time Updated:" + getDateTimeString());
  } else {
    clearSecondsRefPreference();
  }
}

void clearSecondsRefPreference() {
  writePreference(secondsStoreName, "");
}

int readSecondsRefPreference() {
  String sp = readPreference(secondsStoreName, "");
  if (sp == "") {
    return 0;
  } else {
    return sp.toInt();
  }
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
String getDateTimeString() {
  return daysFull[dayFromBase()] + " " + dd(hourOfDay()) + ":" + dd(minuteOfHour());
}

unsigned long  calcBaseSecondsFromLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0;
  }
  return calcBaseSecondsFromDayHourMin(timeinfo.tm_wday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

unsigned long  calcBaseSecondsFromDayHourMin(int d, int h, int m, int sec) {
  return (d * secondsPerDay) + (h * secondsPerHour) + (m * secondsPerMinute) + sec;
}

unsigned long secondsFromBase() {
  return __SecondsReference + (millis() / msPerSecond);
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

void setSecondsReference(unsigned long ref) {
  __SecondsReference = ref;
  timeIsSet = (__SecondsReference > 0);
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
  updateSecondsRefPreference();
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

int readNumberFromString(String s, int num, int defaultVal) {
  int p = 0;
  int l = s.length();
  String r = "";
  char c = s.charAt(p);
  for (int i = 0; i < num; i++) {
    while (((c < '0') || (c > '9')) && (p <= l)) {
      p++;
      c = s.charAt(p);
    }
    r = "";
    while (((c >= '0') && (c <= '9')) && (p <= l)) {
      r += c;
      p++;
      c = s.charAt(p);
    }
  }
  if (r == "") {
    return defaultVal;
  }
  return r.toInt();
}

/*
   Set activity led. Extends the on time so it can be seen.
*/
void startTransaction(unsigned int delayMs) {
  digitalWrite(activityLed, HIGH);
  activityLedOff = millis() + delayMs;
  if (accesspointMode) {
    if (timeToRestart > 0 ) {
      Serial.println("DELAY-RESTART Extended");
      timeToRestart = millis() + msPerMin * 3;
    }
  }
}

String choose(boolean oneNotTheOther, String one, String other) {
  if (oneNotTheOther) {
    return one;
  }
  return other;
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
          if (timeIsSet) {
            int slot = getSlotForMinsOfDay();
            out = isDeviceOn(slot, day, dev);
          }
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
  WiFiEvent(event);
}

void WiFiEvent(WiFiEvent_t event)
{
  Serial.printf("[WiFi-event] event: %d\n", event);

  switch (event) {
    case SYSTEM_EVENT_WIFI_READY:
      Serial.println("WiFi interface ready");
      break;
    case SYSTEM_EVENT_SCAN_DONE:
      Serial.println("Completed scan for access points");
      break;
    case SYSTEM_EVENT_STA_START:
      Serial.println("WiFi client started");
      break;
    case SYSTEM_EVENT_STA_STOP:
      Serial.println("WiFi clients stopped");
      break;
    case SYSTEM_EVENT_STA_CONNECTED:
      Serial.println("Connected to access point");
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("Disconnected from WiFi access point");
      break;
    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
      Serial.println("Authentication mode of access point has changed");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.print("Obtained IP address: ");
      Serial.println(WiFi.localIP());
      break;
    case SYSTEM_EVENT_STA_LOST_IP:
      Serial.println("Lost IP address and IP address is reset to 0");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
      Serial.println("WiFi Protected Setup (WPS): succeeded in enrollee mode");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
      Serial.println("WiFi Protected Setup (WPS): failed in enrollee mode");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
      Serial.println("WiFi Protected Setup (WPS): timeout in enrollee mode");
      break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN:
      Serial.println("WiFi Protected Setup (WPS): pin code in enrollee mode");
      break;
    case SYSTEM_EVENT_AP_START:
      Serial.println("WiFi access point started");
      break;
    case SYSTEM_EVENT_AP_STOP:
      Serial.println("WiFi access point  stopped");
      break;
    case SYSTEM_EVENT_AP_STACONNECTED:
      Serial.println("Client connected");
      break;
    case SYSTEM_EVENT_AP_STADISCONNECTED:
      Serial.println("Client disconnected");
      break;
    case SYSTEM_EVENT_AP_STAIPASSIGNED:
      Serial.println("Assigned IP address to client");
      break;
    case SYSTEM_EVENT_AP_PROBEREQRECVED:
      Serial.println("Received probe request");
      break;
    case SYSTEM_EVENT_GOT_IP6:
      Serial.println("IPv6 is preferred");
      break;
    case SYSTEM_EVENT_ETH_START:
      Serial.println("Ethernet started");
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("Ethernet stopped");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("Ethernet connected");
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("Ethernet disconnected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.println("Obtained IP address");
      break;
    default: break;
  }
}
