/*
  Smart Irrigation System for Urban Garden
*/

// Include the required libraries
#include "VOneMqttClient.h"
#include <ESP32Servo.h>
#include "DHT.h"

int MinMoistureValue = 4095;  // Minimum analog value from the sensor (dry soil)
int MaxMoistureValue = 1600;  // Maximum analog value from the sensor (wet soil)
int MinMoisture = 0;  // Minimum moisture percentage
int MaxMoisture = 100;  // Maximum moisture percentage
int Moisture = 0;  // Variable to store current moisture percentage

// Initialize water level sensor values for calibration and mapping
int MinDepthValue = 4095;  // Minimum analog value from the sensor (empty)
int MaxDepthValue = 1000;  // Maximum analog value from the sensor (full)
int MinDepth = 0;  // Minimum water depth percentage
int MaxDepth = 100;  // Maximum water depth percentage
int depth = 0;  // Variable to store current water depth percentage

// Threshold values for triggering actions
const int waterLevelThreshold = 80;  // Water level percentage threshold
const int soilMoistureThreshold = 25;  // Moisture percentage threshold

// Define device IDs for each sensor and actuator
const char* ServoMotor = "4c5c7d28-dd26-4994-912c-893b067a5a17";  // Servo motor ID
const char* MoistureSensor = "d7816de1-6143-4ff5-8fc9-5c1050a86bd9";  // Soil moisture sensor ID
const char* WaterLevel = "c6dd0858-8456-42fb-8a61-04d6c16d744e";  // Water level sensor ID
const char* RainSensor = "f76a3ccd-3a56-4347-bc70-106ab482d04a";  // Rain sensor ID
const char* Relay = "16387b20-2b43-4a64-9fee-33d7ad176d3f";  // Relay ID
const char* DHT11Sensor = "5ce82c07-39ab-4d91-93c9-d2984d0c4bb4";  // DHT11 sensor ID

// Define pins for sensors and actuators
const int servoPin = 16;  // Pin for servo motor
const int moisturePin = 34;  // Analog pin for soil moisture sensor
const int depthPin = 36;  // Analog pin for water level sensor
const int rainSensorPin = 35;  // Digital pin for rain sensor
const int relayPin = 32;  // Digital pin for relay (water pump control)
const int dht11Pin = 21;  // Digital pin for DHT11 temperature and humidity sensor

// DHT sensor configuration
#define DHTTYPE DHT11
DHT dht(dht11Pin, DHTTYPE);

// Actuator (servo motor) instance
Servo Myservo;

//Create an instance of VOneMqttClient
VOneMqttClient voneClient;

// Last message timestamp
unsigned long lastMsgTime = 0;

