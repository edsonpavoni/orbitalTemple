/*
 * =============================================================================
 *   ___  ____  ____ ___ _____  _    _       _____ _____ __  __ ____  _     _____
 *  / _ \|  _ \| __ )_ _|_   _|/ \  | |     |_   _| ____|  \/  |  _ \| |   | ____|
 * | | | | |_) |  _ \| |  | | / _ \ | |       | | |  _| | |\/| | |_) | |   |  _|
 * | |_| |  _ <| |_) | |  | |/ ___ \| |___    | | | |___| |  | |  __/| |___| |___
 *  \___/|_| \_\____/___| |_/_/   \_\_____|   |_| |_____|_|  |_|_|   |_____|_____|
 *
 * =============================================================================
 *
 * ORBITAL TEMPLE SATELLITE FIRMWARE
 * Version: 1.21
 * Date: 2025-12-16
 *
 * A memorial in outer space, open to every sentient being
 * who lives or has lived on Earth.
 *
 * =============================================================================
 *
 * HARDWARE:
 * - ESP32 microcontroller
 * - SX1262 LoRa radio module
 * - LSM9DS1 IMU sensor
 * - SD card for data storage
 * - Thermistor temperature sensor
 * - Luminosity sensor
 * - Battery voltage monitor
 * - Antenna deployment mechanism (burn wire)
 *
 * =============================================================================
 *
 * VERSION HISTORY:
 *
 * V1.21 (2025-12-16) - Critical Bug Fixes
 * - FIXED: Sensor variable shadowing (telemetry was showing zeros)
 * - FIXED: Duplicate receivedFlag/setFlag definitions
 * - FIXED: LoRa sync word inconsistency
 * - ADDED: Hardware watchdog timer (60 second timeout)
 * - ADDED: HMAC message authentication
 * - ADDED: Non-blocking state machine (no more blocking delays)
 * - ADDED: State persistence (EEPROM)
 * - ADDED: Telemetry timestamps
 * - ADDED: SD card status tracking
 * - ADDED: Division-by-zero protection
 * - ADDED: Memory-safe directory listing
 * - ADDED: Input validation for all commands
 *
 * V1.2 (2025-12) - Modular Architecture
 * - Refactored code into separate modules
 * - Removed serial wait block (V1.1 flight blocker)
 * - Improved IMU failure handling
 *
 * V1.1 (2025-11) - Initial Flight Candidate
 * - Multiple critical issues identified in code review
 * - NOT FLIGHT READY
 *
 * =============================================================================
 *
 * COMMUNICATION PROTOCOL:
 *
 * RX Frequency: 401.5 MHz
 * TX Frequency: 468.5 MHz
 * Modulation: LoRa, SF9, BW 125kHz
 * Sync Word: 0x12
 *
 * Command Format: SAT_ID-COMMAND&PATH@DATA#HMAC
 *
 * Commands:
 * - Status     : Request telemetry
 * - Ping       : Connectivity test
 * - ListDir    : List SD card directory
 * - CreateDir  : Create directory
 * - RemoveDir  : Remove directory
 * - WriteFile  : Write to file
 * - AppendFile : Append to file
 * - ReadFile   : Read file contents
 * - RenameFile : Rename file
 * - DeleteFile : Delete file
 * - TestFileIO : Test SD card performance
 * - MCURestart : Restart satellite
 * - GetState   : Get current state
 * - ForceOperational : Skip antenna deployment (emergency)
 *
 * =============================================================================
 *
 * AD ASTRA
 *
 * =============================================================================
 */

#include <Arduino.h>
#include <stdint.h>
#include "config.h"
#include "setup.h"
#include "loop.h"

// ==================== ARDUINO SETUP ====================
void setup() {
    // Initialize all hardware and load state
    setupGeneral();

    // Start the main loop
    // Note: In V1.2, mainLoop() was called here AND in loop()
    // In V1.21, we only call it in loop() to avoid confusion
}

// ==================== ARDUINO LOOP ====================
void loop() {
    // Main state machine
    mainLoop();
}
