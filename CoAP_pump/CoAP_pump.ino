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

// Process CoAP data from Home Center
void callback_control(CoapPacket &packet, IPAddress ip, int port) {
  // Convert payload to string
  uint8_t p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  String message = String((char *)&p);
  Serial.println(message);

  // Update PUMPSTATE based on the received message
  if (message.equals("0"))
    PUMPSTATE = true;
  else if (message.equals("1"))
    PUMPSTATE = false;

  // Control the pump and send response
  if (PUMPSTATE) {
    digitalWrite(PUMP, HIGH);
    coap.sendResponse(ip, port, packet.messageid, "1");
    Serial.println("Pump OFF");
  } else {
    digitalWrite(PUMP, LOW);
    coap.sendResponse(ip, port, packet.messageid, "0");
    Serial.println("Pump ON");
  }
}

// Handle response data from CoAP server
// Parameters:
//   - packet: the CoapPacket object containing the response packet
//   - ip: the IP address of the server
//   - port: the port number of the server
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  // Print CoAP response message
  Serial.println("[Coap Response got]");

  // Extract the payload data
  uint8_t p;
  memcpy(&p, packet.payload, packet.payloadlen);

  // Print the payload data
  Serial.println(p);
}

/**
 * Send UDP broadcast file.
 */
void getIPHomeCenter() {
  // Print a message to indicate connecting to Home Center
  Serial.println("Connect to Home Center");

  // Create a message to be broadcasted
  String msg = nameTag + "/0";

  // Broadcast the message using UDP
  udp.broadcast(msg.c_str());
}

// Set up the Arduino board
void setup() {
  Serial.begin(115200); // Initialize the serial communication at 115200 baud rate
  pinMode(TRIGGER_PIN, INPUT_PULLUP); // Set the trigger pin as input with internal pull-up resistor

  // Connect to the WiFi network
  bool res = wm.autoConnect("TM-NgocHung CoAP pump", "12345678"); // Attempt to connect to the specified WiFi network
  if (!res) {
    Serial.println("Failed to connect"); // Print an error message if failed to connect
  } else {
    Serial.println("Connected...yay :)"); // Print a success message if connected
  }

  // Set up UDP file handling
  if (udp.listen(1234)) { // Start listening for UDP packets on port 1234
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP()); // Print the local IP address
    udp.onPacket([](AsyncUDPPacket packet) { // Register a callback function to handle UDP packets
      Serial.print("UDP Packet Type: ");
      Serial.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast" : "Unicast");
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
          Serial.println("Connected to server");
          Serial.println(centralIP);
          udp.close();
        }
      }
    });
  }

  pinMode(PUMP, OUTPUT); // Set the pump pin as output
  digitalWrite(PUMP, HIGH); // Turn on the pump
  PUMPSTATE = true;

  coap.server(callback_control, "control"); // Set up the CoAP server with the specified callback function and endpoint
  Serial.println("Setup Response Callback");
  coap.response(callback_response); // Set up the CoAP response callback

  coap.start(); // Start the CoAP server/client
}

// This function handles the button press to toggle the LED state and setup Wi-Fi.
void checkButton() {
  bool reading = digitalRead(TRIGGER_PIN);
  
  // Create a JSON document to store data
  DynamicJsonDocument doc(100);
  
  // Check if the button is pressed
  if (!reading) {
    
    // Check the debounce delay
    if ((millis() - lastDebounceTime) > debounceDelay) {
      
      // Compare the current reading with the previous reading
      if (lastContext != reading) {
        // Toggle the LED state and update the JSON document
        if (!PUMPSTATE) {
          digitalWrite(PUMP, HIGH);
          Serial.println("OFF");
          doc[nameTag] = 0;
          PUMPSTATE = true;
        } else {
          digitalWrite(PUMP, LOW);
          Serial.println("ON");
          doc[nameTag] = 1;
          PUMPSTATE = false;
        }
        
        // Convert the JSON document to a string
        String data;
        serializeJson(doc, data);
        char a[data.length() + 1];
        data.toCharArray(a, data.length() + 1);
        
        Serial.print("Send : ");
        Serial.println(data);
        
        // Send the JSON data using CoAP
        int rq = coap.send(centralIP, 5683, "sensor", COAP_CON, COAP_PUT, NULL, 0, (uint8_t *)&a, sizeof(a));
      }
    }
    
    // Check if the button is held for more than 3 seconds
    if (millis() - timedelay > 3000) {
      if (!reading) {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        
        // Reset Wi-Fi settings and restart the device
        wm.resetSettings();
        ESP.restart();
      }
    }
    
  } else {
    // Update the last debounce time and context
    lastDebounceTime = millis();
    timedelay = millis();
    lastContext = reading;
  }
}


/**
 * @brief The main loop function.
 * 
 * This function is responsible for executing the main logic of the program.
 */
void loop() {
  // Check if the server is not connected
  if (!serverConnect) {
    // Check if the specified time has passed since the last loop execution
    if (millis() - lastLoop >= 2000) {
      // Get the IP address of the home center
      getIPHomeCenter();
      // Update the last loop time
      lastLoop = millis();
    }
  } else {
    // Execute the loop logic for the CoAP protocol
    coap.loop();
  }
  // Check the state of the button
  checkButton();
}
