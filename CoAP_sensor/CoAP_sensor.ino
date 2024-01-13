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

// Process CoAP data from Home Center
void callback_periodSensor(CoapPacket &packet, IPAddress ip, int port) {
  // Copy packet payload to a temporary buffer
  uint8_t p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;

  // Convert the payload buffer to a string
  String message = String((char *)&p);

  // Print the message to the serial monitor
  Serial.println(message);

  // Check if the payload can be converted to a valid floating-point number
  if (atof((char *)&p) > 0) {
    // Convert the payload to a floating-point number and update the 'period' variable
    period = atof((char *)&p);

    // Send a success response back to the client
    coap.sendResponse(ip, port, packet.messageid, "change period successfully");
  } else {
    // Send an error response back to the client
    coap.sendResponse(ip, port, packet.messageid, "invalid period");
  }
}

/**
 * Handle CoAP response data from the server.
 * 
 * @param packet - The CoapPacket object containing the response data.
 * @param ip - The IP address of the server.
 * @param port - The port number of the server.
 */
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  // Print debug message
  Serial.println("[Coap Response got]");

  // Extract payload data
  uint8_t p;
  memcpy(&p, packet.payload, packet.payloadlen);

  // Print payload data
  Serial.println(p);

  // Set server connection flag
  serverConnect = true;
}

// Send UDP broadcast file
void getIPHomeCenter() {
  // Print message to serial console
  Serial.println("Connect to Home Center");

  // Create message string
  String msg = nameTag + "/0";

  // Broadcast message using UDP
  udp.broadcast(msg.c_str());
}

/**
 * Set up the system.
 * - Configure WiFi mode.
 * - Initialize serial communication.
 * - Set pin modes.
 * - Connect to WiFi.
 * - Set up UDP listener.
 * - Start CoAP server.
 */
void setup() {
  // Configure WiFi mode
  WiFi.mode(WIFI_STA);

  // Initialize serial communication
  Serial.begin(115200);

  // Set pin modes
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(CONTEXT_PIN, INPUT_PULLUP);

  // Connect to WiFi
  bool res = wm.autoConnect("TM-NgocHung CoAP sensor", "12345678");
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("Connected... yeey :)");
  }

  // Set up UDP listener
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
            Serial.println("Connected to server");
            Serial.println(centralIP);
            udp.close();
          }
          else {
            Serial.println(String((char *)packet.data()));
            Serial.println("Code does not match");
          }
        }
      }
    });
  }

  // Start CoAP server
  coap.server(callback_periodSensor, "control");

  // Set client response callback
  coap.response(callback_response);

  // Start CoAP server/client
  coap.start();
}

/**
 * Check the button to reset the WiFi configuration.
 */
void checkButton() {
  // Check for button press
  if (digitalRead(TRIGGER_PIN) == LOW) {
    // Poor man's debounce/press-hold, code not ideal for production
    delay(50);
    if (digitalRead(TRIGGER_PIN) == LOW) {
      Serial.println("Button Pressed");
      // Still holding button for 3000 ms, reset settings, code not ideal for production
      delay(3000);  // Reset delay hold
      if (digitalRead(TRIGGER_PIN) == LOW) {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wm.resetSettings();
        ESP.restart();
      }

      // Start portal with delay
      Serial.println("Starting config portal");
      wm.setConfigPortalTimeout(120);

      if (!wm.startConfigPortal("OnDemandAP", "password")) {
        Serial.println("Failed to connect or hit timeout");
        delay(3000);
        // ESP.restart();
      } else {
        // If you get here you have connected to the WiFi
        Serial.println("Connected...yeey :)");
      }
    }
  }
}

void loop() {
  bool reading = digitalRead(CONTEXT_PIN);

  // Change temperature scenario
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

  // Send data periodically
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
      doc[nameTag]["temperature"] = temp;
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
