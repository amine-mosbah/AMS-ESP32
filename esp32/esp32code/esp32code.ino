/**
 * ESP32 + RFID-RC522 + LCD I2C Attendance System
 * Connects to WiFi and sends RFID card data to Spring Boot backend
 * Displays status on LCD via I2C
 */

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// RFID pins
#define RST_PIN         22
#define SS_PIN          21

// WiFi credentials
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// API endpoint
const char* serverUrl = "http://192.168.1.100:8080/api/check-in"; // Replace with your server IP

// Status LEDs
#define LED_SUCCESS     5  // Green LED
#define LED_ERROR       4  // Red LED

// LCD settings (typical I2C address is 0x27 or 0x3F, adjust if needed)
// Parameters: I2C address, number of columns, number of rows
LiquidCrystal_I2C lcd(0x27, 16, 2);

// MFRC522 instance
MFRC522 rfid(SS_PIN, RST_PIN);

// Last scanned card
String lastCardId = "";
unsigned long lastScanTime = 0;
const unsigned long SCAN_COOLDOWN = 3000; // 3 seconds cooldown between scans

void setup() {
  // Initialize serial
  Serial.begin(115200);
  delay(100);
  
  // Initialize LEDs
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
  
  // Initialize SPI
  SPI.begin();
  
  // Initialize RFID
  rfid.PCD_Init();
  rfid.PCD_DumpVersionToSerial();
  
  // Connect to WiFi
  connectToWiFi();
  
  // Show ready state
  displayReadyState();
}

void loop() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi disconnected");
    lcd.setCursor(0, 1);
    lcd.print("Reconnecting...");
    connectToWiFi();
    displayReadyState();
  }
  
  // Check for new cards
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    delay(50);
    return;
  }
  
  // Get card ID
  String cardId = getCardId();
  
  // Check if it's the same card within cooldown period
  unsigned long currentTime = millis();
  if (cardId == lastCardId && (currentTime - lastScanTime < SCAN_COOLDOWN)) {
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
  
  // Update last scanned card
  lastCardId = cardId;
  lastScanTime = currentTime;
  
  Serial.print("Card detected: ");
  Serial.println(cardId);
  
  // Display scanning state
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SCANNING:");
  lcd.setCursor(0, 1);
  lcd.print(cardId);
  
  // Send to server
  sendCardToServer(cardId);
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
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
    
    // Error indicator
    digitalWrite(LED_ERROR, HIGH);
    delay(2000);
    digitalWrite(LED_ERROR, LOW);
  }
}

String getCardId() {
  String cardId = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    cardId += (rfid.uid.uidByte[i] < 0x10 ? "0" : "") + String(rfid.uid.uidByte[i], HEX);
  }
  cardId.toUpperCase();
  
  // Halt PICC and stop encryption
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  
  return cardId;
}

void sendCardToServer(String cardId) {
  HTTPClient http;
  http.begin(serverUrl);
  http.addHeader("Content-Type", "application/json");
  
  // Prepare JSON payload
  StaticJsonDocument<200> doc;
  doc["rfidTag"] = cardId;
  
  String requestBody;
  serializeJson(doc, requestBody);
  
  // Send POST request
  int httpResponseCode = http.POST(requestBody);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.print("HTTP Response: ");
    Serial.println(httpResponseCode);
    Serial.println(response);
    
    // Parse the response
    StaticJsonDocument<512> responseDoc;
    DeserializationError error = deserializeJson(responseDoc, response);
    
    if (!error) {
      bool success = responseDoc["success"];
      String message = responseDoc["message"].as<String>();
      Serial.println(message);
      
      if (success) {
        // Successful check-in
        digitalWrite(LED_SUCCESS, HIGH);
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("ACCEPTED");
        lcd.setCursor(0, 1);
        lcd.print(message);
        
        delay(2000);
        digitalWrite(LED_SUCCESS, LOW);
      } else {
        // Failed check-in
        digitalWrite(LED_ERROR, HIGH);
        
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("REFUSED");
        lcd.setCursor(0, 1);
        lcd.print(message);
        
        delay(2000);
        digitalWrite(LED_ERROR, LOW);
      }
    } else {
      Serial.print("JSON parsing failed: ");
      Serial.println(error.c_str());
      indicateError("JSON Parse Error");
    }
  } else {
    Serial.print("Error on HTTP request: ");
    Serial.println(httpResponseCode);
    indicateError("Server Error");
  }
  
  http.end();
  
  // Return to ready state after displaying result
  delay(1000);
  displayReadyState();
}

void indicateError(String errorMsg) {
  digitalWrite(LED_ERROR, HIGH);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ERROR:");
  lcd.setCursor(0, 1);
  lcd.print(errorMsg);
  
  delay(2000);
  digitalWrite(LED_ERROR, LOW);
}

void displayReadyState() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("READY:");
  lcd.setCursor(0, 1);
  lcd.print("Scan RFID card");
}