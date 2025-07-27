# Writing Data to NFC Card Memory Blocks (MIFARE Classic)

This guide explains how to write and read data to/from the memory blocks of a MIFARE Classic NFC card using the Adafruit PN532 library and an ESP32.

---

## 1. Understanding NFC Card Memory
- **UID:** Each card has a unique, read-only identifier (UID).
- **Memory Blocks:** MIFARE Classic cards have 16 sectors, each with 4 blocks (total 64 blocks). Each block holds 16 bytes.
- **Sector Trailer:** The last block in each sector (e.g., 3, 7, 11, ...) is the sector trailer and stores keys/access bits. **Never overwrite these!**
- **User Data:** Store your data in blocks that are NOT sector trailers (e.g., block 4, 5, 6, 8, 9, 10, etc.).

---

## 2. Writing Data to a Card

### a. Authenticate the Block
Before you can write, you must authenticate the block using a key (default for new cards is usually `0xFFFFFFFFFFFF`).

### b. Write Data
- Data must be exactly 16 bytes (pad with zeros if needed).
- Use the Adafruit PN532 library's `mifareclassic_WriteDataBlock()` function.

### c. Example Code (with comments)
```cpp
#include <Wire.h> // I2C communication library
#include <Adafruit_PN532.h> // Adafruit PN532 NFC library

#define SDA_PIN 21 // ESP32 I2C SDA pin
#define SCL_PIN 22 // ESP32 I2C SCL pin
Adafruit_PN532 nfc(SDA_PIN, SCL_PIN); // Create PN532 object

void setup() {
  Serial.begin(115200); // Start serial communication for debug output
  nfc.begin(); // Initialize PN532 module
  nfc.SAMConfig(); // Configure PN532 to read cards
  Serial.println("Place card to write...");

  uint8_t keya[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }; // Default key for new cards

  while (1) {
    uint8_t uid[7]; // Buffer to hold card UID
    uint8_t uidLength; // Length of UID
    // Wait for a card to be detected
    if (nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength)) {
      Serial.print("Card detected! UID: ");
      for (uint8_t i = 0; i < uidLength; i++) {
        Serial.print(uid[i], HEX); Serial.print(" "); // Print UID in hex
      }
      Serial.println();

      // Authenticate block 4 (not a sector trailer)
      if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya)) {
        // Data to write (16 bytes, pad with zeros if needed)
        uint8_t data[16] = { 'U', 'S', 'E', 'R', '1', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        // Write data to block 4
        if (nfc.mifareclassic_WriteDataBlock(4, data)) {
          Serial.println("Write successful!");
        } else {
          Serial.println("Write failed!");
        }
      } else {
        Serial.println("Auth failed!"); // Authentication failed
      }
      delay(2000); // Wait before next attempt
    }
  }
}

void loop() {} // Not used
```

---

## 3. Reading Data from a Card (with comments)

```cpp
// ...inside your card detection code...
// Authenticate block 4 before reading
if (nfc.mifareclassic_AuthenticateBlock(uid, uidLength, 4, 0, keya)) {
  uint8_t data[16]; // Buffer to hold read data
  // Read 16 bytes from block 4
  if (nfc.mifareclassic_ReadDataBlock(4, data)) {
    Serial.print("Block 4 data: ");
    for (int i = 0; i < 16; i++) Serial.print((char)data[i]); // Print as characters
    Serial.println();
  } else {
    Serial.println("Read failed!");
  }
}
```

---

## 4. Tips & Warnings
- **Never write to sector trailer blocks** (3, 7, 11, ...).
- **Always authenticate** before reading or writing.
- **Data is not encrypted**—anyone with a reader and the key can access it.
- **UID is read-only**—use it as a unique identifier for your database.
- **Test with a spare card** to avoid bricking your main cards.

---

## 5. When to Store Data on the Card
- Use card memory for offline systems or when you want the card to carry its own data (e.g., balance, access level).
- For most secure systems, store only the UID on the card and keep all user data in your backend/database.

---

## 6. References
- [Adafruit PN532 Library](https://github.com/adafruit/Adafruit-PN532)
- [MIFARE Classic 1K Datasheet](https://www.nxp.com/docs/en/data-sheet/MF1S503x.pdf)

---

If you need a full example for a specific use case (e.g., storing balance, attendance, etc.), let me know!
