/*
  ESP32 RFID UID Reader with MQTT Communication
  Uses MFRC522v2 library and LCD display with I2C
*/

#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>
#include <WiFi.h>
#include <PubSubClient.h> // MQTT client library
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// WiFi credentials
const char* ssid = "ReImagineLab";
const char* password = "Relead@2021";

// MQTT Broker settings
const char* mqtt_server = "your-mqtt-broker.com"; // MQTT broker address
const int mqtt_port = 8883;                       // Default MQTT port
const char* mqtt_client_id = "ESP32_RFID_Reader"; // Client ID for MQTT connection
const char* mqtt_username = "mqtt_user";          // Optional: MQTT username
const char* mqtt_password = "mqtt_password";      // Optional: MQTT password

// MQTT Topics
const char* mqtt_topic_publish = "rfid/card";     // Topic to publish card UIDs
const char* mqtt_topic_response = "rfid/response";// Topic to receive responses

// LCD settings (I2C address, columns, rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// WiFi and MQTT clients
WiFiClient espClient;
PubSubClient mqttClient(espClient);

// RFID settings for MFRC522v2 library
MFRC522DriverPinSimple ss_pin(5); // SS pin connected to GPIO 5
MFRC522DriverSPI driver{ss_pin};  // Create SPI driver
MFRC522 mfrc522{driver};          // Create MFRC522 instance

// Status LED pins (optional)
#define LED_SUCCESS 17
#define LED_ERROR 16

// Last scanned card UID and timing
String lastCardUID = "";
unsigned long lastScanTime = 0;
const unsigned long SCAN_COOLDOWN = 3000; // 3 seconds cooldown between scans

// Response tracking
String lastMessageId = "";
bool waitingForResponse = false;
unsigned long responseTimeout = 0;
const unsigned long RESPONSE_TIMEOUT = 5000; // 5 seconds timeout for response

void setup() {
  Serial.begin(115200);

  // Initialize status LEDs
  pinMode(LED_SUCCESS, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);
  digitalWrite(LED_SUCCESS, LOW);
  digitalWrite(LED_ERROR, LOW);

  // Initialize LCD
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  // Initialize RFID reader
  mfrc522.PCD_Init();
  Serial.println(F("RFID Reader initialized"));
  MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial);

  // Connect to WiFi
  connectToWiFi();

  // Setup MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  // Display ready message
  displayReadyState();
}

void loop() {
  // Maintain MQTT connection
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  // Check for response timeout
  if (waitingForResponse && millis() > responseTimeout) {
    waitingForResponse = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Timeout");
    lcd.setCursor(0, 1);
    lcd.print("No response");
    delay(2000);
    displayReadyState();
  }

  // If waiting for a response, don't check for new cards
  if (waitingForResponse) {
    return;
  }

  // Check for new cards
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }

  // Extract UID
  String cardUID = getCardUID();

  // Check for duplicate scans within cooldown period
  unsigned long currentTime = millis();
  if (cardUID == lastCardUID && (currentTime - lastScanTime < SCAN_COOLDOWN)) {
    Serial.println("Card already scanned. Waiting for cooldown.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Card already");
    lcd.setCursor(0, 1);
    lcd.print("scanned");
    delay(1000);
    displayReadyState();
    return;
  }

  // Update last scanned info
  lastCardUID = cardUID;
  lastScanTime = currentTime;

  // Print UID to serial
  Serial.print("Card UID: ");
  Serial.println(cardUID);

  // Display scanning state
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SCANNING:");
  lcd.setCursor(0, 1);
  lcd.print(cardUID);

  // Send UID to MQTT broker
  publishCardUID(cardUID);

  // Reset RFID subsystem for next read
  mfrc522.PICC_HaltA();
}

String getCardUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    uid += (mfrc522.uid.uidByte[i] < 0x10 ? "0" : "") + String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");

  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    lcd.setCursor(attempts % 16, 1);
    lcd.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("Connected to WiFi, IP: ");
    Serial.println(WiFi.localIP());

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(2000);
  } else {
    Serial.println();
    Serial.println("Failed to connect to WiFi");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Failed");
    digitalWrite(LED_ERROR, HIGH);
    delay(2000);
    digitalWrite(LED_ERROR, LOW);
  }
}

void reconnectMQTT() {
  int attempts = 0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting MQTT");

  // Try to connect to MQTT broker
  while (!mqttClient.connected() && attempts < 5) {
    Serial.print("Attempting MQTT connection...");

    if (mqttClient.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
      Serial.println("connected");

      // Subscribe to response topic
      mqttClient.subscribe(mqtt_topic_response);

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT Connected");
      delay(1000);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 2 seconds");

      lcd.setCursor(0, 1);
      lcd.print("Failed: ");
      lcd.print(mqttClient.state());

      delay(2000);
      attempts++;
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == mqtt_topic_response) {
    StaticJsonDocument<512> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, message);

    if (!error) {
      String msgId = responseDoc["messageId"].as<String>();

      if (msgId == lastMessageId) {
        waitingForResponse = false;

        bool success = responseDoc["success"];
        String responseMessage = responseDoc["message"].as<String>();

        if (success) {
          digitalWrite(LED_SUCCESS, HIGH);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("ACCEPTED");
          lcd.setCursor(0, 1);
          lcd.print(responseMessage);
          delay(2000);
          digitalWrite(LED_SUCCESS, LOW);
        } else {
          digitalWrite(LED_ERROR, HIGH);
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("REFUSED");
          lcd.setCursor(0, 1);
          lcd.print(responseMessage);
          delay(2000);
          digitalWrite(LED_ERROR, LOW);
        }

        displayReadyState();
      }
    }
  }
}

void publishCardUID(String cardUID) {
  lastMessageId = String(millis());

  StaticJsonDocument<256> doc;
  doc["cardUID"] = cardUID;
  doc["deviceId"] = mqtt_client_id;
  doc["messageId"] = lastMessageId;
  doc["timestamp"] = millis();

  String payload;
  serializeJson(doc, payload);

  Serial.print("Publishing: ");
  Serial.println(payload);

  if (mqttClient.connected()) {
    if (mqttClient.publish(mqtt_topic_publish, payload.c_str())) {
      Serial.println("Publish successful");

      waitingForResponse = true;
      responseTimeout = millis() + RESPONSE_TIMEOUT;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Verifying...");
    } else {
      Serial.println("Publish failed");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ERROR:");
      lcd.setCursor(0, 1);
      lcd.print("Publish Failed");
      delay(2000);
      displayReadyState();
    }
  } else {
    Serial.println("MQTT not connected");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("ERROR:");
    lcd.setCursor(0, 1);
    lcd.print("MQTT Disconnected");
    delay(2000);
    displayReadyState();
  }
}

void displayReadyState() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("READY:");
  lcd.setCursor(0, 1);
  lcd.print("Scan RFID card");
}
