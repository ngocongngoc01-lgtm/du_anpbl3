#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>  

// ==================== CÀI ĐẶT LCD ====================
LiquidCrystal_I2C lcd(0x27, 16, 2);  // Địa chỉ I2C: 0x27 hoặc 0x3F
// Nếu không hiển thị, thử đổi thành: LiquidCrystal_I2C lcd(0x3F, 16, 2);

// WiFi
const char *ssid = "kk";
const char *password = "12345678";

// MQTT Broker
const char *mqtt_broker = "broker.emqx.io";
const char *temp_topic = "MQTT_ESP32/TEMPERATURE";
const char *hum_topic = "MQTT_ESP32/HUMIDITY";
const char *led_topic = "MQTT_ESP32/LED1";
const char *fan_topic = "MQTT_ESP32/FAN";
const char *pump_topic = "MQTT_ESP32/PUMP";
const char *water_level_topic = "MQTT_ESP32/WATER_LEVEL";
const char *alert_topic = "MQTT_ESP32/ALERT";
const char *intruder_topic = "MQTT_ESP32/INTRUDER";

const char *mqtt_username = "ngocongngoc";
const char *mqtt_password = "Ngocongngoc123";
const int mqtt_port = 1883;

// DHT Sensor
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
// LED
#define LED_PIN 5
bool ledState = LOW;

// Relay for Fan (active HIGH)
#define RELAY_PIN 15
bool fanState = LOW;
// Relay for Pump (active HIGH)
#define PUMP_PIN 13
bool pumpState = LOW;
// Water Level Sensor
#define POWER_PIN 18
#define SIGNAL_PIN 35
int waterLevelValue = 0;
// Ultrasonic Sensor HC-SR04
#define TRIG_PIN 25
#define ECHO_PIN 26

// Buzzer
#define BUZZER_PIN 23
bool alertMode = false;

// Biến lưu giá trị hiển thị trên LCD
float displayTemp = 0.0;
float displayHum = 0.0;
int displayWaterPercent = 0;
long displayDistance = 0;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;

void setup() {
  Serial.begin(115200);

  // === KHỞI TẠO LCD ===
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Farm Init...");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi...");
  delay(2000);
  lcd.clear();

  // Khởi tạo LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  // Khởi tạo Relay Quạt
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  fanState = LOW;
  // Khởi tạo Relay Bơm
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pumpState = LOW;
  // Khởi tạo Cảm biến mực nước
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  analogSetAttenuation(ADC_11db);
  // Khởi tạo Cảm biến siêu âm
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  // Khởi tạo Còi
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Khởi tạo cảm biến DHT
  dht.begin();

  // Kết nối WiFi
  Serial.println("Connecting to WiFi...");
  lcd.setCursor(0, 1);
  lcd.print("WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    lcd.print(".");
  }
  Serial.println("\nConnected to the WiFi network");
  lcd.clear();
  lcd.print("WiFi Connected!");
  delay(1000);
  lcd.clear();
  
  // Cài đặt MQTT
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(callback);

  reconnect();
}

void callback(char *topic, byte *payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();
  Serial.printf("Message arrived [%s]: %s\n", topic, message.c_str());

  // Điều khiển LED
  if (String(topic) == led_topic) {
    if (message.equalsIgnoreCase("ON")) {
      ledState = HIGH;
      digitalWrite(LED_PIN, ledState);
      Serial.println("LED -> ON");
    } else if (message.equalsIgnoreCase("OFF")) {
      ledState = LOW;
      digitalWrite(LED_PIN, ledState);
      Serial.println("LED -> OFF");
    }
  }

  // Điều khiển QUẠT
  if (String(topic) == fan_topic) {
    if (message.equalsIgnoreCase("ON")) {
      fanState = HIGH;
      digitalWrite(RELAY_PIN, HIGH);
      Serial.println("FAN -> ON");
    } else if (message.equalsIgnoreCase("OFF")) {
      fanState = LOW;
      digitalWrite(RELAY_PIN, LOW);
      Serial.println("FAN -> OFF");
    }
  }

  // Điều khiển BƠM
  if (String(topic) == pump_topic) {
    if (message.equalsIgnoreCase("ON")) {
      pumpState = HIGH;
      digitalWrite(PUMP_PIN, HIGH);
      Serial.println("PUMP -> ON (MQTT)");
    } else if (message.equalsIgnoreCase("OFF")) {
      pumpState = LOW;
      digitalWrite(PUMP_PIN, LOW);
      Serial.println("PUMP -> OFF (MQTT)");
    }
  }

  // Điều khiển chế độ CẢNH BÁO
  if (String(topic) == alert_topic) {
    if (message.equalsIgnoreCase("ON")) {
      alertMode = true;
      Serial.println("ALERT MODE -> ON");
    } else if (message.equalsIgnoreCase("OFF")) {
      alertMode = false;
      digitalWrite(BUZZER_PIN, LOW);
      Serial.println("ALERT MODE -> OFF");
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
      // Đăng ký các chủ đề
      client.subscribe(led_topic);
      client.subscribe(fan_topic);
      client.subscribe(pump_topic);
      client.subscribe(alert_topic);
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 2 seconds...");
      delay(2000);
    }
  }
}

