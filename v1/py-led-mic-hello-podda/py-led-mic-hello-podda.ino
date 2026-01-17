#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "test-wifi";
const char* password = "test-psd";

#define LED_PIN 13

WebServer server(80);

void turnOnLED() {
  digitalWrite(LED_PIN, HIGH);
  server.send(200, "text/plain", "LED ON");
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

  server.on("/on", turnOnLED);
  server.begin();
}

void loop() {
  server.handleClient();
}
