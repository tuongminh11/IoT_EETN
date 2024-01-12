# Project IoT

## Library 
* [WiFiManager.h](https://github.com/tzapu/WiFiManager) : Library for connect and config Wifi
* [coap-simple.h](https://github.com/hirotakaster/CoAP-simple-library) : CoAP protocol support
* WiFiUdp.h : available local
* [ArduinoJson.h](https://github.com/bblanchon/ArduinoJson) : JSON support
* [PubSubClient.h](https://github.com/knolleary/pubsubclient) : MQTT protocol client
* ESP32 
    * AsyncUDP.h : available local
    * [PicoMQTT.h](https://github.com/mlesniew/PicoMQTT) : MQTT broker
* ESP8266
    * [ESPAsyncUDP.h](https://github.com/me-no-dev/ESPAsyncUDP) : UDP protocol support
### 1. Home Center

* Logic UDP
    * Catch any broadcast UDP PDU
    * Get data in this PDU and save device infor into cache  
    Eg data : "garden_sensor/0"
        >  
        > "garden_sensor" : device's name
        >  
        > "0"             : device's protocol ( 0 for CoAP, 1 for MQTT)
        >
        JSON cache in Home center like this:  
        ```json
        {
            "garden_sensor":{
                "IP":"192.168.1.19",
                "P":0
            },
            "garden_pump":{
                "IP":"192.168.1.22",
                "P":0
            },
            "home_sensor":{
                "IP":"192.168.1.15",
                "P":1
            },
            "home_plug":{
                "IP":"192.168.1.20",
                "P":1
            },
        }
        ```
    * Send Unicast UDP PDU with key data to remoteIP to service Home Center IP.
* Logic CoAP
    * Service Endpoint "sensor" 
    * Callback "sensor" :
        * If Internet Available : publish to Platform.
        * Not available : Check data temprature and control all device have protocol CoAP to turn off in case HIGH temprature.
        * Respond ack to device.

* Logic Local MQTT
    * Subcribe all topic in local:
        * If Internet Available : publish to Platform.
        * Not available : Check data temprature and control all device have protocol MQTT to turn off in case HIGH temprature.  
        Topic publish is device's name get from cache ("home_sensor", "home_plug" ).
* Logic Online MQTT => Platform
    * Online Broker : 
        * host : "mqtt.thingsboard.cloud"
        * port : 1883
        * username : access token devicee service by Thingboards Platform
        * topic publish : "v1/devices/me/telemetry"
        * topic subcribe : "v1/devices/me/rpc/request/+"
    * Callback : 
        * Structure data :   
        **Int** : 0 or 1 to control  
        **Float** : to change period in sensor device
            ```json
            //data from Platform
            {"method":"$device's name", "params": $data type Int or Float}
            ```
        * Use data in field method to get device's name and protocol, after that send data to this device.

### 2. CoAP sensor

* Auto get Home Center IP
    * Send UDP broadcast with data "$name_device/$type_protocol"
    * Handle unicast and verify data. If key data is right, save IP Home Center to cache.

* Get data from Thingsboard to change period
* Loop
    * If connected to Home Center, send data contribute period, temprature and humidity. Else continue send UDP broadcast to find Home Center.
    Eg:
        ```json
        {
            "$name_device" : {
                "period": $number,
                "temprature": %number,
                "humidity": %number
            }
        }
        ```
    * check digital state GPIO 0 to reset config wifi
    * check digital state GPIO 2 if change (from HIGH to LOW) to set context:
        * Common temprature
        * High temprature (>60 C)
    
### 3. CoAP pump

* Auto get Home Center IP
    * Send UDP broadcast with data "$name_device/$type_protocol"
    * Handle unicast and verify data. If key data is right, save IP Home Center to cache.
* Get data from Thingsboard to control LED
* Loop
    * If not connected Home Center continue send UDP broadcast to find Home Center.
    * output digital state GPIO 2 to control LED.
    * check digital state GPIO 0 if change (from HIGH to LOW) to control LED. If long press 3s then reset config wifi.

### 4. MQTT sensor

* Auto get Home Center IP
    * Send UDP broadcast with data "$name_device/$type_protocol"
    * Handle unicast and verify data. If key data is right, save IP Home Center to cache.

* Get data from Thingsboard to change period
* Reconnect MQTT broker if disconnected
* Loop
    * If connected to Home Center, send data contribute period, temprature and humidity. Else continue send UDP broadcast to find Home Center.
    Eg:
        ```json
        {
            "$name_device" : {
                "period": $number,
                "temprature": %number,
                "humidity": %number
            }
        }
        ```
    * check digital state GPIO 0 to reset config wifi
    * check digital state GPIO 2 if change (from HIGH to LOW) to set context:
        * Common temprature
        * High temprature (>60 C)
    
### 5. MQTT socket

* Auto get Home Center IP
    * Send UDP broadcast with data "$name_device/$type_protocol"
    * Handle unicast and verify data. If key data is right, save IP Home Center to cache.

* Get data from Thingsboard to control LED
* Reconnect MQTT broker if disconnected
* Loop
    * If not connected Home Center continue send UDP broadcast to find Home Center.
    * output digital state GPIO 2 to control LED.
    * check digital state GPIO 0 if change (from HIGH to LOW) to control LED. If long press 3s then reset config wifi.