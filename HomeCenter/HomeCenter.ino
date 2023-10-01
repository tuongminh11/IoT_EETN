#include <WiFi.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <ArduinoJson.h>
#include <String.h>
#include <PubSubClient.h>

const char *ssid = "504 -2.4G";
const char *password = "minhminh";
const char *mqtt_server = "192.168.1.4";

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port);

// CoAP server endpoint url callback
void callback_sensor(CoapPacket &packet, IPAddress ip, int port);

// UDP and CoAP class
// other initialize is "Coap coap(Udp, 512);"
// 2nd default parameter is COAP_BUF_MAX_SIZE(defaulit:128)
// For UDP fragmentation, it is good to set the maximum under
// 1280byte when using the internet connection.
WiFiUDP udp;
Coap coap(udp);
WiFiClient espClient;
PubSubClient client(espClient);
StaticJsonDocument<200> listDevice;
IPAddress ipDevice;

// CoAP server endpoint URL
void callback_sensor(CoapPacket &packet, IPAddress ip, int port)
{
  Serial.println("sensor data:");
  uint8_t p[packet.payloadlen + 1];
  memcpy(&p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  String a = String((char *)&p);
  Serial.println(a);
  DynamicJsonDocument data(100);
  deserializeJson(data, a);
  String ID = data["name"].as<String>();
  listDevice[ID]=ip;
  // serializeJson(doc, Serial);
  String topic = "prjIOT_eetn/" + ID;
  client.publish(topic.c_str(), (char *) &p);
  Serial.println();
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port)
{
  Serial.println("[Coap Response got]");
  uint8_t p[packet.payloadlen + 1];
  memcpy(&p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  String a = String((char *)&p);
  Serial.println(a);
}

//mqtt callback
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  Serial.print(length);
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  String topicStr(topic);
  if (topicStr.indexOf('/') >= 0) {
    // prjIOT_eetn/control/sensor1
    topicStr.remove(0, topicStr.lastIndexOf('/')+1);
    Serial.println(topicStr);
    JsonObject documentRoot = listDevice.as<JsonObject>();
    for (JsonPair keyValue : documentRoot) {
      if (topicStr.equals(keyValue.key().c_str())) {
        ipDevice.fromString(listDevice[keyValue.key()].as<String>());
        Serial.println(listDevice[keyValue.key()].as<String>());
        coap.send(ipDevice, 5683, "device", COAP_CON, COAP_PUT, NULL, 0, payload, length);
      }
    }

  }
}

void reconnect()
{
  // Loop until we're reconnected
  while (!client.connected())
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str()))
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");
      // ... and resubscribe
      client.subscribe("prjIOT_eetn/control/#");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup()
{
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  Serial.println("Setup Callback sensor");
  coap.server(callback_sensor, "sensor");

  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
}

void loop()
{
  if (!client.connected())
  {
    reconnect();
  }
  client.loop();
  coap.loop();
}
/*
if you change LED, req/res test with coap-client(libcoap), run following.
coap-client -m get coap://(arduino ip addr)/light
coap-client -e "1" -m put coap://(arduino ip addr)/light
coap-client -e "0" -m put coap://(arduino ip addr)/light
*/
