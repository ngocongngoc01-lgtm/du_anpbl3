#include <WiFi.h>
#include <PubSubClient.h>
#include "HX711.h"
#include <Preferences.h>
#include <ESP32Servo.h> // THU VIEN MOI CHO SERVO

// WiFi
const char *ssid = "kk";
const char *password = "12345678";

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
const char *weight_topic = "MQTT_ESP32/WEIGHT"; 
const char *feed_topic = "MQTT_ESP32/FEED"; // TOPIC MOI CHO LENH CHO AN

const char *mqtt_username = "ngocongngoc";
const char *mqtt_password = "Ngocongngoc123";
const int mqtt_port = 1883;

// CAC CHAN CUA CAM BIEN LUC HX711
#define LOADCELL_DOUT_PIN 32
#define LOADCELL_SCK_PIN 33

// CHAN CUA SERVO (VD: Chan 27)
#define SERVO_PIN 5 
#define SERVO_OPEN_ANGLE 0   // Goc mo cua (VD: 90 do)
#define SERVO_CLOSE_ANGLE 90   // Goc dong (VD: 0 do)

// HE SO HIEU CHUAN MAC DINH
#define KNOWN_CALIBRATION_FACTOR 46922.12 

HX711 LOADCELL_HX711;
Preferences preferences;
Servo foodServo; // Doi tuong Servo
float weight_In_g = 0.00; // Bien luu tru trong luong

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0; 
unsigned long feedStartTime = 0;
const long feedDuration = 1000; // Thoi gian Servo mo (1 giay)
bool isFeeding = false;

// Ham mo Servo de cho an
void openFeeder() {
  foodServo.write(SERVO_OPEN_ANGLE);
  isFeeding = true;
  feedStartTime = millis();
  Serial.println("Feeder OPENED");
}

// Ham dong Servo sau khi cho an
void closeFeeder() {
  foodServo.write(SERVO_CLOSE_ANGLE);
  isFeeding = false;
  Serial.println("Feeder CLOSED");
}

void callback(char *topic, byte *payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();
  Serial.printf("Message arrived [%s]: %s\n", topic, message.c_str());

  // Dieu khien CHO AN
  if (String(topic) == feed_topic) {
    if (message.equalsIgnoreCase("FEED_NOW")) {
      if (!isFeeding) {
        openFeeder();
        client.publish(feed_topic, "FEEDING"); // Gui trang thai dang cho an
        Serial.println("Starting automatic feeding.");
      } else {
        Serial.println("Feeder is already active.");
      }
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("Attempting MQTT connection as %s...\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Connected to MQTT broker");
      // Dang ky topic moi cho lenh cho an
      client.subscribe(feed_topic); 
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 2 seconds...");
      delay(2000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // KHOI TAO SERVO
  foodServo.attach(SERVO_PIN);
  closeFeeder(); // Dam bao Servo o vi tri dong ban dau

  // KHOI TAO CAM BIEN LUC HX711 VA TAI HE SO HIEU CHUAN
  Serial.println("Scale Initializing...");
  LOADCELL_HX711.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  preferences.begin("CF", false);
  float loaded_factor = preferences.getFloat("CFVal", KNOWN_CALIBRATION_FACTOR);

  LOADCELL_HX711.set_scale(loaded_factor); 
  LOADCELL_HX711.tare(); // Thiet lap diem 0 (zero)

  Serial.printf("Scale ready. Factor: %.2f\n", loaded_factor);
  Serial.println("-------------------------");
  
  // Ket noi WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to the WiFi network");
  
  // Cai dat MQTT
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);

  reconnect();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Logic tu dong dong Servo sau mot thoi gian
  if (isFeeding && (millis() - feedStartTime >= feedDuration)) {
    closeFeeder();
    client.publish(feed_topic, "FEED_DONE"); // Gui trang thai cho an hoan tat
  }

  unsigned long now = millis();
  if (now - lastMsg > 10000) { // Cap nhat du lieu sau moi 10 giay
    lastMsg = now;

    // DOC CAM BIEN LUC VA GUI LEN MQTT
    if (LOADCELL_HX711.is_ready()) {
      float raw_weight_g = LOADCELL_HX711.get_units(10);

      // KIEM TRA VA SUA SO LIEU AM
      if (raw_weight_g < 0) {
        weight_In_g = 0.00; // Dat ve 0 neu la so am
      } else {
        weight_In_g = raw_weight_g;
      }
      
      // Gui gia tri len MQTT
      client.publish(weight_topic, String(weight_In_g, 2).c_str());

      // CAP NHAT SERIAL cho can
      Serial.printf("Published Weight: %.2f g\n", weight_In_g);
    } else {
      Serial.println("HX711 not found or not ready.");
    }
  }
}