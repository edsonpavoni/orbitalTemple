#ifndef LORA_H
#define LORA_H

/*
 * Orbital Temple Satellite - LoRa Communication Module
 * Version: 1.21
 *
 * CHANGELOG from V1.2:
 * - FIXED: Removed duplicate receivedFlag/setFlag definitions (now in config.cpp)
 * - FIXED: Sync word now consistent (uses LORA_SYNC_WORD from config.h)
 * - Uses centralized radio configuration from config.h
 * - Added retry mechanism with counter limits
 * - Improved error handling
 */

// ISR callback for packet received
#if defined(ESP8266) || defined(ESP32)
    ICACHE_RAM_ATTR
#endif
void setFlag(void);

// Initialize radio in receive mode
// Returns true on success, false on failure
bool startRadio();

// Send a message via LoRa
// Automatically switches to TX frequency, transmits, then returns to RX
// Returns true on success, false on failure
bool sendMessage(const String& message);

// Return radio to receive mode after transmission
// Returns true on success, false on failure
bool returnToReceive();

// Check if radio needs recovery (after multiple failures)
bool radioNeedsRecovery();

// Attempt to recover radio after failures
bool recoverRadio();

#endif // LORA_H
