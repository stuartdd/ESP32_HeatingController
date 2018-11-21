
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

const char deviceDataChar = '0'; // Char '0'
const int deviceOneNum = 0;
const int deviceTwoNum = 1;
const String deviceDescList[] = {"Central Heating", "Hot Water"};
const String deviceShortList[] = {"Heating:", "Water:"};
const String deviceNameList[] = {"ch:", "hw:"};
const int devicePinList[] = {16, 17};
const int deviceOnBitList[] = {1, 2};
const int deviceFlagBitList[] = {4, 8};

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
const unsigned long msPerDay = msPerHour * 24;
const unsigned long minsPerHour = 60;
const unsigned long minsPerDay = minsPerHour * 24;
const unsigned long msNtpFetchDelay = msPerSecond * 3;
const unsigned long minsPerSlot = 15;

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
const String fg = " style=\"color:White;\" ";
const String fgp = " style=\"background-color:DarkSalmon;\" ";
const String boostedHtml = " is <span " + fgp + ">&nbsp;BOOSTED&nbsp;</span> for ";
const String htmlEnd = "</body></html>";
const String resetHtml = "<hr><h3 " + fg + ">Reset " + deviceName + ":</h3>"
                         "<input type=\"button\" onclick=\"location.href='/resetAlert';\" value=\"Reset Now\" />";
const String factorySetHtml = "<hr><h3 " + fg + ">Warning:</h3>"
                              "<input type=\"button\" onclick=\"location.href='/factorySettingsAlert';\" value=\"Reset to " + deviceName + " to factory settings\" />";

/*
   Working storage
*/
unsigned long timeToRestart = 0;
unsigned long activityLedOff = millis();
unsigned long connectLedTime = millis();
unsigned long deviceOneOffsetMins = 0;
unsigned long deviceTwoOffsetMins = 0;
unsigned long minutesReference = 0;
unsigned long millisReference = 0;

boolean connectFlipFlop = true;
boolean accesspointMode = false;
boolean restartInApMode = false;
boolean displayConfig = false;
boolean htmlMode = true;
String deviceOneState = offStr;
String deviceTwoState = offStr;
String deviceSlotsList[daysInWeek];

/*
   Define dynamic HTML Strings
*/

