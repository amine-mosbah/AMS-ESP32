#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <MFRC522v2.h>
#include <MFRC522DriverSPI.h>
#include <MFRC522DriverPinSimple.h>
#include <MFRC522Debug.h>
#include <ArduinoJson.h>

// WiFi Credentials
const char* ssid = "ReImagineLab";
const char* password = "Relead@2021";

// MQTT Broker settings
const char* mqtt_server = "0dcf768ae0b747cf8b6d18fda0062323.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_client_id = "ESP32_RFID_Reader";
const char* mqtt_username = "esp32rfid";
const char* mqtt_password = "Hama12345";

// MQTT Topics
const char* mqtt_topic_publish = "rfid/card";
const char* mqtt_topic_response = "rfid/response";

// LCD settings (address, cols, rows)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// WiFi and MQTT clients
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// RFID SPI settings
const uint8_t RST_PIN = 2;  // RFID RST connected to GPIO2
MFRC522DriverPinSimple ss_pin(5); // RFID SS -> GPIO5
MFRC522DriverSPI driver{ss_pin};  // The SS pin is part of the driver
MFRC522 mfrc522{driver};  // The driver is the only parameter needed


// Status LEDs (optional)
#define LED_SUCCESS 17
#define LED_ERROR 16

String lastCardUID = "";
unsigned long lastScanTime = 0;
const unsigned long SCAN_COOLDOWN = 3000;

String lastMessageId = "";
bool waitingForResponse = false;
unsigned long responseTimeout = 0;
const unsigned long RESPONSE_TIMEOUT = 5000;

void setup() {
  Serial.begin(115200);

  pinMode(LED_SUCCESS, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);
  digitalWrite(LED_SUCCESS, LOW);
  digitalWrite(LED_ERROR, LOW);

  Wire.begin(21, 22); // I2C (SDA, SCL)
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

    SPI.begin(); // SPI must be started before RFID
  // Add a reset of the RFID module using the RST pin
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW);   // Reset the RFID module
  delay(50);
  digitalWrite(RST_PIN, HIGH);  // Exit reset mode
  delay(50);
  
  mfrc522.PCD_Init();  // Initialize the RFID reader
  MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial);
  
  // Set insecure connection for testing (remove in production)
  espClient.setInsecure();

  connectToWiFi();
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);

  displayReadyState();
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

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

  if (waitingForResponse) return;

  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }

  String cardUID = getCardUID();

  unsigned long currentTime = millis();
  if (cardUID == lastCardUID && (currentTime - lastScanTime < SCAN_COOLDOWN)) {
    Serial.println("Duplicate scan cooldown.");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Card already");
    lcd.setCursor(0, 1);
    lcd.print("scanned");
    delay(1000);
    displayReadyState();
    return;
  }

  lastCardUID = cardUID;
  lastScanTime = currentTime;

  Serial.print("Card UID: ");
  Serial.println(cardUID);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SCANNING:");
  lcd.setCursor(0, 1);
  lcd.print(cardUID);

  publishCardUID(cardUID);
  mfrc522.PICC_HaltA();
}

String getCardUID() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();
  return uid;
}

void connectToWiFi() {
  Serial.print("Connecting WiFi...");
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
    Serial.println(" Connected!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
    delay(2000);
  } else {
    Serial.println(" WiFi Failed");
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

  while (!mqttClient.connected() && attempts < 5) {
    Serial.print("Attempt MQTT...");
    
    // Add debug output for connection attempt
    Serial.print("Connecting to: ");
    Serial.print(mqtt_server);
    Serial.print(":");
    Serial.print(mqtt_port);
    Serial.print(" with ID: ");
    Serial.println(mqtt_client_id);
    
    if (mqttClient.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
      Serial.println(" connected");
      mqttClient.subscribe(mqtt_topic_response);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT Connected");
      delay(1000);
    } else {
      Serial.print(" failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(", retry...");
      
      // Show more detailed error information
      switch(mqttClient.state()) {
        case -4: 
          Serial.println("Connection timeout");
          lcd.setCursor(0, 1);
          lcd.print("Timeout");
          break;
        case -3: 
          Serial.println("Connection lost");
          lcd.setCursor(0, 1);
          lcd.print("Connection lost");
          break;
        case -2: 
          Serial.println("Connection failed");
          lcd.setCursor(0, 1);
          lcd.print("Conn failed");
          break;
        case -1: 
          Serial.println("Disconnected");
          lcd.setCursor(0, 1);
          lcd.print("Disconnected");
          break;
        case 1: 
          Serial.println("Bad protocol");
          lcd.setCursor(0, 1);
          lcd.print("Bad protocol");
          break;
        case 2: 
          Serial.println("Bad client ID");
          lcd.setCursor(0, 1);
          lcd.print("Bad client ID");
          break;
        case 3: 
          Serial.println("Server unavailable");
          lcd.setCursor(0, 1); 
          lcd.print("Server unavail");
          break;
        case 4: 
          Serial.println("Bad credentials");
          lcd.setCursor(0, 1);
          lcd.print("Bad credentials");
          break;
        case 5: 
          Serial.println("Not authorized");
          lcd.setCursor(0, 1);
          lcd.print("Not authorized");
          break;
        default:
          lcd.setCursor(0, 1);
          lcd.print("Failed: ");
          lcd.print(mqttClient.state());
      }
      
      delay(2000);
      attempts++;
    }
  }

  if (!mqttClient.connected() && attempts >= 5) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MQTT Failed");
    lcd.setCursor(0, 1);
    lcd.print("Will retry later");
    delay(2000);
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");

  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);

  if (String(topic) == mqtt_topic_response) {
    StaticJsonDocument<512> doc;
    DeserializationError error = deserializeJson(doc, message);

    if (!error) {
      String msgId = doc["messageId"].as<String>();

      if (msgId == lastMessageId) {
        waitingForResponse = false;

        bool success = doc["success"];
        String responseMessage = doc["message"].as<String>();

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
      Serial.println("Publish success");
      waitingForResponse = true;
      responseTimeout = millis() + RESPONSE_TIMEOUT;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Verifying...");
    } else {
      Serial.println("Publish failed");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("ERROR: Publish");
      delay(2000);
      displayReadyState();
    }
  } else {
    Serial.println("MQTT not connected");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MQTT Disconnect");
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