// Hàm đo khoảng cách bằng HC-SR04
long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = duration * 0.034 / 2;
  return distance;
}

// Hàm cập nhật hiển thị LCD
void updateLCD() {
  lcd.clear();
  // Dòng 1: Nhiệt độ và độ ẩm
  lcd.setCursor(0, 0);
  lcd.printf("T:%.1fC H:%.0f%%", displayTemp, displayHum);
  // Dòng 2: Mực nước và khoảng cách
  lcd.setCursor(0, 1);
  lcd.printf("W:%3d%% D:%2ldcm", displayWaterPercent, displayDistance);
  // Nhấp nháy dấu "!" khi có cảnh báo
  if (alertMode && (millis() / 500) % 2 == 0) {
    lcd.setCursor(15, 1);
    lcd.print("!");
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 10000) { // Cập nhật dữ liệu sau mỗi 10 giây
    lastMsg = now;
    // Đọc cảm biến DHT
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();
    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT sensor!");
    } else {
      displayTemp = temperature;
      displayHum = humidity;
      // Tự động bật quạt nếu nhiệt độ > 40°C
      if (temperature > 40.0) {
        fanState = HIGH;
        digitalWrite(RELAY_PIN, HIGH);
      }
      client.publish(temp_topic, String(temperature).c_str());
      client.publish(hum_topic, String(humidity).c_str());
    }

    // Đọc cảm biến mực nước
    digitalWrite(POWER_PIN, HIGH);
    delay(10);
    waterLevelValue = analogRead(SIGNAL_PIN);
    digitalWrite(POWER_PIN, LOW);

    // Tính phần trăm mực nước
    if (waterLevelValue <= 600) displayWaterPercent = 0;
    else if (waterLevelValue >= 1600) displayWaterPercent = 100;
    else displayWaterPercent = map(waterLevelValue, 600, 1600, 0, 100);

    // Tự động điều khiển máy bơm dựa trên mực nước
    if (waterLevelValue > 1600 ) {
      pumpState = LOW;
      digitalWrite(PUMP_PIN, LOW);
      Serial.println("PUMP -> OFF (Water Level High)");
    } 
    client.publish(water_level_topic, String(waterLevelValue).c_str());

    // Đọc cảm biến siêu âm và điều khiển còi
    displayDistance = measureDistance();
    if (alertMode && displayDistance < 10) { 
      digitalWrite(BUZZER_PIN, HIGH);
      Serial.println("ALERT: Object detected!");
      client.publish(intruder_topic, "DANGER: Intruder Detected!");
    } else {
      // Chỉ tắt còi nếu alertMode đang bật và không phát hiện vật thể
      if (alertMode) {
        digitalWrite(BUZZER_PIN, LOW);
      }
    }

    // Gửi trạng thái các thiết bị
    client.publish(led_topic, ledState ? "ON" : "OFF");
    client.publish(fan_topic, fanState ? "ON" : "OFF");
    client.publish(pump_topic, pumpState ? "ON" : "OFF");
    client.publish(alert_topic, alertMode ? "ON" : "OFF");

    // Cập nhật LCD
    updateLCD();

    // In ra Serial Monitor
    Serial.printf("Published Temp: %.2f °C, Hum: %.2f %%, Water Level: %d (%d%%), Distance: %ld cm, LED: %s, Fan: %s, Pump: %s, Alert: %s\n",
                  displayTemp, displayHum, waterLevelValue, displayWaterPercent, displayDistance, 
                  ledState ? "ON" : "OFF", fanState ? "ON" : "OFF", pumpState ? "ON" : "OFF", alertMode ? "ON" : "OFF");
  }
}