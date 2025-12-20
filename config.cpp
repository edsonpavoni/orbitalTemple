/*
 * Orbital Temple Satellite - Configuration Implementation
 * Version: 1.21
 *
 * Contains:
 * - Global variable definitions
 * - HMAC authentication functions
 * - State persistence (EEPROM)
 * - Watchdog feeding
 */

#include "config.h"
#include "radiation.h"
#include "accel.h"
#include "secrets.h"  // HMAC key - this file should NOT be committed to git

// Forward declaration for battery reading (defined in sensors.cpp)
extern void readBatteryVoltage();

// ==================== RADIO ====================
// RFM95 module (SX1276 chip) - uses DIO0 for interrupts
SX1276 radio = new Module(CS_RF, DIO0_RF, RST_RF);

// LoRa receive flag - SINGLE DEFINITION (was duplicated in V1.2)
volatile bool receivedFlag = false;

String MsR = "";

// ==================== SATELLITE ID ====================
String sat_id = "";

// ==================== STATE MACHINE ====================
MissionState currentState = STATE_BOOT;
AntennaState antennaState = ANT_IDLE;
unsigned long stateStartTime = 0;
unsigned long lastWdtFeed = 0;
unsigned long missionStartTime = 0;
uint32_t bootCount = 0;

// ==================== ANTENNA DEPLOYMENT ====================
int deployRetryCount = 0;
bool antennaDeployed = false;

// ==================== BEACON SYSTEM ====================
bool groundContactEstablished = false;    // True after first successful command received
unsigned long lastGroundContact = 0;       // Time of last command from ground station
unsigned long lastBeaconTime = 0;          // Time of last beacon transmission

// ==================== HARDWARE STATUS FLAGS ====================
bool IMUOK = true;
bool RFOK = true;
bool SDOK = false;  // Start false, set true only after successful init

// ==================== LORA RETRY COUNTERS ====================
int contE = 0;
int contR = 0;

// ==================== SOAK TEST COUNTERS ====================
uint32_t soakBeaconsSent = 0;
uint32_t soakBeaconsSkipped = 0;
uint32_t soakCommandsReceived = 0;
uint32_t soakCommandsFailed = 0;
uint32_t soakTxErrors = 0;
uint32_t soakRxErrors = 0;
uint32_t soakRadioResets = 0;
uint32_t soakLoopIterations = 0;
unsigned long soakLastHourlyLog = 0;
unsigned long soakLastDailyLog = 0;

// ==================== SENSORS: BATTERY ====================
int VM1 = 0;
float VE = 0.0f;
float VT = 0.0f;

// ==================== SENSORS: TEMPERATURE ====================
double adcMax = 4095.0;
double Vs = 3.3;
double R1z = 10000.0;
double Beta = 3950.0;
double To = 298.15;
double Ro = 10000.0;
double Tc = 0.0;

// ==================== SENSORS: LUMINOSITY ====================
float VM = 0.0f;
float VP = 0.0f;
float amps = 0.0f;
float microamps = 0.0f;
float lux = 0.0f;

// ==================== SENSORS: IMU ====================
LSM9DS1 imu;

// ==================== WATCHDOG ====================
void feedWatchdog() {
    esp_task_wdt_reset();
    lastWdtFeed = millis();
}

// ==================== STATE PERSISTENCE ====================
// Note: State loading is handled by initRadiationProtection() in radiation.cpp
// which calls loadStateWithCRC() for CRC-verified state restoration.

void saveState() {
    // Sync critical variables to TMR copies for radiation protection
    tmrWrite(tmr_missionState, (uint8_t)currentState);
    tmrWrite(tmr_antennaState, (uint8_t)antennaState);
    tmrWrite(tmr_antennaDeployed, antennaDeployed);
    tmrWrite(tmr_groundContact, groundContactEstablished);
    tmrWrite(tmr_rfOK, RFOK);
    tmrWrite(tmr_imuOK, IMUOK);
    tmrWrite(tmr_sdOK, SDOK);
    tmrWrite(tmr_bootCount, bootCount);

    // Use CRC-protected save
    saveStateWithCRC();
}

// ==================== HMAC AUTHENTICATION ====================

String calculateHMAC(const String& message) {
    uint8_t hmacResult[32];

    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, HMAC_KEY, HMAC_KEY_LENGTH);
    mbedtls_md_hmac_update(&ctx, (const unsigned char*)message.c_str(), message.length());
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    // Convert to hex string (first 8 bytes = 16 hex chars for brevity in LoRa)
    String hmacHex = "";
    for (int i = 0; i < 8; i++) {
        if (hmacResult[i] < 16) hmacHex += "0";
        hmacHex += String(hmacResult[i], HEX);
    }

    return hmacHex;
}

