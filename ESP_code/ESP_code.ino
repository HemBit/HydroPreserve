#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// WiFi and MQTT Configuration
const char* wifi_network = "botspot";                // WiFi SSID
const char* wifi_pass = "myhotspot2407";             // WiFi password
const char* mqtt_serv_address = "192.168.238.229";   // Raspberry Pi MQTT broker IP
const int mqtt_port_number = 1883;

// MQTT Topics
const char* topic_water_level = "hydropreserve/tank_level";
const char* topic_pump_status = "hydropreserve/pump_status";
const char* topic_pump_control = "hydropreserve/control";  // Topic to receive control commands

// Variables for sensor data
double percentage = 0.0;
int relayState = 0;
int overrideState = 0;
String receivedString = "";
unsigned long lastDataTime = 0;

// WiFi and MQTT clients
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;

void setup() {
  // Initialize serial for debug output and STM32 communication
  Serial.begin(115200);
  
  // Give time for serial to initialize
  delay(2000);
  
  Serial.println("\n\n==== ESP8266 Water Tank Monitor ====");
  Serial.println("Initializing...");
  
  setup_wifi();
  client.setServer(mqtt_serv_address, mqtt_port_number);
  client.setCallback(callback);  // Set the callback for MQTT message reception
  
  Serial.println("Setup complete - waiting for data from STM32");
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.println("------------------------------");
  Serial.print("Attempting to connect to WiFi: ");
  Serial.println(wifi_network);
  
  WiFi.begin(wifi_network, wifi_pass);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    attempts++;
    Serial.print("Attempt #");
    Serial.print(attempts);
    Serial.print(" - Status: ");
    Serial.println(WiFi.status());  // Print numeric status code
    delay(500);
    Serial.print(".");
    delay(500);
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("");
    Serial.println("WiFi connected successfully!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("");
    Serial.println("Failed to connect to WiFi!");
    Serial.print("Status code: ");
    Serial.println(WiFi.status());
    Serial.println("Will continue trying in the background...");
  }
  Serial.println("------------------------------");
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to a null-terminated string
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  
  Serial.println("\n--------- MQTT MESSAGE RECEIVED ---------");
  Serial.print("Topic: [");
  Serial.print(topic);
  Serial.println("]");
  
  Serial.print("Payload: [");
  Serial.print(message);
  Serial.println("]");
  
  Serial.print("Length: ");
  Serial.println(length);
  
  Serial.print("Hex values: ");
  for (unsigned int i = 0; i < length; i++) {
    if (payload[i] < 16) Serial.print("0");
    Serial.print(payload[i], HEX);
    Serial.print(" ");
  }
  Serial.println("\n----------------------------------------");
  
  if (String(topic) == topic_pump_control) {
    // Forward command to STM32
    String command = "{\"control\":\"" + String(message) + "\"}\r\n";
    Serial.print(command);
    
    Serial.print("Sent control command to STM32: ");
    Serial.println(command);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    // Create client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      
      // Subscribe to control topic
      client.subscribe(topic_pump_control);
      Serial.print("Subscribed to: ");
      Serial.println(topic_pump_control);
      
      // Publish a connection message
      client.publish("hydropreserve/status", "ESP8266 connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void processSerialData(String data) {
  Serial.println("Processing data: " + data);
  
  // Check if the data looks like JSON
  if (data.indexOf('{') >= 0 && data.indexOf('}') >= 0) {
    
    // Extract percentage value
    int percentageStart = data.indexOf("\"percentage\":") + 13;
    int percentageEnd = data.indexOf(",", percentageStart);
    
    if (percentageStart > 13 && percentageEnd > percentageStart) {
      String percentageStr = data.substring(percentageStart, percentageEnd);
      double newPercentage = percentageStr.toFloat();
      
      Serial.print("Extracted percentage: ");
      Serial.println(newPercentage);
      
      // Update percentage (only if it changed significantly)
      if (abs(newPercentage - percentage) >= 0.5) {
        percentage = newPercentage;
        
        // Publish to MQTT if connected
        if (client.connected()) {
          String levelStr = String((int)round(percentage)); // Round to integer
          client.publish(topic_water_level, levelStr.c_str());
          Serial.print("Published water level: ");
          Serial.println(levelStr);
        }
      }
    }
    
    // Extract relay state
    int relayStart = data.indexOf("\"relay\":") + 8;
    int relayEnd = data.indexOf(",", relayStart);
    if (relayEnd < 0) { // Check if it's the last field
      relayEnd = data.indexOf("}", relayStart);
    }
    
    if (relayStart > 8 && relayEnd > relayStart) {
      String relayStr = data.substring(relayStart, relayEnd);
      int newRelayState = relayStr.toInt();
      
      Serial.print("Extracted relay state: ");
      Serial.println(newRelayState);
      
      // Update relay state (only if it changed)
      if (newRelayState != relayState) {
        relayState = newRelayState;
        
        // Publish to MQTT if connected
        if (client.connected()) {
          const char* pumpStatus = (relayState == 1) ? "ON" : "OFF";
          client.publish(topic_pump_status, pumpStatus);
          Serial.print("Published pump status: ");
          Serial.println(pumpStatus);
        }
      }
    }
    
    // Update last data time
    lastDataTime = millis();
  }
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnectAttempt = 0;
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 30000) { 
      lastReconnectAttempt = now;
      // Attempt to reconnect
      setup_wifi();
    }
  } 
  // Check MQTT connection (if WiFi is connected)
  else if (!client.connected()) {
    static unsigned long lastMQTTRetry = 0;
    unsigned long now = millis();
    
    if (now - lastMQTTRetry > 5000) { 
      lastMQTTRetry = now;
      reconnect();
    }
  }
  else {
    // Process MQTT messages
    client.loop();
  }
  
  // Process serial data from STM32
  while (Serial.available() > 0) {
    char c = Serial.read();
    
    // Add to buffer
    if (c == '\n' || c == '\r') {
      if (receivedString.length() > 0) {
        // Process complete line
        processSerialData(receivedString);
        receivedString = "";
      }
    } else {
      receivedString += c;
    }
  }
  
  // Watchdog check - if no data received for 60 seconds
  if (lastDataTime > 0 && millis() - lastDataTime > 60000) {
    Serial.println("WARNING: No data received for 60 seconds");
    
    // Publish warning if connected
    if (client.connected()) {
      client.publish("hydropreserve/status", "WARNING: No data from STM32");
    }
    
    lastDataTime = millis(); // Reset to avoid repeated warnings
  }
  
  // Small delay to avoid hogging CPU
  delay(10);
}