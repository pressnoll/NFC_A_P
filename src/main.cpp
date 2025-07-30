#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
// OLED libraries commented out
//#include <Adafruit_GFX.h>
//#include <Adafruit_SSD1306.h>

// I2C pins for both device
#define I2C_SDA 21
#define I2C_SCL 22

// Oled display settings commented out
/*
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
*/

// Create PN532 instance with I2C
#define PN532_IQR  (-1)
#define PN532_RESET (-1)
Adafruit_PN532 nfc(PN532_IQR, PN532_RESET);

// Wifi credentials
const char* ssid = "Galaxy S10e51a0";
const char* password = "123456789";

// Backend API URL
const char* serverUrl = "http://192.168.45.249:5000/api/attendance"; // will have to change later

// Define device ID
#define DEVICE_ID "esp32-main-entrance"

// Define Led pins
#define LED_SUCCESS 25 //GREEN LED
#define LED_ERROR 26 //RED LED

// Function declarations - modified to use Serial instead of display
void logMessage(String title, String message, bool isError);
void flashLED(int pin, int times);
void sendAttendanceData(String uid);

// Modified to log to Serial instead of display
void logMessage(String title, String message, bool isError) {
  Serial.println("\n----- " + title + " -----");
  Serial.println(message);
  Serial.println("----------------------");
  
  // Flash appropriate LED to provide visual feedback
  if (isError) {
    flashLED(LED_ERROR, 2);
  } else {
    flashLED(LED_SUCCESS, 1);
  }
}

// Function to flash LEDs
void flashLED(int pin, int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(200);
    digitalWrite(pin, LOW);
    delay(200);
  }
}

void setup(){
    // begin serial communications
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("NFC Attendance System");

    // Initialize I2C
    Wire.begin(I2C_SDA, I2C_SCL);

    // Setup status LEDs
    pinMode(LED_SUCCESS, OUTPUT);
    pinMode(LED_ERROR, OUTPUT);

    // Power indicator - blink both LEDs at startup
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_SUCCESS, HIGH);
        digitalWrite(LED_ERROR, HIGH);
        delay(200);
        digitalWrite(LED_SUCCESS, LOW);
        digitalWrite(LED_ERROR, LOW);
        delay(200);
    }

    // OLED display initialization commented out
    /*
    if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        // if display fails continue but show error on LEDs
        digitalWrite(LED_ERROR, HIGH);
        delay(100);
        digitalWrite(LED_ERROR, LOW);
    }

    // Display setup - updated to use the enhanced displayMessage function
    displayMessage("STARTUP", "Attendance System\nInitializing...", false);
    delay(1000);
    */
    
    // Log to Serial instead
    logMessage("STARTUP", "Attendance System\nInitializing...", false);
    delay(1000);

    // Initialize NFC reader with I2C
    nfc.begin();

    uint32_t versiondata = nfc.getFirmwareVersion();
    if (!versiondata) {
        Serial.println("Couldn't find PN532 board");
        logMessage("ERROR", "PN532 not found!", true);
        digitalWrite(LED_ERROR, HIGH);
        while (1) delay(10);
    }

    // Configure to read RFID tags
    Serial.println("PN532 NFC Reader detected");
    Serial.print("Firmware ver. ");
    Serial.print((versiondata>>16) & 0xFF, DEC);
    Serial.print('.');
    Serial.println((versiondata>>8) & 0xFF, DEC);

    String firmwareVersion = "Firmware: " + String((versiondata>>16) & 0xFF) + "." + String((versiondata>>8) & 0xFF);
    logMessage("NFC READER", "PN532 detected\n" + firmwareVersion, false);
    
    nfc.SAMConfig();
    delay(1000);

    // connect to wifi
    logMessage("WIFI", "Connecting to:\n" + String(ssid), false);

    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");

    int dots = 0;
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        dots++;
        attempts++;

        if (dots >= 20) {
            dots = 0;
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi connection failed");
        logMessage("ERROR", "WiFi connection\nfailed!\n\nCheck settings", true);
        digitalWrite(LED_ERROR, HIGH);
        delay(5000);
    } else {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());

        // If alright flash led and show ready message
        flashLED(LED_SUCCESS, 3);

        String ipAddress = "IP: " + WiFi.localIP().toString();
        logMessage("CONNECTED", ipAddress + "\nSystem Ready", false);
        delay(2000);
    }

    logMessage("READY", "Scan your card", false);
}

