
#include <WiFi.h>
#include <WiFiClient.h>


WiFiServer server(80);
IPAddress IP(192, 168, 4, 15);
IPAddress mask = (255, 255, 255, 0);

byte ledPin = 2;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Wemos_AP", "pw123abc");
  WiFi.softAPConfig(IP, IP, mask);
  server.begin();
  pinMode(ledPin, OUTPUT);
  Serial.println();
  Serial.println("accesspoint_bare_01.ino");
  Serial.println("Server started.");
  Serial.print("IP: ");     Serial.println(WiFi.softAPIP());
  Serial.print("MAC:");     Serial.println(WiFi.softAPmacAddress());
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    digitalWrite(ledPin, LOW);
    String request = client.readStringUntil('\r');
    Serial.println("********************************");
    Serial.println("From the station: " + request);
    client.println("HI FROM AP");
    client.flush();
    digitalWrite(ledPin, HIGH);
  }
}