// Function to connect to Wi-Fi
void setup_wifi() 
{

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// Callback function for actuator commands from MQTT
void triggerActuator_callback(const char* actuatorDeviceId, const char* actuatorCommand)
{
  Serial.print("Main received callback : ");
  Serial.print(actuatorDeviceId);
  Serial.print(" : ");
  Serial.println(actuatorCommand);

  String errorMsg = "";

  // Parse actuator command JSON
  JSONVar commandObjct = JSON.parse(actuatorCommand);
  JSONVar keys = commandObjct.keys();

  // Handle servo motor commands
  if (String(actuatorDeviceId) == ServoMotor)
  {
    String key = "";
    JSONVar commandValue = "";
    for (int i = 0; i < keys.length(); i++) 
    {
      key = (const char* )keys[i];
      commandValue = commandObjct[keys[i]];
    }
    Serial.print("Key : ");
    Serial.println(key.c_str());
    Serial.print("value : ");
    Serial.println(commandValue);

    int angle = (int)commandValue;
    moveServoTo(angle, 10);
    voneClient.publishActuatorStatusEvent(actuatorDeviceId, actuatorCommand, errorMsg.c_str(), true);//publish actuator status
  }
  else
  {
    Serial.print(" No actuator found : ");
    Serial.println(actuatorDeviceId);
    errorMsg = "No actuator found";
    voneClient.publishActuatorStatusEvent(actuatorDeviceId, actuatorCommand, errorMsg.c_str(), false);//publish actuator status
  }
  // Handle relay (water pump) commands
  if (String(actuatorDeviceId) == Relay)
  {
    String key = "";
    bool commandValue = "";
    for (int i = 0; i < keys.length(); i++) {
      key = (const char* )keys[i];
      commandValue = (bool)commandObjct[keys[i]];
      Serial.print("Key : ");
      Serial.println(key.c_str());
      Serial.print("value : ");
      Serial.println(commandValue);
    }

    if (commandValue == true) {
      Serial.println("Relay ON");
      digitalWrite(relayPin, LOW);
    }
    else {
      Serial.println("Relay OFF");
      digitalWrite(relayPin, HIGH);
    }
    voneClient.publishActuatorStatusEvent(actuatorDeviceId, actuatorCommand, true);
  }
}

// Function to smoothly move servo motor to target angle
void moveServoTo(int targetAngle, int delayTime) 
{
  int currentAngle = Myservo.read();
  if (currentAngle < targetAngle) 
  {
    for (int angle = currentAngle; angle <= targetAngle; angle++) 
    {
      Myservo.write(angle);
      delay(delayTime);
    }
  } 
  else 
  {
    for (int angle = currentAngle; angle >= targetAngle; angle--) 
    {
      Myservo.write(angle);
      delay(delayTime);
    }
  }
}

// Setup function (runs once at startup)
void setup() 
{
  setup_wifi(); // Connect to Wi-Fi
  voneClient.setup(); // Initialize MQTT client
  voneClient.registerActuatorCallback(triggerActuator_callback);
  
  dht.begin(); // Initialize DHT sensor
  pinMode(rainSensorPin, INPUT); // Configure rain sensor pin
  pinMode(relayPin, OUTPUT); // Configure relay pin

  Myservo.attach(servoPin); // Attach servo motor to pin
  digitalWrite(relayPin, HIGH); // Turn off relay initially
  Myservo.write(0); // Set servo to initial position
}

// Main loop (runs repeatedly)
void loop() 
{
  if (!voneClient.connected()) 
  {
    voneClient.reconnect();
    String errorMsg = "Sensor Fail";
    voneClient.publishDeviceStatusEvent(MoistureSensor, true);
    voneClient.publishDeviceStatusEvent(WaterLevel, true);
    voneClient.publishDeviceStatusEvent(RainSensor, true);
    voneClient.publishDeviceStatusEvent(DHT11Sensor, true);
  }
  voneClient.loop();

  unsigned long cur = millis();
  if (cur - lastMsgTime > INTERVAL) 
  {
    lastMsgTime = cur;

    // Read temperature and humidity data
    float h = dht.readHumidity();
    int t = dht.readTemperature();

    // Publish DHT sensor telemetry data
    JSONVar payloadObject;
    payloadObject["Humidity"] = h;
    payloadObject["Temperature"] = t;
    voneClient.publishTelemetryData(DHT11Sensor, payloadObject);

    // Read and publish soil moisture data
    int sensorValue = analogRead(moisturePin);
    Moisture = map(sensorValue, MinMoistureValue, MaxMoistureValue, MinMoisture, MaxMoisture);
    voneClient.publishTelemetryData(MoistureSensor, "Soil moisture", Moisture);

    // Read and publish water level data
    int sensorValue2 = analogRead(depthPin);
    depth = map(sensorValue2, MinDepthValue, MaxDepthValue, MinDepth, MaxDepth);
    voneClient.publishTelemetryData(WaterLevel, "Depth", depth);

    // Read and publish rain sensor data
    int raining = !digitalRead(rainSensorPin);
    voneClient.publishTelemetryData(RainSensor, "Raining", raining);
    
    // Control irrigation based on sensor data
    if (!raining && Moisture < soilMoistureThreshold && depth > 10) 
    {
      digitalWrite(relayPin, LOW); // Activate pump
      delay(1500); // Run pump for 1.5 seconds
      digitalWrite(relayPin, HIGH); // Deactivate pump
    }
    
    // Control reservoir cover based on rain and water level    
    if (raining && depth < waterLevelThreshold) 
    {
      moveServoTo(90, 20); // Open reservoir cover
    } 
    else 
    {
      moveServoTo(0, 20); // Close reservoir cover
    }
  }  
}