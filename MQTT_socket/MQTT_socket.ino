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

// Callback
void callback_period(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  Serial.print("Message:");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  // Control power pocket
  if (payload[0] == '1')
  {
    Serial.println("ON");
    digitalWrite(Led_Pin, LOW);
    socket_state = 1;
  }
  else if (payload[0] == '0')
  {
    Serial.println("OFF");
    digitalWrite(Led_Pin, HIGH);
    socket_state = 0;
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

  bool res = wf.autoConnect("TM-NgocHung MQTT socket", "12345678");
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("connected...yay :)");
  }

  //xử lý tập tin UDP
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
  pinMode(Led_Pin, OUTPUT);
  digitalWrite(Led_Pin, HIGH);
  socket_state = 0;
  client.subscribe("home_socket");
  client.setCallback(callback_period);
}

//xử lý nút bấm thay đổi trạng thái led và setup wifi
void checkButton() {
  bool reading = digitalRead(TRIGGER_PIN);
  DynamicJsonDocument doc(100);
  if (!reading) {
    if ((millis() - lastDebounceTime) > debounceDelay) {
      if (!reading) {
        if (lastContext != reading)
        {
          if (socket_state == 1)
          {
            digitalWrite(Led_Pin, HIGH);
            Serial.println("OFF");
            doc[nameTag] = 0;
            socket_state = 0;
          }
          else
          {
            digitalWrite(Led_Pin, LOW);
            Serial.println("ON");
            doc[nameTag] = 1;
            socket_state = 1;
          }
          Serial.print("Send : ");
          String data;
          serializeJson(doc, data);
          Serial.println(data);
          client.publish("ProjectIoT/1/sensor", data.c_str());
        }
      }
      lastContext = reading;
    }

    if (millis() - timedelay > 3000) {
      if (!reading)
      {
        Serial.println("Button Held");
        Serial.println("Erasing Config, restarting");
        wf.resetSettings();
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


unsigned long lastConnectMQTTserver = 0;

//kết nối lại MQTT broker
void reconnect() {
  // try reconnected after 5 seconds
  if (millis() - lastConnectMQTTserver >= 5000) {
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
    lastConnectMQTTserver = millis();
  }
}
unsigned long lastLoop = 0;
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  if (!serverConnect) {
    if (millis() - lastLoop >= 2000) {
      getIPHomeCenter();
      lastLoop = millis();
    }
  }
  client.loop();
  checkButton();
}
