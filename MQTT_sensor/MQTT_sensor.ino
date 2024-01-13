#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ESPAsyncUDP.h>

#define TRIGGER_PIN 0
#define CONTEXT_PIN 2

bool context = false;
bool lastContext = false;
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers

WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wf;
String nameTag = "home_sensor";

AsyncUDP udp;
IPAddress centralIP;
bool serverConnect = false;
const String cmd = "BUSTER CALL";
unsigned long lastLoop = 0;
uint8_t period = 2;

// Callback function to handle received messages and update period value
void callback_period(char* topic, byte* payload, unsigned int length) {
  // Print received message topic
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // Print received message content
  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Update period value with received value
  if (strcmp(topic, "home_sensor") == 0)
  {
    // Convert payload to integer for period value
    String payloadStr = "";
    for (int i = 0; i < length; i++) {
      payloadStr += (char)payload[i];
    }
    if(payloadStr.toInt() != 0 ) period = payloadStr.toInt();
  }
}

// Function to get the IP of the Home Center
void getIPHomeCenter() {
  // Print message to connect to Home Center
  Serial.println("Connect to Home Center");

  // Create the message to broadcast
  String msg = nameTag + "/1";

  // Broadcast the message using UDP
  udp.broadcast(msg.c_str());
}
void setup() {
  // Initialize serial communication
  Serial.begin(115200);

  // Set pin modes
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(CONTEXT_PIN, INPUT_PULLUP);

  // Connect to WiFi
  bool res = wf.autoConnect("TM-NgocHung MQTT sensor", "12345678");

  // Check if connection is successful
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("connected...yay :)");
  }

  // Handle UDP file
  if (udp.listen(1234)) {
    Serial.print("UDP Listening on IP: ");
    Serial.println(WiFi.localIP());

    // Callback function for UDP packet reception
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

      // Check if packet is not broadcast or multicast
      if (!packet.isBroadcast() && !packet.isMulticast()) {
        // Extract the first 11 characters from the packet data
        char p[11];
        memcpy(&p, (char *)packet.data(), 11);
        String a = String(p);

        // Check if the extracted string matches the command
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
}

// This function checks if a button is pressed to reset the WiFi configuration.
// It includes a debounce mechanism to prevent false triggers.
// If the button is held for 3 seconds, the WiFi settings are reset and the ESP restarts.
// If the button is pressed momentarily, a configuration portal is started to connect to WiFi.

void checkButton() {
  // Check for button press
  if (digitalRead(TRIGGER_PIN) == LOW) {
    // Debounce mechanism
    delay(50);
    if (digitalRead(TRIGGER_PIN) == LOW) {
      Serial.println("Button Pressed");
      // Check if the button is held for 3 seconds
      delay(3000);
      if (digitalRead(TRIGGER_PIN) == LOW) {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wf.resetSettings();
        ESP.restart();
      }

      // Start the configuration portal with a delay
      Serial.println("Starting config portal");
      wf.setConfigPortalTimeout(120);

      if (!wf.startConfigPortal("OnDemandAP", "password")) {
        Serial.println("Failed to connect or hit timeout");
        delay(3000);
        ESP.restart();
      } else {
        // If you get here, you have successfully connected to WiFi
        Serial.println("Connected...yeey :)");
      }
    }
  }
}

unsigned long lastConnectMQTTserver = 0;

// Reconnect to MQTT broker
void reconnect() {
  // Try reconnecting after 5 seconds
  if (millis() - lastConnectMQTTserver >= 5000) {
    if (!client.connected()) {
      Serial.print("Attempting MQTT connection..."); // Print message to Serial Monitor
      String clientId = "ESP8266Client-"; // Create a random client ID
      clientId += String(random(0xffff), HEX);
      client.setServer(centralIP, 1883); // Set server and port
      client.setCallback(callback_period); // Set callback function
      if (client.connect(clientId.c_str())) { // Attempt to connect
        Serial.println("connected"); // Print message to Serial Monitor
        client.subscribe("home_sensor"); // Subscribe to topic
      } else {
        Serial.print("failed, rc=");
        Serial.print(client.state()); // Print error code to Serial Monitor
        Serial.println(" try again after 5 seconds"); // Print message to Serial Monitor
      }
    }
    lastConnectMQTTserver = millis();
  }
}
// This function is the main loop of the program.
// It checks the client connection, reads a digital pin,
// changes the temperature scenario, and sends data periodically.

void loop() {
  // Check if the client is connected
  if (!client.connected()) {
    reconnect();
  }

  // Read the digital pin
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
      Serial.println(data);
      client.publish("ProjectIoT/1/sensor", data.c_str());
    }
    lastLoop = millis();
  }

  // Keep the MQTT client running
  client.loop();

  // Check the button state
  checkButton();
}
