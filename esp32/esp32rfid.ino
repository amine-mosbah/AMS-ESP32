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
#include <map>  // Add this line for std::map support

// WiFi Credentials
const char* ssid = "Hamaa";
const char* password = "hama12345";

// MQTT Broker settings
const char* mqtt_server = "0dcf768ae0b747cf8b6d18fda0062323.s1.eu.hivemq.cloud";
const int mqtt_port = 8883;
const char* mqtt_client_id = "ESP32_RFID_Reader";
const char* mqtt_username = "esp32rfid";
const char* mqtt_password = "Hama12345";

// MQTT Topics
const char* mqtt_topic_publish = "rfid/card";

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
#define BUZZER_PIN 4

// Known RFID cards mapping (same as in app.tsx)
struct CardOwner {
  const char* firstName;
  const char* role;
};

// Use a simple array of structs instead of std::map
const int KNOWN_CARDS_COUNT = 4;
struct KnownCard {
  const char* uid;
  CardOwner owner;
};

const KnownCard knownCards[KNOWN_CARDS_COUNT] = {
  {"BBBA3040", {"Mohamed Amine", "President"}},
  {"98923040", {"Ahmed Aziz", "Project Manager"}},
  {"0F9A631E", {"Sedki", "Senior Member"}},
  {"EF26401D", {"Rayen", "Senior Member"}}
};

String lastCardUID = "";
unsigned long lastScanTime = 0;
const unsigned long SCAN_COOLDOWN = 3000;

void setup() {
  Serial.begin(115200);

  pinMode(LED_SUCCESS, OUTPUT);
  pinMode(LED_ERROR, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_SUCCESS, LOW);
  digitalWrite(LED_ERROR, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(21, 22); // I2C (SDA, SCL)
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Initializing...");

  SPI.begin();
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(RST_PIN, LOW);
  delay(50);
  digitalWrite(RST_PIN, HIGH);
  delay(50);
  
  mfrc522.PCD_Init();
  MFRC522Debug::PCD_DumpVersionToSerial(mfrc522, Serial);
  
  espClient.setInsecure();

  connectToWiFi();
  mqttClient.setServer(mqtt_server, mqtt_port);

  displayReadyState();
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }

  // Buzzer feedback
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(100);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);

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

  // Check if card is known and display name
  bool cardKnown = false;
  CardOwner owner = {"Unknown", ""};
  
  for (int i = 0; i < KNOWN_CARDS_COUNT; i++) {
    if (cardUID.equals(knownCards[i].uid)) {
      owner = knownCards[i].owner;
      cardKnown = true;
      break;
    }
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(owner.firstName);
  lcd.setCursor(0, 1);
  lcd.print(owner.role);
  
  if (cardKnown) {
    digitalWrite(LED_SUCCESS, HIGH);
  } else {
    digitalWrite(LED_ERROR, HIGH);
  }

  publishCardUID(cardUID);
  delay(1000); // Show the name for 1 second
  digitalWrite(LED_SUCCESS, LOW);
  digitalWrite(LED_ERROR, LOW);
  mfrc522.PICC_HaltA();
  displayReadyState();
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
    
    if (mqttClient.connect(mqtt_client_id, mqtt_username, mqtt_password)) {
      Serial.println(" connected");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("MQTT Connected");
      delay(1000);
    } else {
      Serial.print(" failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(", retry...");
      
      lcd.setCursor(0, 1);
      lcd.print("Failed: ");
      lcd.print(mqttClient.state());
      
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

void publishCardUID(String cardUID) {
  StaticJsonDocument<256> doc;
  doc["cardUID"] = cardUID;
  doc["deviceId"] = mqtt_client_id;
  doc["timestamp"] = millis();

  String payload;
  serializeJson(doc, payload);

  Serial.print("Publishing: ");
  Serial.println(payload);

  if (mqttClient.connected()) {
    if (!mqttClient.publish(mqtt_topic_publish, payload.c_str())) {
      Serial.println("Publish failed");
    }
  } else {
    Serial.println("MQTT not connected");
  }
}

void displayReadyState() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("READY:");
  lcd.setCursor(0, 1);
  lcd.print("Scan RFID card");
}