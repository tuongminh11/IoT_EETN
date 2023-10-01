#include <WiFiManager.h>
#include <WiFiUdp.h>
#include <coap-simple.h>

#define PUMP 2

void callback_response(CoapPacket &packet, IPAddress ip, int port);
void callback_control(CoapPacket &packet, IPAddress ip, int port);

WiFiUDP udp;
Coap coap(udp);

bool PUMPSTATE;

void callback_control(CoapPacket &packet, IPAddress ip, int port) {  
  uint8_t p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  String message= String((char *)&p);
  Serial.println(message);
  if (message.equals("0"))
    PUMPSTATE = false; 
  else if(message.equals("1"))
    PUMPSTATE = true;
      
  if (PUMPSTATE) {
    digitalWrite(PUMP, HIGH) ; 
    coap.sendResponse(ip, port, packet.messageid, "1");
    Serial.println("ON");
  } else { 
    digitalWrite(PUMP, LOW) ; 
    coap.sendResponse(ip, port, packet.messageid, "0");
    Serial.println("OFF");
  }
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


  pinMode(PUMP, OUTPUT);
  digitalWrite(PUMP, HIGH);
  PUMPSTATE = true;

  coap.server(callback_control, "device");
  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
  String data = "{\"name\":\"pump\"}";
  char a[data.length() + 1];
  data.toCharArray(a, data.length() + 1);
  Serial.println(data);
  int rq = coap.send(IPAddress(192, 168, 1, 8), 5683, "sensor", COAP_CON, COAP_PUT, NULL, 0,(uint8_t *) &a, sizeof(a));
}

void loop() {
  coap.loop();
}
