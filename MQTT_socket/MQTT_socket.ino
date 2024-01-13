#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ESPAsyncUDP.h>

#define TRIGGER_PIN 0
#define Led_Pin 2

WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wf;
String nameTag = "home_socket";
int socket_state;
AsyncUDP udp;
IPAddress centralIP;
bool serverConnect = false;
const String cmd = "BUSTER CALL";
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long timedelay;
bool context = false;
bool lastContext = false;

/**
 * Callback function for handling MQTT message arrival.
 * 
 * @param topic   The topic of the MQTT message.
 * @param payload The payload of the MQTT message.
 * @param length  The length of the payload.
 */
void callback_period(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Control led
  if (payload[0] == '1') {
    Serial.println("ON");
    digitalWrite(Led_Pin, LOW);
    socket_state = 1;
  } else if (payload[0] == '0') {
    Serial.println("OFF");
    digitalWrite(Led_Pin, HIGH);
    socket_state = 0;
  }
}

// This function sends a broadcast message to connect to the Home Center

void getIPHomeCenter() {
  // Print a message to the Serial Monitor
  Serial.println("Connect to Home Center");

  // Create a string message to send as a broadcast
  String msg = nameTag + "/1";

  // Send the broadcast message
  udp.broadcast(msg.c_str());
}

void setup() {
  Serial.begin(115200); // Initialize the serial communication
  pinMode(TRIGGER_PIN, INPUT_PULLUP); // Set the trigger pin as input with pull-up resistor enabled

  // Attempt to connect to the WiFi network
  bool res = wf.autoConnect("TM-NgocHung MQTT socket", "12345678");
  if (!res) {
    Serial.println("Failed to connect"); // Print an error message if connection fails
  } else {
    Serial.println("connected...yay :)"); // Print a success message if connection succeeds
  }

  // Set up UDP file handling
  if (udp.listen(1234)) {
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP()); // Print the local IP address
    udp.onPacket([](AsyncUDPPacket packet) {
      Serial.print("UDP Packet Type: ");
      Serial.print(packet.isBroadcast() ? "Broadcast" : packet.isMulticast() ? "Multicast"
                   : "Unicast");
      Serial.print(", From: ");
      Serial.print(packet.remoteIP()); // Print the remote IP address
      Serial.print(":");
      Serial.print(packet.remotePort()); // Print the remote port
      Serial.print(", To: ");
      Serial.print(packet.localIP()); // Print the local IP address
      Serial.print(":");
      Serial.print(packet.localPort()); // Print the local port
      Serial.print(", Length: ");
      Serial.print(packet.length()); // Print the packet length
      Serial.print(", Data: ");
      Serial.write(packet.data(), packet.length()); // Print the packet data
      Serial.println();
      if (!packet.isBroadcast() && !packet.isMulticast()) {
        char p[11];
        memcpy(&p, (char *)packet.data(), 11);
        String a = String(p);
        if (a.equals(cmd)) {
          centralIP = packet.remoteIP(); // Set the central IP address
          serverConnect = true; // Set the server connection status to true
          Serial.println("connected to server");
          Serial.println(centralIP);
          udp.close(); // Close the UDP connection
        }
      }
    });
  }

  pinMode(Led_Pin, OUTPUT); // Set the LED pin as output
  digitalWrite(Led_Pin, HIGH); // Turn on the LED
  socket_state = 0; // Set the socket state to 0
  client.subscribe("home_socket"); // Subscribe to the "home_socket" topic
  client.setCallback(callback_period); // Set the callback function for message reception
}

// This function checks the state of a button and performs actions accordingly.
// It toggles the state of an LED and sends a message over MQTT when the button is pressed.
// If the button is held for more than 3 seconds, it erases the configuration and restarts the device.
void checkButton() {
  bool reading = digitalRead(TRIGGER_PIN);  // Read the state of the button
  DynamicJsonDocument doc(100);  // Create a JSON document

  if (!reading) {
    // Debounce the button
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (!reading) {
        if (lastContext != reading) {
          if (socket_state == 1) {
            digitalWrite(Led_Pin, HIGH);  // Turn off the LED
            Serial.println("OFF");
            doc[nameTag] = 0;
            socket_state = 0;
          } else {
            digitalWrite(Led_Pin, LOW);  // Turn on the LED
            Serial.println("ON");
            doc[nameTag] = 1;
            socket_state = 1;
          }
          Serial.print("Send : ");
          String data;
          serializeJson(doc, data);  // Serialize the JSON document to a string
          Serial.println(data);
          client.publish("ProjectIoT/1/sensor", data.c_str());  // Publish the message over MQTT
        }
      }
      lastContext = reading;
    }

    // Check if the button is held for more than 3 seconds
    if (millis() - timedelay > 3000) {
      if (!reading) {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wf.resetSettings();  // Erase the WiFi configuration
        ESP.restart();  // Restart the device
      }
    }
  } else {
    lastDebounceTime = millis();
    timedelay = millis();
    lastContext = reading;
  }
}


unsigned long lastConnectMQTTserver = 0;

// Reconnects to the MQTT broker
void reconnect() {
  // Check if it has been 5 seconds since the last connection attempt
  if (millis() - lastConnectMQTTserver >= 5000) {
    // Check if the client is not connected
    if (!client.connected()) {
      Serial.print("Attempting MQTT connection...");
      // Create a random client ID
      String clientId = "ESP8266Client-";
      clientId += String(random(0xffff), HEX);
      client.setServer(centralIP, 1883);
      // Attempt to connect
      if (client.connect(clientId.c_str())) {
        Serial.println("connected");
        client.subscribe("home_socket");
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again after 5 seconds");
      }
    }
    // Update the last connection attempt time
    lastConnectMQTTserver = millis();
  }
}
unsigned long lastLoop = 0;
// Function to handle the main loop of the program
void loop() {
  // Check if the client is not connected and reconnect if necessary
  if (!client.connected()) {
    reconnect();
  }

  // Check if the server is not connected
  if (!serverConnect) {
    // Check if enough time has elapsed since the last loop
    if (millis() - lastLoop >= 2000) {
      // Call the function to get the IP of the Home Center
      getIPHomeCenter();
      // Update the last loop time
      lastLoop = millis();
    }
  }

  // Call the loop function of the client
  client.loop();

  // Check the state of the button
  checkButton();
}
