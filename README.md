# Project IoT

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
    * Send Unicast UDP PDU to remoteIP to service Home Center IP with key data.
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
