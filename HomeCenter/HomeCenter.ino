#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <ArduinoJson.h>
#include <String.h>
#include <PubSubClient.h>
#include <PicoMQTT.h>
#include "AsyncUDP.h"

#define MAX_TEMP 60
#define TRIGGER_PIN 23

const char *mqtt_server = "mqtt.thingsboard.cloud";  //Thingboards Platform
const char *token = "ICzOXamZl4ZjWTuIdBHh";          //ACCESS TOKEN

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port);

// CoAP server endpoint url callback
void callback_sensor(CoapPacket &packet, IPAddress ip, int port);

WiFiManager wm;
PicoMQTT::Server mqtt;
WiFiUDP Udp;
Coap coap(Udp);
AsyncUDP udp;
WiFiClient espClient;
PubSubClient client(espClient);
StaticJsonDocument<1000> listDevice;

unsigned long lastConnectMQTTserver = 0;
int expiredMQTT = 5000;
const char *cmd = "BUSTER CALL";
const char *topicSuper = "v1/devices/me/telemetry";
bool automation = false;
bool coapAlarm = false;
bool MQTTAlarm = false;

// CoAP server endpoint URL
void callback_sensor(CoapPacket &packet, IPAddress ip, int port) {
  Serial.print("Coap data:");

  uint8_t p[packet.payloadlen + 1];
  memcpy(&p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  String a = String((char *)&p);
  Serial.println(a);
  coap.sendResponse(ip, port, packet.messageid, "1");

  if (automation) {
    IPAddress ipDevice;
    StaticJsonDocument<100> buffer;
    deserializeJson(buffer, a);
    JsonObject documentRoot = listDevice.as<JsonObject>();
    for (JsonPair keyValue : documentRoot) {
      if ((listDevice[keyValue.key()]["P"].as<unsigned int>()) == 0) {
        //Serial.println(listDevice[keyValue.key()]["IP"].as<String>());
        ipDevice.fromString(listDevice[keyValue.key()]["IP"].as<String>());
        if (buffer[keyValue.key()]["temprature"]) {
          uint8_t temp = buffer[keyValue.key()]["temprature"].as<uint8_t>();
          if (temp > MAX_TEMP) {
            //send to pump or switch
            coapAlarm = true;
          }
          else coapAlarm = false;
        }
        if (coapAlarm) coap.put(ipDevice, 5683, "control", "0");
      }
    }
  }

  //publish to platform => need fix
  client.publish(topicSuper, (char *)&p);
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Coap Response got]");
  uint8_t p[packet.payloadlen + 1];
  memcpy(&p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  String a = String((char *)&p);
  Serial.println(a);
}

//mqtt callback
void callback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  //handle call back

  DynamicJsonDocument command(100);
  char p[length];
  memcpy(&p, payload, length);
  deserializeJson(command, p);
  String sig = command["params"].as<String>();
  String id = command["method"].as<String>();
  Serial.println(sig);
  serializeJson(command, Serial);
  JsonObject documentRoot = listDevice.as<JsonObject>();
  for (JsonPair keyValue : documentRoot) {
    if (id.equals(keyValue.key().c_str())) {
      IPAddress ipDevice;
      unsigned int protocol = listDevice[keyValue.key()]["P"].as<unsigned int>();
      ipDevice.fromString(listDevice[keyValue.key()]["IP"].as<String>());
      if (protocol == 0) {
        coap.put(ipDevice, 5683, "control", sig.c_str());
      } else if (protocol == 1) {
        mqtt.publish(keyValue.key().c_str(), sig);
      }
    }
  }
}

//MQTT server retry connect
void reconnect() {
  // try reconnected after 5 seconds
  if (millis() - lastConnectMQTTserver >= 5000) {
    if (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      // Create a random client ID
      String clientId = "ESP32Client-";
      clientId += String(random(0xffff), HEX);
      // Attempt to connect
      if (client.connect(clientId.c_str(), token, "")) {
        Serial.println("connected");
        automation = false;
        client.subscribe("v1/devices/me/rpc/request/+");
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again after 5 seconds");
      }
    }
    lastConnectMQTTserver = millis();
  }
}

void checkButton() {
  // check for button press
  if ( digitalRead(TRIGGER_PIN) == LOW ) {
    // poor mans debounce/press-hold, code not ideal for production
    delay(50);
    if ( digitalRead(TRIGGER_PIN) == LOW ) {
      Serial.println("Button Pressed");
      // still holding button for 3000 ms, reset settings, code not ideaa for production
      delay(3000); // reset delay hold
      if ( digitalRead(TRIGGER_PIN) == LOW ) {
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

void setup() {
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, INPUT);

  //auto begin Access Point to config Wifi
  bool res = wm.autoConnect("TM-NgocHung Home Center", "12345678");
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("connected...yay :)");
  }

  //listen UDP PDU
  if (udp.listen(1234)) {
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP());
    udp.onPacket([](AsyncUDPPacket packet) {
      if (packet.isBroadcast()) {
        //handle data broadcast from client
        Serial.print(packet.remoteIP());
        Serial.print(" ");
        Serial.println((char *)packet.data());
        //save client to cache
        String data = String((const char *)packet.data());
        String name;
        uint8_t protocol;
        if (data.indexOf('/') >= 0) {
          name = data.substring(0, data.indexOf('/'));
          data.remove(0, data.indexOf('/') + 1);
          protocol = data.toInt();
        }
        listDevice[name]["IP"] = packet.remoteIP();
        listDevice[name]["P"] = protocol;
        serializeJson(listDevice, Serial);
        //send to remoteIP Home Center information
        packet.write((uint8_t *)cmd, strlen(cmd));
      }
    });
  }

  //connect MQTT server online
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  //start CoAP
  Serial.println("Setup Callback sensor");
  coap.server(callback_sensor, "sensor");
  Serial.println("Setup Response Callback");
  coap.response(callback_response);
  coap.start();

  //start local MQTT Broker
  mqtt.subscribe("#", [](const char *topic, const char *payload) {
    //handle Local MQTT
    Serial.printf("MQTT locol '%s': %s\n", topic, payload);
    String a = String(payload);

    if (automation) {
      StaticJsonDocument<100> buffer;
      deserializeJson(buffer, a);
      JsonObject documentRoot = listDevice.as<JsonObject>();
      for (JsonPair keyValue : documentRoot) {
        if ((listDevice[keyValue.key()]["P"].as<unsigned int>()) == 1) {
          if (buffer[keyValue.key()]["temprature"]) {
            uint8_t temp = buffer[keyValue.key()]["temprature"].as<uint8_t>();
            if (temp > MAX_TEMP) {
              //send to pump or switch
              MQTTAlarm = true;
            }
            else MQTTAlarm = false;
          }
          if (MQTTAlarm) mqtt.publish(keyValue.key().c_str(), "0");
        }
      }
    }
    client.publish(topicSuper, payload);
  });

  mqtt.begin();
}

//automation if not connect platform

void loop() {
  if (!client.connected()) {
    reconnect();
    automation = true;
  }
  client.loop();
  coap.loop();
  mqtt.loop();
  checkButton();
}
