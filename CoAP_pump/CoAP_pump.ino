#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include "ESPAsyncUDP.h"
#include <ArduinoJson.h>
#define PUMP 2
#define TRIGGER_PIN 0
void callback_response(CoapPacket &packet, IPAddress ip, int port);
void callback_control(CoapPacket &packet, IPAddress ip, int port);

WiFiManager wm;
String nameTag = "garden_pump";
DynamicJsonDocument doc(100);
WiFiUDP Udp;
Coap coap(Udp);
AsyncUDP udp;

IPAddress centralIP;

bool PUMPSTATE;
unsigned long lastLoop = 0;
bool serverConnect = false;
String cmd = "BUSTER CALL";
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long timedelay;
bool context = false;
bool lastContext = false;

void callback_control(CoapPacket &packet, IPAddress ip, int port) {
  uint8_t p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  String message = String((char *)&p);
  Serial.println(message);
  if (message.equals("0"))
    PUMPSTATE = true;
  else if (message.equals("1"))
    PUMPSTATE = false;

  if (PUMPSTATE) {
    digitalWrite(PUMP, HIGH);
    coap.sendResponse(ip, port, packet.messageid, "1");
    Serial.println("OFF");
  } else {
    digitalWrite(PUMP, LOW);
    coap.sendResponse(ip, port, packet.messageid, "0");
    Serial.println("ON");
  }
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Coap Response got]");
  uint8_t p;
  memcpy(&p, packet.payload, packet.payloadlen);
  Serial.println(p);
}

void getIPHomeCenter() {
  Serial.println("Connect to Home Center");
  String msg = nameTag + "/0";
  udp.broadcast(msg.c_str());
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  bool res = wm.autoConnect("TM-NgocHung CoAP pump", "12345678");
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
      if (!packet.isBroadcast() && !packet.isMulticast()) {
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
      }
    });
  }
  pinMode(PUMP, OUTPUT);
  digitalWrite(PUMP, HIGH);
  PUMPSTATE = true;

  coap.server(callback_control, "control");
  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
}

void checkButton() {
  bool reading = digitalRead(TRIGGER_PIN);
  DynamicJsonDocument doc(100);
  if (!reading) {
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (!reading) {
        if (lastContext != reading)
        {
          if (!PUMPSTATE)
          {
            digitalWrite(PUMP, HIGH);
            Serial.println("OFF");
            doc[nameTag] = 0;
            PUMPSTATE = true;
          }
          else
          {
            digitalWrite(PUMP, LOW);
            Serial.println("ON");
            doc[nameTag] = 1;
            PUMPSTATE = false;
          }
          Serial.print("Send : ");
          String data;
          serializeJson(doc, data);
          char a[data.length() + 1];
          data.toCharArray(a, data.length() + 1);
          Serial.println(data);
          int rq = coap.send(centralIP, 5683, "sensor", COAP_CON, COAP_PUT, NULL, 0, (uint8_t *)&a, sizeof(a));
        }
      }
      lastContext = reading;
    }

    if (millis() - timedelay > 3000) {
      if (!reading)
      {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wm.resetSettings();
        ESP.restart();
      }
    }

  }
  else {
    lastDebounceTime = millis();
    timedelay = millis();
    lastContext = reading;
  }

}


void loop() {
  if (!serverConnect) {
    if (millis() - lastLoop >= 2000) {
      getIPHomeCenter();
      lastLoop = millis();
    }
  } else {
    coap.loop();
  }
  checkButton();
}