String configHtmlHead(boolean refresh) {
  String m1 = "";
  if (deviceOneOffsetMins > 0) {
    m1 = "<h3 " + fg + " >" + deviceDescList[deviceOneNum] + boostedHtml + calcMinutesToGo(deviceOneOffsetMins) + ".&nbsp;" + boostButtonHtml(deviceNameList[deviceOneNum] + "=0", "Remove BOOST") + "</h3>";
  }
  String m2 = "";
  if (deviceTwoOffsetMins > 0) {
    m2 = "<h3 " + fg + " >" + deviceDescList[deviceTwoNum] + boostedHtml + calcMinutesToGo(deviceTwoOffsetMins) + ".&nbsp;" + boostButtonHtml(deviceNameList[deviceTwoNum] + "=0", "Remove BOOST") + "</h3>";
  }

  if (displayConfig) {
    return "<html><head><title>Config: " + deviceName + "</title></head>"
           "<body style=\"background-color:DarkSalmon;\"><h1 " + fg + ">Config: " + deviceDesc + "</h1>" + m1 + m2;
  } else {
    String refreshStr = "";
    if (refresh) {
      refreshStr = "<meta http-equiv=\"refresh\" content=\"10;url=/config\"/>";
    }
    return "<html><head>" + refreshStr + "<title>Main:" + deviceName + "</title></head>"
           "<body style=\"background-color:DodgerBlue;\"><h2 " + fg + ">" + deviceDesc + "</h2>"
           "<hr><h1 " + fg + ">Date: " + String(getLocalTime()) + "</h1>" + m1 + m2;
  }
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

String configHtml(String msg) {
  if (displayConfig) {
    return configHtmlHead(false) + messageHtml(msg) + statusHtml() + switchHtml("Running") + connectionHtml() + resetHtml + factorySetHtml + htmlEnd;
  }
  return configHtmlHead(true) + messageHtml(msg) + statusHtml() + dayButtonsHtml() + switchHtml("Config") + htmlEnd;
}

String connectionHtml() {
  return "<hr><form " + fg + " action=\"/store\">"
         "<h2>Connection:</h2>"
         "<h3>Router Name:</h3><input type=\"text\" name=\"ssid\" value=\"" + readSsid(emptyStr) + "\"><br>"
         "<h3>Password:</h3><input type=\"text\" name=\"pw\" value=\"\"><br><br>"
         "<input type=\"submit\" value=\"Send connection data\">"
         "</form>";
}

String statusHtml() {
  String d1 = deviceNameList[deviceOneNum];
  String d2 = deviceNameList[deviceTwoNum];
  return "<hr><h2 " + fg + " >Boost (Overrides the schedule):</h2><table " + fg + ">"
         "<tr>"
         "<th>Device</th><th>Now</th><th>For</th><th>Boost</th><th>Boost</th><th>Boost</th><th>Until</th>"
         "</tr><tr>"
         "<td>" + deviceShortList[deviceOneNum] + "</td>"
         "<td>" + deviceOneState + "</td>"
         "<td>" + calcMinutesToGo(deviceOneOffsetMins) + "&nbsp;</td>" +
         boostButtonHtml(d1 + "=60", "1 Hour") +
         boostButtonHtml(d1 + "=120", "2 Hour") +
         boostButtonHtml(d1 + "=240", "4 Hour") +
         boostButtonHtml(d1 + "=H24", "Midnight") +
         "</tr><tr>"
         "<td>" + deviceShortList[deviceTwoNum] + "</td>"
         "<td>" + deviceTwoState + "</td>"
         "<td>" + calcMinutesToGo(deviceTwoOffsetMins) + "&nbsp;</td>" +
         boostButtonHtml(d2 + "=60", "1 Hour") +
         boostButtonHtml(d2 + "=120", "2 Hour") +
         boostButtonHtml(d2 + "=240", "4 Hour") +
         boostButtonHtml(d2 + "=H24", "Midnight") +
         "</tr>"
         "</table>";
}

String boostButtonHtml(String deviceTime, String desc) {
  return "<td><input type=\"button\" onclick=\"location.href='/boost?" + deviceTime + "';\" value=\"" + desc + "\" /></td>";
}

String dayButtonsHtml() {
  return "<hr><h2 " + fg + " >Schedule (Select a day to change):</h2>" +
         "<table " + fg + ">"
         "<tr>" +
         "<td>" + deviceShortList[deviceOneNum] + "</td>" + dayButtonRowHtml(deviceOneNum) +
         "</tr>" +
         "<tr>" +
         "<td>" + deviceShortList[deviceTwoNum] + "</td>" + dayButtonRowHtml(deviceTwoNum) +
         "</tr>" +
         "</table>";
}

String dayButtonRowHtml(int device) {
  return dayButtonHtml(daySun, device) +
         dayButtonHtml(dayMon, device) +
         dayButtonHtml(dayTue, device) +
         dayButtonHtml(dayWed, device) +
         dayButtonHtml(dayThu, device) +
         dayButtonHtml(dayFri, device) +
         dayButtonHtml(daySat, device);
}

String dayButtonHtml(int day, int device) {
  return "<td><input type=\"button\" onclick=\"location.href='/dispDay?day=" + String(day) + "&dev=" + String(device) + "';\" value=\"" + days[day] + "\" /></td>";
}


String schedulePageHtml(int day, int dev) {
  return configHtmlHead(false) + dayButtonsHtml() + scheduleHtml(day, dev) + htmlEnd;
}

String scheduleHtml(int day, int dev) {
  return "<hr><h2 " + fg + " >Times for " + deviceDescList[dev] + " on " + daysFull[day] + "</h2>" +
         "<table border=\"1\" width=\"400px\">" +
         timeCbHtmlAll(day, dev) +
         "</table>" +
         "<br><input type=\"button\" onclick=\"location.href='/config';\" value=\"DONE!\"/>";
}

String timeCbHtmlAll(int day, int dev) {
  String ofs = "";
  int offset = 0;
  for (uint8_t y = 0; y < 24; y++) {
    ofs += "<tr>";
    for (uint8_t x = 0; x < 4; x++) {
      ofs += timeCbHtml(offset, day, dev);
      offset++;
    }
    ofs += "</tr>";
  }
  return ofs;
}

String timeCbHtml(int slot, int day, int dev) {
  String s = "<td>";
  if (isDeviceFlagOn(slot, day, dev)) {
    s = "<td " + fgp + ">";
  }
  return s + "<input type=\"checkbox\" " + checked(slot, day, dev) + " onclick=\"location.href='/setTime?ofs=" + slot + "&day=" + day + "&dev=" + dev + "'\">" + osfToTime(slot * minsPerSlot) + "</td>";
}


void setup(void) {

  initRestart(false, 0);
  Serial.begin(115200);
  delay(msPerHalfSecond);

  WiFi.disconnect(true);
  pinMode(forceAPModeIn, INPUT_PULLUP);

  pinMode(activityLed, OUTPUT);
  pinMode(connectLed, OUTPUT);
  pinMode(devicePinList[deviceOneNum], OUTPUT);
  pinMode(devicePinList[deviceTwoNum], OUTPUT);

  digitalWrite(activityLed, LOW);

  digitalWrite(devicePinList[deviceOneNum], LOW);
  deviceOneState = offStr;
  digitalWrite(devicePinList[deviceTwoNum], LOW);
  deviceTwoState = offStr;

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

    digitalWrite(connectLed, HIGH);
    Serial.print("Fetching time from NTP:");
    boolean timeNotFetched = true;
    int timeNotFetchedCount = 0;
    while (timeNotFetched) {
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      struct tm timeinfo;
      if (!getLocalTime(&timeinfo)) {
        delay(msNtpFetchDelay);
        digitalWrite(connectLed, LOW);
        delay(msNtpFetchDelay);
        digitalWrite(connectLed, HIGH);
        minutesReference = 0;
        millisReference = 0;
        Serial.println("Failed");
        timeNotFetchedCount++;
        if (timeNotFetchedCount > 2) {
          performRestart(false, "NTP FAILED");
        }
      } else {
        timeNotFetched = 0;
        digitalWrite(connectLed, LOW);
        minutesReference = minutesFromLocalTime();
        millisReference = millis();
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

  checkDevice(minutes());

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

  initRestart(true, msPerSecond);
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
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "?: ?:?:?";
  }
  return daysFull[timeinfo.tm_wday] + " " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min);
}

int minutesFromLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0;
  }
  return calcMinsFromTime(timeinfo.tm_wday, timeinfo.tm_hour, timeinfo.tm_min);
}

