# HydroPreserve

##System Tasks and Operations:

1. Sensor-Based Monitoring:<br>
○ Periodically trigger and read ultrasonic sensor (HC-SR04) data for water level measurement using STM32F407G <br>

2. Pump Control Logic:<br>
○ Automatic threshold comparison: Turn ON relay if water level is below threshold and turn OFF if water level is above threshold.<br>

3. STM32 Responsibilities:<br>
○ Send water level and pump status over UART to ESP8266 WiFi Module.<br>
○ Receive manual override commands from the mobile app (via UART through the ESP8266 WiFi module).<br>

4. ESP8266 Wifi Module Responsibilities:<br>
○ The ESP8266 acts as a communication link between the STM32F407G board and the Raspberry Pi(which helps connect to the mobile app).<br>
○ It acts as an MQTT Publisher who forwards the data it receives from the STM32407G Board to the Raspberry Pi.<br>

6. Communication with Raspberry Pi:<br>
○ Raspberry Pi acts as a MQTT broker.<br>
○ Raspberry Pi receives from the ESP8266 WiFi module so that it can be picked up by the mobile app.<br>

7. Using MQTT for Messaging:<br>
○ As explained above, we use a publish/subscribe model to facilitate the communication between the mobile app and the rest of the system.<br>
○ The ESP8266 Wifi Module acts as the publisher which the mobile app subscribes to while the Raspberry Pi acts like the MQTT broker to facilitate this connection.<br>

8. Mobile Application:<br>
○ Android app subscribes to MQTT topics to display water level and pump status.<br>
○ Provides manual override functionality to force the pump OFF, regardless of water level.<br>
