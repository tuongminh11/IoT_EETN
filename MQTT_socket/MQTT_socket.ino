#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
const char* ssid = "LingLing";
const char* password = "menhmonghoaha";
const char* mqtt_server = "192.168.43.25";
#define Led_Pin 20
#define Led_State 8
//const char* mqtt_user = "Hunggg";
//const char* mqtt_pass = "Lh200819";
WiFiClient espClient;
PubSubClient client(espClient);
WiFiManager wf;
String nameTag;
uint8_t period = 3;
WiFiManagerParameter set_device_name("nameTag", "Device name", "", 30);
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "mqtt-explore";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("evse_service/EVSE45678/Receive");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
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
  // Control power pocket
  if(payload[1]=='n')
  {
    digitalWrite(Led_Pin,HIGH);
  }
  else if(payload[1]=='f')
  {
    digitalWrite(Led_Pin,LOW);
  }
  // Update periodValue with received value
  if(strcmp(topic,"ProjectIoT/1/period")==0)
  {
  // Convert payload to integer for period value
  String payloadStr = "";
  for (int i = 0; i < length; i++) {
    payloadStr += (char)payload[i];
  }
  period = payloadStr.toInt();
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(Led_Pin,OUTPUT);
  pinMode(Led_State, INPUT);
  wf.addParameter(&set_device_name);
  wf.setSaveParamsCallback(saveParamsCallback);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback_period);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  String state="";
  if(digitalRead(Led_State)==LOW)
  {
    state=state+"OFF";
  }
  else
  {
    state=state+"ON";
  }
  DynamicJsonDocument doc(200);
  doc[nameTag]["period"] = period;
  doc[nameTag]["state"] = state;
  Serial.print("Send : ");
  String data;
  serializeJson(doc, data);
  Serial.println(data);
  client.publish("ProjectIoT/1/sensor", data.c_str());
  int delayValue= period*1000;
  delay(delayValue);
}
void saveParamsCallback() {
  Serial.println("Get Params:");
  Serial.print(set_device_name.getID());
  Serial.print(" : ");
  Serial.println(set_device_name.getValue());
  nameTag = String(set_device_name.getValue());
}
