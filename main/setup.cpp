/*
 * Orbital Temple Satellite - Setup Implementation
 * Version: 1.21
 *
 * CRITICAL FIXES from V1.2:
 *
 * 1. REMOVED `while (!Serial)`:
 *    V1.1 had this and it was identified as a flight blocker.
 *    V1.2 removed it - confirmed still removed in V1.21.
 *
 * 2. ADDED WATCHDOG TIMER:
 *    ESP32 hardware watchdog is now initialized.
 *    If code hangs for more than WDT_TIMEOUT_SECONDS, device resets.
 *
 * 3. ADDED STATE PERSISTENCE:
 *    EEPROM is initialized and previous state is loaded.
 *    Antenna deployment status survives reboots.
 *
 * 4. NON-BLOCKING INITIALIZATION:
 *    If any peripheral fails to initialize, we set a flag and continue.
 *    Satellite can still operate with degraded capabilities.
 */

#include <Arduino.h>
#include <stdint.h>
#include "config.h"
#include "setup.h"
#include "lora.h"
#include "id.h"
#include "sensors.h"

void setupGeneral() {
    // Initialize serial first for debugging
    Serial.begin(115200);

    // Wait a moment for serial to stabilize (but don't block on it!)
    delay(1000);

    Serial.println();
    Serial.println("=============================================");
    Serial.println("  ORBITAL TEMPLE SATELLITE");
    Serial.println("  Firmware Version: 1.21");
    Serial.println("  A memorial in outer space");
    Serial.println("=============================================");
    Serial.println();

    // ==================== WATCHDOG INITIALIZATION ====================
    Serial.println("[SETUP] Initializing watchdog timer...");
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, WDT_PANIC_ON_TIMEOUT);
    esp_task_wdt_add(NULL);  // Add current task to watchdog
    Serial.printf("[SETUP] Watchdog configured: %d second timeout\n", WDT_TIMEOUT_SECONDS);

    // Feed watchdog
    feedWatchdog();

    // ==================== EEPROM INITIALIZATION ====================
    Serial.println("[SETUP] Initializing EEPROM...");
    EEPROM.begin(EEPROM_SIZE);

    // Load previous state (or initialize if first boot)
    loadState();

    // Feed watchdog
    feedWatchdog();

    // ==================== SATELLITE ID ====================
    Serial.println("[SETUP] Loading satellite ID...");
    getId();

    // ==================== PIN CONFIGURATION ====================
    Serial.println("[SETUP] Configuring pins...");

    // Antenna deployment
    pinMode(AntSwitch, INPUT);
    pinMode(R1, OUTPUT);
    digitalWrite(R1, LOW);  // Ensure burn wire is off

    // Sensors
    pinMode(VBAT_DR, INPUT);
    pinMode(TL, INPUT_PULLUP);
    pinMode(ThermistorPin, INPUT);

    Serial.println("[SETUP] Pins configured");

    // Feed watchdog
    feedWatchdog();

    // ==================== POWER SAVING ====================
    Serial.println("[SETUP] Disabling WiFi and Bluetooth for power saving...");
    WiFi.mode(WIFI_OFF);
    btStop();

    // ==================== IMU INITIALIZATION ====================
    Serial.println("[SETUP] Initializing IMU...");
    BeginIMU();
    // Note: BeginIMU() sets IMUOK flag, doesn't hang on failure

    // Feed watchdog
    feedWatchdog();

    // ==================== SD CARD INITIALIZATION ====================
    Serial.println("[SETUP] Initializing SD card...");
    SDBegin();
    // Note: SDBegin() sets SDOK flag, doesn't hang on failure

    // Feed watchdog
    feedWatchdog();

    // ==================== INITIAL SENSOR READING ====================
    Serial.println("[SETUP] Reading initial sensor values...");
    readBatteryVoltage();

    Serial.print("[SETUP] Battery voltage: ");
    Serial.print(VT);
    Serial.println("V");

    // Feed watchdog
    feedWatchdog();

    // ==================== RADIO INITIALIZATION ====================
    Serial.println("[SETUP] Initializing LoRa radio...");
    if (startRadio()) {
        Serial.println("[SETUP] Radio initialized successfully");
    } else {
        Serial.println("[SETUP] WARNING: Radio initialization failed!");
        Serial.println("[SETUP] Will retry in main loop");
    }

    // ==================== FINAL SETUP ====================

    // Small delay for stability
    delay(500);

    // Feed watchdog
    feedWatchdog();

    // Print status summary
    Serial.println();
    Serial.println("=============================================");
    Serial.println("  SETUP COMPLETE - STATUS SUMMARY");
    Serial.println("=============================================");
    Serial.print("  IMU:      ");
    Serial.println(IMUOK ? "OK" : "FAILED");
    Serial.print("  SD Card:  ");
    Serial.println(SDOK ? "OK" : "FAILED");
    Serial.print("  Radio:    ");
    Serial.println(RFOK ? "OK" : "FAILED");
    Serial.print("  Antenna:  ");
    Serial.println(antennaDeployed ? "DEPLOYED" : "PENDING");
    Serial.print("  Boot #:   ");
    Serial.println(bootCount);
    Serial.print("  State:    ");
    Serial.println((int)currentState);
    Serial.println("=============================================");
    Serial.println();

    // Log startup to SD card
    if (SDOK) {
        char logMsg[100];
        snprintf(logMsg, sizeof(logMsg),
                 "BOOT #%lu - IMU:%s SD:%s RF:%s ANT:%s",
                 bootCount,
                 IMUOK ? "OK" : "FAIL",
                 SDOK ? "OK" : "FAIL",
                 RFOK ? "OK" : "FAIL",
                 antennaDeployed ? "DEPLOYED" : "PENDING");
        logToSD(logMsg);
    }

    Serial.println("[SETUP] Entering main loop...");
    Serial.println();
}
