/*
 * Orbital Temple Satellite - LoRa Communication Implementation
 * Version: 1.21
 *
 * CRITICAL FIXES from V1.2:
 *
 * 1. REMOVED DUPLICATE DEFINITIONS:
 *    In V1.2, receivedFlag and setFlag() were defined in BOTH lora.cpp
 *    AND loop.cpp. This violated the One Definition Rule and could cause
 *    the radio interrupt to update one variable while the loop checked
 *    a different variable.
 *
 *    FIX: receivedFlag is now defined ONLY in config.cpp, declared extern
 *    in config.h. setFlag() is defined here only.
 *
 * 2. CONSISTENT SYNC WORD:
 *    In V1.2, startRadio() used sync word 0x10, but return2Rec() and
 *    sendMensage() used 0x12. After first transmission, the satellite
 *    might not receive messages from ground station using 0x10.
 *
 *    FIX: All functions now use LORA_SYNC_WORD from config.h (0x12).
 *
 * 3. IMPROVED ERROR HANDLING:
 *    Added retry limits and recovery mechanism instead of infinite loops
 *    or immediate ESP.restart().
 */

#include <Arduino.h>
#include "config.h"
#include "lora.h"

// Maximum retry attempts before considering radio failed
#define MAX_INIT_RETRIES    5
#define MAX_TX_RETRIES      3
#define RETRY_DELAY_MS      1000

// ==================== ISR CALLBACK ====================
// This is the interrupt service routine called when a packet is received
// NOTE: receivedFlag is defined in config.cpp, declared extern in config.h

#if defined(ESP8266) || defined(ESP32)
    ICACHE_RAM_ATTR
#endif
void setFlag(void) {
    receivedFlag = true;
}

// ==================== RADIO INITIALIZATION ====================
bool startRadio() {
    Serial.println("[LORA] Initializing radio...");

    int retries = 0;
    int state;

    // Try to initialize radio with retries
    while (retries < MAX_INIT_RETRIES) {
        // Feed watchdog during potentially long operation
        feedWatchdog();

        Serial.printf("[LORA] Init attempt %d/%d\n", retries + 1, MAX_INIT_RETRIES);

        // Initialize radio with parameters from config.h
        // Using CONSISTENT sync word (was inconsistent in V1.2)
        state = radio.begin(
            LORA_FREQ_RX,       // Frequency
            LORA_BW,            // Bandwidth
            LORA_SF,            // Spreading Factor
            LORA_CR,            // Coding Rate
            LORA_SYNC_WORD,     // Sync Word - NOW CONSISTENT!
            LORA_PREAMBLE       // Preamble Length
        );

        if (state == RADIOLIB_ERR_NONE) {
            Serial.println("[LORA] Radio initialized successfully");
            break;
        }

        Serial.printf("[LORA] Init failed, code: %d\n", state);
        retries++;
        delay(RETRY_DELAY_MS);
    }

    if (state != RADIOLIB_ERR_NONE) {
        Serial.println("[LORA] ERROR: Radio initialization failed after all retries!");
        RFOK = false;
        contR = MAX_INIT_RETRIES;
        return false;
    }

    // Set up receive callback
    radio.setPacketReceivedAction(setFlag);

    // Start receiving
    Serial.println("[LORA] Starting receive mode...");
    state = radio.startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[LORA] Receive mode started successfully");
        RFOK = true;
        contR = 0;
        return true;
    } else {
        Serial.printf("[LORA] ERROR: startReceive failed, code: %d\n", state);
        RFOK = false;
        contR++;
        return false;
    }
}

// ==================== RETURN TO RECEIVE MODE ====================
bool returnToReceive() {
    Serial.println("[LORA] Returning to receive mode...");

    // Feed watchdog
    feedWatchdog();

    // Re-initialize radio for RX frequency with CONSISTENT sync word
    int state = radio.begin(
        LORA_FREQ_RX,
        LORA_BW,
        LORA_SF,
        LORA_CR,
        LORA_SYNC_WORD,     // Same sync word as TX!
        LORA_PREAMBLE
    );

    if (state != RADIOLIB_ERR_NONE) {
        Serial.printf("[LORA] ERROR: RX init failed, code: %d\n", state);
        RFOK = false;
        contR++;
        return false;
    }

    // Re-set receive callback
    radio.setPacketReceivedAction(setFlag);

    // Start receiving
    state = radio.startReceive();

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[LORA] Back in receive mode");
        RFOK = true;
        contR = 0;
        return true;
    } else {
        Serial.printf("[LORA] ERROR: startReceive failed, code: %d\n", state);
        RFOK = false;
        contR++;
        return false;
    }
}

// ==================== SEND MESSAGE ====================
bool sendMessage(const String& message) {
    Serial.print("[LORA] Sending: ");
    Serial.println(message);

    // Feed watchdog before transmission
    feedWatchdog();

    int retries = 0;
    int state;

    // Configure radio for TX frequency
    while (retries < MAX_TX_RETRIES) {
        state = radio.begin(
            LORA_FREQ_TX,       // TX frequency
            LORA_BW,
            LORA_SF,
            LORA_CR,
            LORA_SYNC_WORD,     // Same sync word!
            LORA_PREAMBLE
        );

        if (state == RADIOLIB_ERR_NONE) {
            break;
        }

        Serial.printf("[LORA] TX config failed, retry %d/%d\n", retries + 1, MAX_TX_RETRIES);
        retries++;
        delay(RETRY_DELAY_MS);
        feedWatchdog();
    }

    if (state != RADIOLIB_ERR_NONE) {
        Serial.println("[LORA] ERROR: Could not configure for TX!");
        RFOK = false;
        contE = MAX_TX_RETRIES;
        returnToReceive();  // Try to at least get back to RX
        return false;
    }

    // Small delay before transmitting
    delay(100);

    // Transmit the message (make a copy since RadioLib takes non-const String&)
    String txMessage = message;
    state = radio.transmit(txMessage);

    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[LORA] Message sent successfully");
        Serial.printf("[LORA] Datarate: %.2f bps\n", radio.getDataRate());
        contE = 0;

        // Return to receive mode
        returnToReceive();
        return true;

    } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
        Serial.println("[LORA] ERROR: Message too long!");
        soakTxErrors++;  // Track for soak test
        returnToReceive();
        return false;

    } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
        Serial.println("[LORA] ERROR: TX timeout!");
        contE++;
        soakTxErrors++;  // Track for soak test
        returnToReceive();
        return false;

    } else {
        Serial.printf("[LORA] ERROR: TX failed, code: %d\n", state);
        contE++;
        soakTxErrors++;  // Track for soak test
        returnToReceive();
        return false;
    }
}

// ==================== RADIO RECOVERY ====================
bool radioNeedsRecovery() {
    // Radio needs recovery if too many consecutive failures
    return (contR > 5 || contE > 5 || !RFOK);
}

bool recoverRadio() {
    Serial.println("[LORA] Attempting radio recovery...");
    soakRadioResets++;  // Track for soak test

    // Reset counters
    contR = 0;
    contE = 0;

    // Try to reinitialize
    if (startRadio()) {
        Serial.println("[LORA] Radio recovered successfully");
        return true;
    }

    Serial.println("[LORA] Radio recovery failed!");
    return false;
}
