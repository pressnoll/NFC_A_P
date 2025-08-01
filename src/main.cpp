#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_PN532.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// I2C pins for both device
#define I2C_SDA 21
#define I2C_SCL 22

// Create PN532 instance with I2C
#define PN532_IQR  (-1)
#define PN532_RESET (-1)
Adafruit_PN532 nfc(PN532_IQR, PN532_RESET);

// Backend API URL
const char* serverUrl = "http://192.168.45.249:5000/api/attendance"; 

// Define device ID
#define DEVICE_ID "esp32-main-entrance"

// Define Led pins
#define LED_SUCCESS 25 //GREEN LED
#define LED_ERROR 26 //RED LED

// Function declarations
void logMessage(String title, String message, bool isError);
void flashLED(int pin, int times);
void sendAttendanceData(String uid);
void setupWiFiManager();

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
    delay(1000);
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
    
    logMessage("STARTUP", "Attendance System\nInitializing...", false);

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

    // Setup WiFi using our enhanced function
    setupWiFiManager();

    logMessage("READY", "Scan your card", false);
}

void setupWiFiManager() {
    logMessage("WIFI", "Starting WiFi setup", false);
    
    // Keep LED on during WiFi setup for clear visual feedback
    digitalWrite(LED_SUCCESS, HIGH);
    
    // Create WiFiManager instance
    WiFiManager wifiManager;
    
    // Uncomment to reset settings and force portal mode (for testing)
    // wifiManager.resetSettings();
    
    // Use custom parameters if needed (like custom server URL)
    // WiFiManagerParameter custom_server("server", "Server URL", serverUrl, 40);
    // wifiManager.addParameter(&custom_server);
    
    // IMPORTANT: Enhanced captive portal settings
    wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    
    // Set portal timeout (3 minutes)
    wifiManager.setConfigPortalTimeout(180);

    // Enhanced portal name for clarity
    const char* apName = "NFC-AttendanceSetup";
    const char* apPassword = ""; // No password for easier access
    
    Serial.println("\n╔═════════════════════════════════════════╗");
    Serial.println("║              WIFI SETUP                 ║");
    Serial.println("╠═════════════════════════════════════════╣");
    Serial.println("║ 1. Connect to WiFi: NFC-AttendanceSetup ║");
    Serial.println("║ 2. Wait for portal OR                  ║");
    Serial.println("║    Navigate to: http://10.0.1.1        ║");
    Serial.println("╚═════════════════════════════════════════╝\n");
    
    logMessage("WIFI SETUP", "Connect to WiFi:\nNFC-AttendanceSetup\nThen follow instructions", false);
    
    // Flash the LED in a distinct pattern while in setup mode
    for (int i = 0; i < 5; i++) {
        digitalWrite(LED_SUCCESS, HIGH);
        delay(100);
        digitalWrite(LED_SUCCESS, LOW);
        delay(100);
    }
    digitalWrite(LED_SUCCESS, HIGH); // Keep on during setup
    
    // Start the config portal in blocking mode
    // This uses the enhanced captive portal detection
    bool result = wifiManager.startConfigPortal(apName, apPassword);
    
    // Turn off the LED after setup
    digitalWrite(LED_SUCCESS, LOW);
    
    if (!result) {
        Serial.println("Failed to connect or timeout");
        logMessage("ERROR", "WiFi setup failed\nRestarting...", true);
        delay(3000);
        ESP.restart();
    }
    
    // If we get here, we're connected to WiFi
    Serial.println("WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    
    // Success indication
    flashLED(LED_SUCCESS, 3);
    String ipAddress = "IP: " + WiFi.localIP().toString();
    logMessage("CONNECTED", ipAddress + "\nSystem Ready", false);
    delay(1000);
}

void loop() {
    // check if wifi is still connected
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected. Reconnecting...");
        logMessage("ERROR", "WiFi disconnected\nReconnecting...", true);
        WiFi.begin(); // Reconnect using stored credentials
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
            String ipAddress = "IP: " + WiFi.localIP().toString();
            logMessage("RECONNECTED", ipAddress + "\nSystem Ready", false);
        } else {
            logMessage("ERROR", "WiFi failed\nCheck connection", true);
            // If reconnection fails after multiple attempts, restart WiFi setup
            setupWiFiManager();
        }
        
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
        
        // Match the case used in your Firebase database (lowercase)
        cardUID.toLowerCase();
        
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
        doc["uid"] = uid; // This matches the field name expected by your Flask backend
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
            
            // Parse response
            DynamicJsonDocument respDoc(1024);
            DeserializationError error = deserializeJson(respDoc, response);
            
            // Different handling based on response code
            if (httpResponseCode == 201) {  // Created status - successful attendance
                String userName = "User";
                if (!error && respDoc.containsKey("user")) {
                    userName = respDoc["user"].as<String>();
                }
                flashLED(LED_SUCCESS, 1);
                logMessage("SUCCESS", "Welcome, " + userName + "\nAttendance recorded", false);
            } 
            else if (httpResponseCode == 404) {  // Not Found - card not registered
                flashLED(LED_ERROR, 2);
                logMessage("NOT REGISTERED", "Card not registered\nPlease register first", true);
            }
            else if (httpResponseCode == 400 && !error && respDoc.containsKey("error")) {
                // Already recorded attendance today
                String errorMsg = respDoc["error"].as<String>();
                flashLED(LED_ERROR, 1);
                logMessage("ALREADY RECORDED", errorMsg, true);
            }
            else {
                // Other errors
                flashLED(LED_ERROR, 1);
                String errorMsg = "Error: ";
                if(!error && respDoc.containsKey("error")) {
                    errorMsg += respDoc["error"].as<String>();
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