void loop() {
    // check if wifi is still connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected. Reconnecting...");
        logMessage("ERROR", "WiFi disconnected\nReconnecting...", true);
        WiFi.begin(ssid, password);
        delay(5000);  
        logMessage("READY", "Scan your card", false);
        return;
    }

    // show ready message if not already showing
    static unsigned long lastStatusUpdate = 0;
    if(millis() - lastStatusUpdate > 10000) { // Update every 10 seconds
        logMessage("READY", "Scan your card", false);
        lastStatusUpdate = millis();
    }

    // Look for NFC cards
    uint8_t success;
    uint8_t uid[7]; 
    uint8_t uidLength;
    
    // Wait for a card to be available
    success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);

    if (success) {
        // Card found! Flash success LED briefly
        digitalWrite(LED_SUCCESS, HIGH);
        delay(100);
        digitalWrite(LED_SUCCESS, LOW);
        
        // Convert UID to string format
        String cardUID = "";
        for (uint8_t i = 0; i < uidLength; i++) {
            if (uid[i] < 0x10) {
                cardUID += "0";
            }
            cardUID += String(uid[i], HEX);
            if (i < uidLength - 1) {
                cardUID += ":";
            }
        }
        
        cardUID.toUpperCase();
        Serial.print("Card detected! UID: ");
        Serial.println(cardUID);
        
        // Show card detected message
        logMessage("CARD", "Card detected!\nUID: " + cardUID + "\nProcessing...", false);
        
        // Send the UID to the backend
        sendAttendanceData(cardUID);
        
        // Wait before reading again (to avoid multiple reads of same card)
        delay(3000);
        lastStatusUpdate = 0; // Force status update after card read
    }
    
    delay(250); // Small delay to avoid hammering the PN532
}

void sendAttendanceData(String uid) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverUrl);
        http.addHeader("Content-Type", "application/json");
        
        // Create JSON payload
        StaticJsonDocument<200> doc;
        doc["uid"] = uid;
        doc["device_id"] = DEVICE_ID;
        
        String jsonPayload;
        serializeJson(doc, jsonPayload);
        
        // Show sending message
        logMessage("SENDING", "Connecting to server...", false);
        
        // Send POST request
        int httpResponseCode = http.POST(jsonPayload);
        String response = "";
        
        if (httpResponseCode > 0) {
            response = http.getString();
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
            Serial.println(response);
            
            // Parse response to extract user name if possible
            String userName = "User";
            if(httpResponseCode == 201) {
                DynamicJsonDocument respDoc(1024);
                deserializeJson(respDoc, response);
                if(respDoc.containsKey("user")) {
                    userName = respDoc["user"].as<String>();
                }
            }
            
            // Check if the request was successful
            if (httpResponseCode == 201) {  // Created status
                flashLED(LED_SUCCESS, 1);
                logMessage("SUCCESS", "Welcome, " + userName + "\nAttendance recorded", false);
            } else {
                flashLED(LED_ERROR, 1);
                String errorMsg = "Error: ";
                if(response.length() > 0) {
                    DynamicJsonDocument respDoc(1024);
                    DeserializationError error = deserializeJson(respDoc, response);
                    if (!error && respDoc.containsKey("error")) {
                        errorMsg += respDoc["error"].as<String>();
                    } else {
                        errorMsg += "Code " + String(httpResponseCode);
                    }
                } else {
                    errorMsg += "Code " + String(httpResponseCode);
                }
                logMessage("FAILED", errorMsg, true);
            }
        } else {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
            flashLED(LED_ERROR, 2);
            logMessage("ERROR", "Server connection\nfailed", true);
        }
        
        http.end();
    } else {
        Serial.println("WiFi not connected");
        flashLED(LED_ERROR, 3);
        logMessage("ERROR", "WiFi disconnected", true);
    }
}