#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <ArduinoJson.h>
#include <String.h>
#include <PubSubClient.h>
#include <PicoMQTT.h>
#include "AsyncUDP.h"

//khởi tạo các biến toàn cục lưu dữ liệu
#define TRIGGER_PIN 23
int MAX_TEMP = 60;
const char *mqtt_server = "thingsboard.hust-2slab.org";  //Thingboards Platform
const char *token = "nJIx4zHJtkuffvtJjoN8";          //ACCESS TOKEN

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

/**
 * Callback function for CoAP server endpoint URL.
 * Processes incoming CoAP packets and performs necessary actions.
 * @param packet The CoapPacket object representing the received packet.
 * @param ip The IP address of the sender.
 * @param port The port number of the sender.
 */
void callback_sensor(CoapPacket &packet, IPAddress ip, int port) {
  // Print CoAP data
  Serial.print("Coap data:");

  // Copy payload to a new buffer
  uint8_t p[packet.payloadlen + 1];
  memcpy(&p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;

  // Convert payload to string
  String a = String((char *)&p);
  Serial.println(a);

  // Send CoAP response with messageid "1"
  coap.sendResponse(ip, port, packet.messageid, "1");

  // Process automation logic if enabled
  if (automation) {
    IPAddress ipDevice;
    StaticJsonDocument<100> buffer;

    // Deserialize JSON payload
    deserializeJson(buffer, a);
    JsonObject documentRoot = listDevice.as<JsonObject>();

    // Iterate through devices in the list
    for (JsonPair keyValue : documentRoot) {
      if ((listDevice[keyValue.key()]["P"].as<unsigned int>()) == 0) {
        ipDevice.fromString(listDevice[keyValue.key()]["IP"].as<String>());

        // Check for temperature data
        if (buffer[keyValue.key()]["temperature"]) {
          uint8_t temp = buffer[keyValue.key()]["temperature"].as<uint8_t>();

          // Check if temperature exceeds the maximum threshold
          if (temp > MAX_TEMP) {
            // Set coapAlarm flag
            coapAlarm = true;
          } else {
            coapAlarm = false;
          }
        }

        // Control the device if coapAlarm is set
        if (coapAlarm) {
          coap.put(ipDevice, 5683, "control", "0");
        }
      }
    }
  }

  // Publish payload to platform
  client.publish(topicSuper, (char *)&p);
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  // Print message indicating that a CoAP response was received
  Serial.println("[Coap Response got]");

  // Create a buffer to hold the payload data
  uint8_t payloadBuffer[packet.payloadlen + 1];

  // Copy the payload data into the buffer
  memcpy(payloadBuffer, packet.payload, packet.payloadlen);

  // Null-terminate the buffer to convert it to a C-style string
  payloadBuffer[packet.payloadlen] = '\0';

  // Convert the payload buffer to a String object
  String payloadString = String((char *)&payloadBuffer);

  // Print the payload string
  Serial.println(payloadString);
}

/**
 * This function is the callback for the MQTT messages.
 * It handles the received messages and performs appropriate actions based on the message content.
 * @param topic The topic of the MQTT message.
 * @param payload The payload of the MQTT message.
 * @param length The length of the payload.
 */
void callback(char *topic, byte *payload, unsigned int length) {
  // Print the topic and payload of the received message
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Deserialize the payload into a JSON document
  DynamicJsonDocument command(100);
  char p[length];
  memcpy(&p, payload, length);
  deserializeJson(command, p);

  // Extract the required values from the JSON document
  String sig = command["params"].as<String>();
  String id = command["method"].as<String>();
  Serial.println(sig);
  serializeJson(command, Serial);

  // Perform actions based on the message content
  if (id.equals("home_center")) {
    // Update the MAX_TEMP value
    MAX_TEMP = command["params"].as<signed int>();
  } else {
    // Iterate through the list of devices and perform appropriate actions based on the device protocol
    JsonObject documentRoot = listDevice.as<JsonObject>();
    for (JsonPair keyValue : documentRoot) {
      if (id.equals(keyValue.key().c_str())) {
        // Get the device IP address and protocol
        IPAddress ipDevice;
        unsigned int protocol = listDevice[keyValue.key()]["P"].as<unsigned int>();
        ipDevice.fromString(listDevice[keyValue.key()]["IP"].as<String>());

        // Perform actions based on the device protocol
        if (protocol == 0) {
          // Send CoAP message to the device
          coap.put(ipDevice, 5683, "control", sig.c_str());
        } else if (protocol == 1) {
          // Publish MQTT message to the device
          mqtt.publish(keyValue.key().c_str(), sig);
        }
      }
    }
  }
}

// MQTT server retry connect
void reconnect() {
  // Check if it's time to attempt reconnection
  if (millis() - lastConnectMQTTserver >= 5000) {
    // Check if client is not connected
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

/**
 * Check if a button is pressed and perform corresponding actions.
 */
void checkButton() {
  // Check for button press
  if (digitalRead(TRIGGER_PIN) == LOW) {
    // Poor man's debounce/press-hold, code not ideal for production
    delay(50);
    if (digitalRead(TRIGGER_PIN) == LOW) {
      // Button Pressed
      Serial.println("Button Pressed");

      // Still holding button for 3000 ms, reset settings, code not ideal for production
      delay(3000); // Reset delay hold
      if (digitalRead(TRIGGER_PIN) == LOW) {
        // Button Held
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wm.resetSettings();
        ESP.restart();
      }

      // Start portal with delay
      Serial.println("Starting config portal");
      wm.setConfigPortalTimeout(120);

      if (!wm.startConfigPortal("OnDemandAP", "password")) {
        // Failed to connect or hit timeout
        Serial.println("failed to connect or hit timeout");
        delay(3000);
        // ESP.restart();
      } else {
        // If you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
      }
    }
  }
}

void setup() {
  // Initialize Serial communication
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, INPUT);

  // Auto connect to WiFi Access Point for configuration
  bool res = wm.autoConnect("TM-NgocHung Home Center", "12345678");
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("Connected to WiFi");
  }

  // Listen for UDP packets
  if (udp.listen(1234)) {
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP());
    udp.onPacket([](AsyncUDPPacket packet) {
      if (packet.isBroadcast()) {
        // Handle data broadcast from client
        Serial.print(packet.remoteIP());
        Serial.print(" ");
        Serial.println((char *)packet.data());

        // Save client information to cache
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

        // Send Home Center information to remote IP
        packet.write((uint8_t *)cmd, strlen(cmd));
      }
    });
  }

  // Connect to MQTT server online
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  // Start CoAP server
  Serial.println("Setup Callback sensor");
  coap.server(callback_sensor, "sensor");
  Serial.println("Setup Response Callback");
  coap.response(callback_response);
  coap.start();

  // Subscribe to local MQTT topics
  mqtt.subscribe("#", [](const char *topic, const char *payload) {
    // Handle local MQTT messages
    Serial.printf("MQTT local '%s': %s\n", topic, payload);
    String a = String(payload);

    if (automation) {
      StaticJsonDocument<100> buffer;
      deserializeJson(buffer, a);
      JsonObject documentRoot = listDevice.as<JsonObject>();

      // Iterate through devices and check for temperature values
      for (JsonPair keyValue : documentRoot) {
        if ((listDevice[keyValue.key()]["P"].as<unsigned int>()) == 1) {
          if (buffer[keyValue.key()]["temperature"]) {
            uint8_t temp = buffer[keyValue.key()]["temperature"].as<uint8_t>();
            if (temp > MAX_TEMP) {
              // Send alarm to pump or switch
              MQTTAlarm = true;
            } else {
              MQTTAlarm = false;
            }
          }
          if (MQTTAlarm) {
            mqtt.publish(keyValue.key().c_str(), "0");
          }
        }
      }
    }
    client.publish(topicSuper, payload);
  });

  // Begin MQTT client
  mqtt.begin();
}

//automation if not connect platform

// Function to handle the main loop of the program
void loop() {
  // Check if the client is not connected
  if (!client.connected()) {
    // Attempt to reconnect
    reconnect();
    
    // Enable automation flag
    automation = true;
  }
  
  // Perform loop operations for client, coap, and mqtt
  client.loop();
  coap.loop();
  mqtt.loop();
  
  // Check for button press
  checkButton();
}
