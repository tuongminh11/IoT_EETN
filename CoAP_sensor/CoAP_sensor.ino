#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <ArduinoJson.h>
#include "ESPAsyncUDP.h"

#define TRIGGER_PIN 0  //pin reset config
#define CONTEXT_PIN 2

bool context = false;
bool lastContext = false;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

uint8_t period = 2;

void callback_response(CoapPacket &packet, IPAddress ip, int port);
void callback_periodSensor(CoapPacket &packet, IPAddress ip, int port);

WiFiManager wm;
String nameTag = "garden_sensor";

WiFiUDP Udp;
Coap coap(Udp);
AsyncUDP udp;

IPAddress centralIP;
bool serverConnect = false;

const String cmd = "BUSTER CALL";
unsigned long lastLoop = 0;

void callback_periodSensor(CoapPacket &packet, IPAddress ip, int port) {
  uint8_t p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  String message = String((char *)&p);
  Serial.println(message);
  if (atof((char *)&p) > 0) {
    period = atof((char *)&p);
    coap.sendResponse(ip, port, packet.messageid, "change period successfully");
  } else coap.sendResponse(ip, port, packet.messageid, "invalid period");
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Coap Response got]");
  uint8_t p;
  memcpy(&p, packet.payload, packet.payloadlen);
  Serial.println(p);
  serverConnect = true;
}

void getIPHomeCenter() {
  Serial.println("Connect to Home Center");
  String msg = nameTag + "/0";
  udp.broadcast(msg.c_str());
}

void setup() {
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(CONTEXT_PIN, INPUT_PULLUP);

  bool res = wm.autoConnect("TM-NgocHung CoAP sensor", "12345678");
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("connected...yeey :)");
  }

  if (udp.listen(1234)) {
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP());
    udp.onPacket([](AsyncUDPPacket packet) {
      Serial.print("UDP Packet Type: ");
      Serial.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast"
                   : "Unicast");
      Serial.print(", From: ");
      Serial.print(packet.remoteIP());
      Serial.print(":");
      Serial.print(packet.remotePort());
      Serial.print(", To: ");
      Serial.print(packet.localIP());
      Serial.print(":");
      Serial.print(packet.localPort());
      Serial.print(", Length: ");
      Serial.print(packet.length());
      Serial.print(", Data: ");
      Serial.write(packet.data(), packet.length());
      Serial.println();
      if (!packet.isBroadcast()) {
        if (!packet.isMulticast()) {
          char p[11];
          memcpy(&p, (char *)packet.data(), 11);
          String a = String(p);
          if (a.equals(cmd)) {
            centralIP = packet.remoteIP();
            serverConnect = true;
            Serial.println("connected to server");
            Serial.println(centralIP);
            udp.close();
          }
          else {
            Serial.println(String((char *)packet.data()));
            Serial.println("code not match");
          }
        }
      }
    });
  }

  coap.server(callback_periodSensor, "control");
  // client response callback.
  // this endpoint is single callback.

  coap.response(callback_response);

  // start coap server/client
  coap.start();
}

void checkButton() {
  // check for button press
  if (digitalRead(TRIGGER_PIN) == LOW) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if (digitalRead(TRIGGER_PIN) == LOW) {
      Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000);  // reset delay hold
      if (digitalRead(TRIGGER_PIN) == LOW) {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wm.resetSettings();
        ESP.restart();
      }

      // start portal w delay
      Serial.println("Starting config portal");
      wm.setConfigPortalTimeout(120);

      if (!wm.startConfigPortal("OnDemandAP", "password")) {
        Serial.println("failed to connect or hit timeout");
        delay(3000);
        // ESP.restart();
      } else {
        //if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
      }
    }
  }
}

void loop() {
  bool reading = digitalRead(CONTEXT_PIN);
  if (!reading) {
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (!reading) {
        if(lastContext != reading) context = !context;
      }
      lastContext = reading;
    }
  }
  else {
    lastDebounceTime = millis();
    lastContext = reading;
  }
  if (millis() - lastLoop >= period * 1000) {
    if (!serverConnect) {
      getIPHomeCenter();
    } else {
      serverConnect = false;
      DynamicJsonDocument doc(100);
      uint8_t temp = random(10, 40);
      uint8_t humi = random(0, 100);
      if (context) temp = random(60, 100);
      doc[nameTag]["period"] = period;
      doc[nameTag]["temprature"] = temp;
      doc[nameTag]["humidity"] = humi;
      Serial.print("Send : ");
      String data;
      serializeJson(doc, data);
      char a[data.length() + 1];
      data.toCharArray(a, data.length() + 1);
      Serial.println(data);
      int rq = coap.send(centralIP, 5683, "sensor", COAP_CON, COAP_PUT, NULL, 0, (uint8_t *)&a, sizeof(a));
    }
    lastLoop = millis();
  }
  coap.loop();
  checkButton();
}