bool verifyHMAC(const String& message, const String& receivedHMAC) {
    String calculatedHMAC = calculateHMAC(message);

    // Case-insensitive comparison
    String recvLower = receivedHMAC;
    String calcLower = calculatedHMAC;
    recvLower.toLowerCase();
    calcLower.toLowerCase();

    bool valid = recvLower.equals(calcLower);

    if (!valid) {
        Serial.println("[AUTH] HMAC verification failed!");
        Serial.print("[AUTH] Expected: ");
        Serial.println(calcLower);
        Serial.print("[AUTH] Received: ");
        Serial.println(recvLower);
    }

    return valid;
}

// ==================== BEACON SYSTEM ====================
/*
 * Adaptive Beacon System
 *
 * The beacon is sent at different intervals depending on ground contact status:
 *
 * 1. BEFORE FIRST CONTACT (groundContactEstablished = false):
 *    - Beacon every BEACON_INTERVAL_NO_CONTACT (default: 1 minute)
 *    - This helps ground station find and track the satellite
 *
 * 2. AFTER CONTACT ESTABLISHED (groundContactEstablished = true):
 *    - Beacon every BEACON_INTERVAL_NORMAL (default: 1 hour)
 *    - Reduces power consumption and channel usage
 *
 * 3. LOST CONTACT (no contact for > BEACON_LOST_THRESHOLD):
 *    - Beacon every BEACON_INTERVAL_LOST (default: 5 minutes)
 *    - Helps re-establish contact if ground station has issues
 *
 * Configure timing in config.h (BEACON_* defines)
 */

// Forward declaration for sendMessage (defined in lora.cpp)
extern bool sendMessage(const String& message);

// Get the appropriate beacon interval based on contact status
unsigned long getBeaconInterval() {
    unsigned long now = millis();

    if (!groundContactEstablished) {
        // No contact yet - beacon every 4 minutes to help ground find us
        Serial.println("[BEACON] Interval: NO_CONTACT (every 4 min)");
        return BEACON_INTERVAL_NO_CONTACT;
    }

    // Check if we've lost contact (no command for > threshold)
    unsigned long timeSinceContact = now - lastGroundContact;

    if (timeSinceContact > BEACON_LOST_THRESHOLD) {
        // Lost contact - beacon every 8 minutes
        Serial.printf("[BEACON] Interval: LOST (every 8 min, no contact for %lu hours)\n",
                      timeSinceContact / 3600000UL);
        return BEACON_INTERVAL_LOST;
    }

    // Normal operation - beacon every 1 hour
    Serial.println("[BEACON] Interval: NORMAL (every 1 hour)");
    return BEACON_INTERVAL_NORMAL;
}

// Register that we received a valid command from ground station
void registerGroundContact() {
    unsigned long now = millis();

    bool isFirstContact = !groundContactEstablished;

    if (isFirstContact) {
        Serial.println("[BEACON] First ground contact established!");
        groundContactEstablished = true;
    }

    lastGroundContact = now;
    Serial.printf("[BEACON] Ground contact registered at T+%lu ms\n",
                  now - missionStartTime);

    // Trigger first accelerometer recording on initial contact
    if (isFirstContact) {
        checkFirstContactRecording();
    }
}

// Send a beacon message
void sendBeacon() {
    unsigned long now = millis();

    // ==================== BATTERY CHECK ====================
    // Read battery voltage before sending beacon
    Serial.println("[BEACON] Checking battery voltage...");
    readBatteryVoltage();

    // Skip beacon if battery is too low (power saving mode)
    if (VT < BEACON_MIN_BATTERY_VOLTAGE && VT > 0) {
        Serial.printf("[BEACON] LOW BATTERY (%.2fV < %.1fV) - Skipping beacon to save power\n",
                      VT, BEACON_MIN_BATTERY_VOLTAGE);
        // Still update lastBeaconTime to maintain interval timing
        lastBeaconTime = millis();
        soakBeaconsSkipped++;  // Track for soak test
        return;
    }

    Serial.printf("[BEACON] Battery OK: %.2fV\n", VT);

    // Choose beacon message based on contact status
    String beacon;
    if (!groundContactEstablished) {
        // Searching for Earth
        Serial.println("[BEACON] Mode: SEARCHING (every 4 min)");
        beacon = BEACON_MSG_SEARCHING;
    } else {
        // Check if lost
        unsigned long timeSinceContact = now - lastGroundContact;
        if (timeSinceContact > BEACON_LOST_THRESHOLD) {
            // Lost contact
            Serial.println("[BEACON] Mode: LOST (every 8 min)");
            beacon = BEACON_MSG_LOST;
        } else {
            // Connected
            Serial.println("[BEACON] Mode: CONNECTED (every 1 hour)");
            beacon = BEACON_MSG_CONNECTED;
        }
    }

    beacon += "|";

    // Add mission elapsed time
    unsigned long elapsed = now - missionStartTime;
    unsigned long hours = elapsed / 3600000UL;
    unsigned long minutes = (elapsed % 3600000UL) / 60000UL;
    unsigned long seconds = (elapsed % 60000UL) / 1000UL;

    char timeStr[20];
    snprintf(timeStr, sizeof(timeStr), "T+%02lu:%02lu:%02lu", hours, minutes, seconds);
    beacon += timeStr;

    // Add boot count
    beacon += "|B:";
    beacon += String(bootCount);

    // Add contact status
    beacon += "|C:";
    beacon += groundContactEstablished ? "YES" : "NO";

    // Add battery voltage
    beacon += "|V:";
    beacon += String(VT, 1);

    Serial.println("[BEACON] Sending: " + beacon);
    sendMessage(beacon);

    lastBeaconTime = millis();
    soakBeaconsSent++;  // Track for soak test
}

