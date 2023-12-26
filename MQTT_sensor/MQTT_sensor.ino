#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ESPAsyncUDP.h>
#define TRIGGER_PIN 23

WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wf;
String nameTag = "home_sensor";

AsyncUDP udp;
IPAddress centralIP;
bool serverConnect = false;
const String cmd = "BUSTER CALL";
uint8_t period = 2;

// Callback + change period
void callback_period(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  // Update periodValue with received value
  if (strcmp(topic, "ProjectIoT/1/period") == 0)
  {
    // Convert payload to integer for period value
    String payloadStr = "";
    for (int i = 0; i < length; i++) {
      payloadStr += (char)payload[i];
    }
    period = payloadStr.toInt();
  }
}

void getIPHomeCenter() {
  Serial.println("Connect to Home Center");
  String msg = nameTag + "/1";
  udp.broadcast(msg.c_str());
}
void setup() {
  Serial.begin(115200);
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  bool res = wf.autoConnect("TM-NgocHung", "12345678");
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("connected...yay :)");
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
        String a = String((char *)packet.data());
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
  client.subscribe("IoT/sensor/1");
  client.setServer(centralIP, 1883);
  client.setCallback(callback_period);
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
        wf.resetSettings();
        ESP.restart();
      }

      // start portal w delay
      Serial.println("Starting config portal");
      wf.setConfigPortalTimeout(120);

      if (!wf.startConfigPortal("OnDemandAP", "password")) {
        Serial.println("failed to connect or hit timeout");
        delay(3000);
        ESP.restart();
      } else {
        //if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");
      }
    }
  }
}

void loop() {
  if (!serverConnect) {
    getIPHomeCenter();
  }
  else {
    DynamicJsonDocument doc(200);
    uint8_t temp = random(0, 100);
    uint8_t humi = random(0, 100);
    doc[nameTag]["period"] = period;
    doc[nameTag]["temprature"] = temp;
    doc[nameTag]["humidity"] = humi;
    Serial.print("Send : ");
    String data;
    serializeJson(doc, data);
    Serial.println(data);
    client.publish("ProjectIoT/1/sensor", data.c_str());
    int delayValue = period * 1000;
    delay(delayValue);
  }
  client.loop();
  checkButton();
}
