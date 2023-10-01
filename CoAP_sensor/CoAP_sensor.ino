#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <coap-simple.h>
#include <ArduinoJson.h>

uint8_t period = 1;

void callback_response(CoapPacket &packet, IPAddress ip, int port);
void callback_periodSensor(CoapPacket &packet, IPAddress ip, int port);
WiFiUDP udp;
Coap coap(udp);

void callback_periodSensor(CoapPacket &packet, IPAddress ip, int port) {  
  uint8_t p;
  memcpy(&p, packet.payload, packet.payloadlen);
  period = p - '0';
  Serial.println(p);
  coap.sendResponse(ip, port, packet.messageid, "change period successfully");
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Coap Response got]");
  uint8_t p;
  memcpy(&p, packet.payload, packet.payloadlen);
  Serial.println(p);
}

void setup() {
  Serial.begin(9600);

  WiFiManager wm;
  bool res = wm.autoConnect("AutoConnectAP","password");
  if(!res) {
        Serial.println("Failed to connect");
    } 
    else {
        Serial.println("connected...yeey :)");
    }


  coap.server(callback_periodSensor, "device");
  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
}

void loop() {
  DynamicJsonDocument doc(100);
  
  uint8_t temp = random(0, 50);
  uint8_t humi = random(0, 100);
  doc["temprature"]=temp;
  doc["humidity"]=humi;
  doc["name"]="home_sensor";
  Serial.print("Send Request: ");
  serializeJson(doc, Serial);
  Serial.println();
  String data;
  serializeJson(doc, data);
  char a[data.length() + 1];
  data.toCharArray(a, data.length() + 1);
  Serial.println(data);
//  Serial.print(" ");
//  Serial.println(humi);
//  int temprature = coap.put(IPAddress(192, 168, 1, 5), 5683, "sensor", (char *) &temp, sizeof(temp));
//  int humidity = coap.put(IPAddress(192, 168, 1, 5), 5683, "sensor", (char *) &humi, sizeof(humi));
  int rq = coap.send(IPAddress(192, 168, 1, 8), 5683, "sensor", COAP_CON, COAP_PUT, NULL, 0,(uint8_t *) &a, sizeof(a));
  delay(period * 1000);
  coap.loop();
}