// ==================== SOAK TEST LOGGING ====================
// Comprehensive logging for 7-day endurance test debugging

// Forward declarations for functions we need
extern void logToSD(const char* message);

// Get free heap memory (detect memory leaks)
uint32_t getFreeHeap() {
    return ESP.getFreeHeap();
}

// Format uptime as days:hours:minutes:seconds
String formatUptime(unsigned long ms) {
    unsigned long seconds = ms / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    unsigned long days = hours / 24;

    seconds %= 60;
    minutes %= 60;
    hours %= 24;

    char buf[32];
    snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu", days, hours, minutes, seconds);
    return String(buf);
}

// Called every loop iteration - checks if it's time for logging
void soakTestTick() {
    unsigned long now = millis();
    soakLoopIterations++;  // Will overflow, that's OK

    // Hourly log
    if (now - soakLastHourlyLog >= SOAK_LOG_INTERVAL) {
        soakLogHourly();
        soakLastHourlyLog = now;
    }

    // Daily log
    if (now - soakLastDailyLog >= SOAK_DAILY_INTERVAL) {
        soakLogDaily();
        soakLastDailyLog = now;
    }
}

// Hourly status log - written to SD card and serial
void soakLogHourly() {
    unsigned long now = millis();

    Serial.println();
    Serial.println("╔═══════════════════════════════════════════════════════════════╗");
    Serial.println("║              SOAK TEST - HOURLY STATUS                        ║");
    Serial.println("╠═══════════════════════════════════════════════════════════════╣");
    Serial.printf("║ Uptime: %-50s  ║\n", formatUptime(now).c_str());
    Serial.printf("║ Boot Count: %-5lu    Free Heap: %-10lu bytes            ║\n",
                  (unsigned long)bootCount, (unsigned long)getFreeHeap());
    Serial.println("╠═══════════════════════════════════════════════════════════════╣");
    Serial.printf("║ Beacons Sent: %-8lu   Skipped (low bat): %-8lu         ║\n",
                  (unsigned long)soakBeaconsSent, (unsigned long)soakBeaconsSkipped);
    Serial.printf("║ Commands OK: %-9lu  Failed: %-8lu                     ║\n",
                  (unsigned long)soakCommandsReceived, (unsigned long)soakCommandsFailed);
    Serial.printf("║ TX Errors: %-11lu  RX Errors: %-8lu                   ║\n",
                  (unsigned long)soakTxErrors, (unsigned long)soakRxErrors);
    Serial.printf("║ Radio Resets: %-8lu                                        ║\n",
                  (unsigned long)soakRadioResets);
    Serial.println("╠═══════════════════════════════════════════════════════════════╣");
    Serial.printf("║ Battery: %.2fV   Temp: %.1fC   Contact: %-3s               ║\n",
                  VT, Tc, groundContactEstablished ? "YES" : "NO");
    Serial.printf("║ IMU: %-4s  SD: %-4s  RF: %-4s                                ║\n",
                  IMUOK ? "OK" : "FAIL", SDOK ? "OK" : "FAIL", RFOK ? "OK" : "FAIL");
    Serial.println("╚═══════════════════════════════════════════════════════════════╝");
    Serial.println();

    // Log to SD card for persistence
    if (SDOK) {
        char logEntry[256];
        snprintf(logEntry, sizeof(logEntry),
                 "HOURLY|UP:%s|BOOT:%lu|HEAP:%lu|BCN:%lu|SKIP:%lu|CMD:%lu|FAIL:%lu|TX_ERR:%lu|RX_ERR:%lu|RST:%lu|BAT:%.2f|TEMP:%.1f",
                 formatUptime(now).c_str(),
                 (unsigned long)bootCount,
                 (unsigned long)getFreeHeap(),
                 (unsigned long)soakBeaconsSent,
                 (unsigned long)soakBeaconsSkipped,
                 (unsigned long)soakCommandsReceived,
                 (unsigned long)soakCommandsFailed,
                 (unsigned long)soakTxErrors,
                 (unsigned long)soakRxErrors,
                 (unsigned long)soakRadioResets,
                 VT, Tc);
        logToSD(logEntry);
    }
}