int minutes() {
  return minutesReference + ((millis() - millisReference) / msPerMin);
}

int calcMinsFromTime(int day, int hour, int mins) {
  return (day * minsPerDay) + (hour * minsPerHour) + mins;
}

int calcMinsFromHour(int hour, int mins) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0;
  }
  return calcMinsFromTime(timeinfo.tm_wday, hour, mins);
}

int calcMinsFromMins(int mins) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return 0;
  }
  return calcMinsFromTime(timeinfo.tm_wday, timeinfo.tm_hour, mins);
}

String calcMinutesToGo(unsigned long mins) {
  unsigned long diff = (mins - minutes()) + 1;
  if ((mins == 0) || (diff < 0)) {
    return "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;?&nbsp;&nbsp;&nbsp;&nbsp;";
  }
  int h = diff / 60;
  int m = diff - (h * 60);
  if (h == 0) {
    return String(m) + "mins&nbsp;";
  }
  return String(h) + "h " + String(m) + "m";
}

String osfToTime(int ofs) {
  int h = ofs / 60;
  int m = ofs % 60;
  String s;
  if (h < 10) {
    s = "0" + String(h) + ":";
  } else {
    s = String(h) + ":";
  }
  if (m < 10) {
    s += "0" + String(m);
  } else {
    s += String(m);
  }
  return s;
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

boolean isDeviceOn(int slot, int day, int device) {
  if ((deviceSlotsList[day].charAt(slot) & deviceOnBitList[device]) != 0) {
    return true;
  }
  return false;
}

boolean isDeviceFlagOn(int slot, int day, int device) {
  if ((deviceSlotsList[day].charAt(slot) & deviceFlagBitList[device]) != 0) {
    return true;
  }
  return false;
}

void flipDeviceState(int slot, int day, int device) {
  deviceSlotsList[day].setCharAt(slot, deviceSlotsList[day].charAt(slot) ^ deviceOnBitList[device]);
  boolean flagOn = false;
  boolean isOn = false;
  for (int i = 0; i < slots; i++) {
    isOn = isDeviceOn(i, day, device);
    if (isOn) {
      flagOn = ! flagOn;
    }
    if (flagOn) {
      deviceSlotsList[day].setCharAt(i, deviceSlotsList[day].charAt(i) | deviceFlagBitList[device]);
    } else {
      deviceSlotsList[day].setCharAt(i, deviceSlotsList[day].charAt(i) & (~deviceFlagBitList[device]));
    }
  }
  writePreference("SL" + String(day), deviceSlotsList[day]);
}


String checked(int slot, int day, int device) {
  if (isDeviceOn(slot, day, device)) {
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

/*
   Interpret device id and actions from request query parameters.
   Calculates offsets in MS for the device OFF
*/
String processBoostArgs() {
  int minutesNow = minutes();
  for (uint8_t i = 0; i < server.args(); i++) {

    String divName = server.argName(i);
    String divVal = server.arg(i);

    int divAbsolute;
    if (divVal.startsWith("H")) {
      //
      // H prefix is on until a specific hour
      //
      divAbsolute = calcMinsFromHour(divVal.substring(1).toInt(), 0);
    } else {
      //
      // No prefix is on until for the next n minutes
      //
      divAbsolute = (minutesNow + divVal.toInt()) - 1;
    }
    if (divAbsolute < 0) {
      divAbsolute = 0;
    }
    if (divName == deviceNameList[deviceOneNum]) {
      deviceOneOffsetMins = divAbsolute;
    }
    if (divName == deviceNameList[deviceTwoNum]) {
      deviceTwoOffsetMins = divAbsolute;
    }
  }
  checkDevice(minutesNow);
  return "";
}

/*
   Check for a boosted device making sure it is off or on
*/
void checkDevice(int mins) {
  if (deviceOneOffsetMins > 0) {
    if (deviceOneOffsetMins > mins) {
      digitalWrite(devicePinList[deviceOneNum], HIGH);
      deviceOneState = onStr;
    } else {
      digitalWrite(devicePinList[deviceOneNum], LOW);
      deviceOneState = offStr;
      deviceOneOffsetMins = 0;
    }
  }
  if (deviceTwoOffsetMins > 0) {
    if (deviceTwoOffsetMins > mins) {
      digitalWrite(devicePinList[deviceTwoNum], HIGH);
      deviceTwoState = onStr;
    } else {
      digitalWrite(devicePinList[deviceTwoNum], LOW);
      deviceTwoState = offStr;
      deviceTwoOffsetMins = 0;
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
