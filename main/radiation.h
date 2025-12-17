#ifndef RADIATION_H
#define RADIATION_H

/*
 * Orbital Temple Satellite - Radiation Protection Module
 * Version: 1.21
 *
 * Protection against Single Event Upsets (SEUs) caused by
 * charged particles flipping bits in memory.
 *
 * TECHNIQUES USED:
 *
 * 1. TRIPLE MODULAR REDUNDANCY (TMR)
 *    - Critical variables stored 3 times
 *    - Voting logic: 2-out-of-3 majority wins
 *    - Single bit flip is detected and corrected
 *
 * 2. CRC32 CHECKSUM
 *    - EEPROM data protected with CRC
 *    - Corruption detected on boot
 *
 * 3. PERIODIC SCRUBBING
 *    - TMR variables checked every SCRUB_INTERVAL
 *    - Mismatches corrected automatically
 *
 * LIMITATIONS:
 *    - Cannot protect against multi-bit upsets (MBUs)
 *    - Cannot protect code in flash memory
 *    - ESP32 has no hardware ECC
 */

#include <Arduino.h>
#include <stdint.h>

// ==================== CONFIGURATION ====================

// How often to scrub TMR variables (milliseconds)
#define SCRUB_INTERVAL  10000  // Every 10 seconds

// ==================== TMR DATA STRUCTURE ====================

// Triple Modular Redundancy container for any type
template<typename T>
struct TMR {
    T copy1;
    T copy2;
    T copy3;
};

// ==================== TMR CRITICAL VARIABLES ====================
// These are the most important variables that must survive bit flips

extern TMR<uint8_t> tmr_missionState;      // Mission state machine
extern TMR<uint8_t> tmr_antennaState;      // Antenna deployment state
extern TMR<bool>    tmr_antennaDeployed;   // Antenna deployed flag
extern TMR<bool>    tmr_groundContact;     // Ground contact established
extern TMR<bool>    tmr_rfOK;              // Radio status
extern TMR<bool>    tmr_imuOK;             // IMU status
extern TMR<bool>    tmr_sdOK;              // SD card status
extern TMR<uint32_t> tmr_bootCount;        // Boot counter

// ==================== TMR FUNCTIONS ====================

// Initialize TMR variable with a value (sets all 3 copies)
template<typename T>
void tmrWrite(TMR<T>& tmr, T value) {
    tmr.copy1 = value;
    tmr.copy2 = value;
    tmr.copy3 = value;
}

// Read TMR variable with voting (returns majority value)
template<typename T>
T tmrRead(TMR<T>& tmr) {
    // 2-out-of-3 voting
    if (tmr.copy1 == tmr.copy2) return tmr.copy1;
    if (tmr.copy1 == tmr.copy3) return tmr.copy1;
    if (tmr.copy2 == tmr.copy3) return tmr.copy2;

    // All three different - catastrophic, return copy1 and log
    Serial.println("[RAD] WARNING: TMR all copies differ!");
    return tmr.copy1;
}

// Check and correct TMR variable (returns true if correction was made)
template<typename T>
bool tmrScrub(TMR<T>& tmr) {
    T correct = tmrRead(tmr);
    bool corrected = false;

    if (tmr.copy1 != correct) {
        tmr.copy1 = correct;
        corrected = true;
        Serial.println("[RAD] Corrected TMR copy1");
    }
    if (tmr.copy2 != correct) {
        tmr.copy2 = correct;
        corrected = true;
        Serial.println("[RAD] Corrected TMR copy2");
    }
    if (tmr.copy3 != correct) {
        tmr.copy3 = correct;
        corrected = true;
        Serial.println("[RAD] Corrected TMR copy3");
    }

    return corrected;
}

// ==================== CRC32 FUNCTIONS ====================

// Calculate CRC32 of a data block
uint32_t calculateCRC32(const uint8_t* data, size_t length);

// ==================== EEPROM WITH CRC ====================

// EEPROM layout with CRC protection:
// [0]      Magic byte (0xAB)
// [1-N]    State data
// [N+1-N+4] CRC32 of bytes 0-N

#define EEPROM_CRC_OFFSET  100  // CRC stored at byte 100-103

// Save state with CRC protection
void saveStateWithCRC();

// Load state with CRC verification (returns false if corrupted)
bool loadStateWithCRC();

// ==================== SCRUBBING ====================

// Scrub all TMR variables, correct any bit flips
// Returns number of corrections made
int scrubAllTMR();

// Initialize radiation protection
void initRadiationProtection();

// Periodic radiation protection check (call from main loop)
void radiationProtectionTick();

// Get radiation protection status
String getRadiationStatus();

// ==================== STATISTICS ====================

extern uint32_t seuCorrectionsTotal;   // Total SEU corrections since boot
extern uint32_t lastScrubTime;         // Last scrub timestamp

#endif // RADIATION_H