// Daily summary - more comprehensive
void soakLogDaily() {
    unsigned long now = millis();
    unsigned long uptimeDays = now / 86400000UL;

    Serial.println();
    Serial.println("╔═══════════════════════════════════════════════════════════════╗");
    Serial.println("║         *** SOAK TEST - DAILY SUMMARY ***                     ║");
    Serial.printf("║                    DAY %lu COMPLETE                             ║\n", uptimeDays);
    Serial.println("╠═══════════════════════════════════════════════════════════════╣");
    Serial.printf("║ Total Uptime: %-48s  ║\n", formatUptime(now).c_str());
    Serial.printf("║ Boot Count: %-5lu (should be 1 for clean test)                ║\n",
                  (unsigned long)bootCount);
    Serial.printf("║ Free Heap: %-10lu bytes                                    ║\n",
                  (unsigned long)getFreeHeap());
    Serial.println("╠═══════════════════════════════════════════════════════════════╣");
    Serial.println("║ COMMUNICATION STATS:                                          ║");
    Serial.printf("║   Beacons Sent: %-10lu                                     ║\n",
                  (unsigned long)soakBeaconsSent);
    Serial.printf("║   Beacons Skipped: %-7lu (low battery)                      ║\n",
                  (unsigned long)soakBeaconsSkipped);
    Serial.printf("║   Commands Received: %-5lu                                   ║\n",
                  (unsigned long)soakCommandsReceived);
    Serial.printf("║   Commands Failed: %-7lu                                    ║\n",
                  (unsigned long)soakCommandsFailed);
    Serial.println("╠═══════════════════════════════════════════════════════════════╣");
    Serial.println("║ ERROR COUNTS:                                                 ║");
    Serial.printf("║   TX Errors: %-10lu                                        ║\n",
                  (unsigned long)soakTxErrors);
    Serial.printf("║   RX Errors: %-10lu                                        ║\n",
                  (unsigned long)soakRxErrors);
    Serial.printf("║   Radio Resets: %-7lu                                       ║\n",
                  (unsigned long)soakRadioResets);
    Serial.println("╠═══════════════════════════════════════════════════════════════╣");
    Serial.printf("║ HEALTH: Battery=%.2fV Temp=%.1fC                            ║\n", VT, Tc);

    // Health assessment
    bool healthy = (bootCount == 1) &&
                   (soakCommandsFailed == 0) &&
                   (soakTxErrors < 10) &&
                   (soakRxErrors < 10) &&
                   (getFreeHeap() > 50000);

    Serial.printf("║ STATUS: %s                                             ║\n",
                  healthy ? "HEALTHY ✓" : "CHECK REQUIRED !");
    Serial.println("╚═══════════════════════════════════════════════════════════════╝");
    Serial.println();

    // Log to SD card
    if (SDOK) {
        char logEntry[300];
        snprintf(logEntry, sizeof(logEntry),
                 "DAILY|DAY:%lu|UP:%s|BOOT:%lu|HEAP:%lu|BCN:%lu|SKIP:%lu|CMD:%lu|FAIL:%lu|TX_ERR:%lu|RX_ERR:%lu|RST:%lu|BAT:%.2f|TEMP:%.1f|STATUS:%s",
                 uptimeDays,
                 formatUptime(now).c_str(),
                 (unsigned long)bootCount,
                 (unsigned long)getFreeHeap(),
                 (unsigned long)soakBeaconsSent,
                 (unsigned long)soakBeaconsSkipped,
                 (unsigned long)soakCommandsReceived,
                 (unsigned long)soakCommandsFailed,
                 (unsigned long)soakTxErrors,
                 (unsigned long)soakRxErrors,
                 (unsigned long)soakRadioResets,
                 VT, Tc,
                 healthy ? "HEALTHY" : "CHECK");
        logToSD(logEntry);
    }
}
