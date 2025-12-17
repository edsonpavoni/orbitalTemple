#ifndef CONFIG_H
#define CONFIG_H

/*
 * Orbital Temple Satellite - Configuration Header
 * Version: 1.21
 *
 * CHANGELOG from V1.2:
 * - Added watchdog timer configuration
 * - Added state machine enums for non-blocking operation
 * - Added HMAC authentication key
 * - Centralized LoRa configuration (sync word consistency)
 * - Added SD card status tracking
 * - Added mission elapsed time tracking
 */

#include <Arduino.h>
#include <stdint.h>

// General
#include <SPI.h>
#include <Wire.h>

// RF - LoRa
#include <ArduinoJson.h>
#include <RadioLib.h>

// Power saving
#include <WiFi.h>

// SD Card
#include "FS.h"
#include "SD.h"

// IMU Sensor
#include <SparkFunLSM9DS1.h>

// Watchdog
#include <esp_task_wdt.h>

// EEPROM for state persistence
#include <EEPROM.h>

// HMAC for authentication
#include "mbedtls/md.h"

// ==================== PIN DEFINITIONS ====================

// Luminosity sensor
#define TL 26

// Temperature (Thermistor)
#define ThermistorPin 34

// Battery voltage divider
#define VBAT_DR 35

// Antenna deployment
#define AntSwitch 33
#define R1 27

// RF Module (SX1262)
#define CS_RF 5
#define RST_RF 14
#define DIO1_RF 2

// ==================== RADIO CONFIGURATION ====================
// Centralized to ensure consistency across all functions

#define LORA_FREQ_RX      401.5f    // Receive frequency (MHz)
#define LORA_FREQ_TX      468.5f    // Transmit frequency (MHz)
#define LORA_BW           125.0f    // Bandwidth (kHz)
#define LORA_SF           9         // Spreading factor
#define LORA_CR           7         // Coding rate
#define LORA_SYNC_WORD    0x12      // Sync word - MUST BE SAME FOR RX AND TX
#define LORA_PREAMBLE     8         // Preamble length

// ==================== WATCHDOG CONFIGURATION ====================

#define WDT_TIMEOUT_SECONDS  60     // Watchdog timeout in seconds
#define WDT_PANIC_ON_TIMEOUT true   // Reset on timeout

// ==================== STATE MACHINE ====================

// Mission states - non-blocking state machine
typedef enum {
    STATE_BOOT,                  // Initial boot
    STATE_WAIT_DEPLOY,           // Waiting before antenna deployment
    STATE_DEPLOYING,             // Antenna deployment in progress
    STATE_DEPLOY_COOLING,        // Cooling period after deployment attempt
    STATE_OPERATIONAL,           // Normal operation - listening for commands
    STATE_TRANSMITTING,          // Currently transmitting
    STATE_ERROR                  // Error state
} MissionState;

// Antenna deployment sub-states
typedef enum {
    ANT_IDLE,
    ANT_HEATING,
    ANT_COOLING,
    ANT_RETRY_WAIT,
    ANT_COMPLETE
} AntennaState;

// ==================== SECURITY CONFIGURATION ====================

// HMAC-SHA256 key for message authentication
// IMPORTANT: Change this key before flight!
// This should be a 32-byte (256-bit) key
#define HMAC_KEY_LENGTH 32
extern const uint8_t HMAC_KEY[HMAC_KEY_LENGTH];

// Message format: SAT_ID-COMMAND&PATH@DATA#HMAC
// HMAC is calculated over: SAT_ID-COMMAND&PATH@DATA

// ==================== TIMING CONFIGURATION ====================

// All times in milliseconds
#define DEPLOY_WAIT_TIME       300000UL   // 5 minutes before deployment (300000ms)
#define DEPLOY_HEAT_TIME       90000UL    // 1.5 minutes heating (90000ms)
#define DEPLOY_COOL_TIME       90000UL    // 1.5 minutes cooling (90000ms)
#define DEPLOY_RETRY_WAIT      900000UL   // 15 minutes between retries (900000ms)
#define DEPLOY_MAX_RETRIES     3          // Maximum deployment attempts

#define STATUS_INTERVAL        60000UL    // Send status every 60 seconds in operational mode
#define WDT_FEED_INTERVAL      10000UL    // Feed watchdog every 10 seconds

// ==================== BEACON CONFIGURATION ====================
// Adaptive beacon timing based on ground station contact status
//
// Before first contact:    BEACON_INTERVAL_NO_CONTACT (1 minute)
// After contact established: BEACON_INTERVAL_NORMAL (1 hour)
// After 24h without contact: BEACON_INTERVAL_LOST (5 minutes)
//
// EASY TO CONFIGURE: Adjust these values as needed

#define BEACON_INTERVAL_NO_CONTACT   60000UL      // 1 minute (60,000 ms) - before first contact
#define BEACON_INTERVAL_NORMAL       3600000UL    // 1 hour (3,600,000 ms) - after contact established
#define BEACON_INTERVAL_LOST         300000UL     // 5 minutes (300,000 ms) - no contact for 24h
#define BEACON_LOST_THRESHOLD        86400000UL   // 24 hours (86,400,000 ms) - time to consider "lost"

// Beacon message content
#define BEACON_MESSAGE "BEACON:ORBITAL_TEMPLE"

// ==================== EEPROM CONFIGURATION ====================

#define EEPROM_SIZE            512
#define EEPROM_MAGIC           0xAB       // Magic byte to verify valid data
#define EEPROM_ADDR_MAGIC      0
#define EEPROM_ADDR_STATE      1
#define EEPROM_ADDR_BOOTCOUNT  2          // 4 bytes (uint32_t)
#define EEPROM_ADDR_DEPLOY_OK  6          // 1 byte (bool)
#define EEPROM_ADDR_MISSION_START 7       // 4 bytes (uint32_t) - millis at first boot

// ==================== SD CARD CONFIGURATION ====================

#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23
#define SD_CS   17

// ==================== EXTERNAL VARIABLES ====================

// --- Satellite ID ---
extern String sat_id;

// --- LoRa Radio ---
extern SX1262 radio;
extern volatile bool receivedFlag;
extern String MsR;

// --- State Machine ---
extern MissionState currentState;
extern AntennaState antennaState;
extern unsigned long stateStartTime;
extern unsigned long lastWdtFeed;
extern unsigned long missionStartTime;
extern uint32_t bootCount;

// --- Antenna Deployment ---
extern int deployRetryCount;
extern bool antennaDeployed;

// --- Beacon System ---
extern bool groundContactEstablished;     // True after first successful command received
extern unsigned long lastGroundContact;    // Time of last command from ground station
extern unsigned long lastBeaconTime;       // Time of last beacon transmission

// --- Hardware Status Flags ---
extern bool IMUOK;
extern bool RFOK;
extern bool SDOK;

// --- LoRa retry counters ---
extern int contE;  // Transmit retry counter
extern int contR;  // Receive retry counter

// --- Sensors: Battery ---
extern int VM1;
extern float VE;
extern float VT;

// --- Sensors: Temperature ---
extern double adcMax;
extern double Vs;
extern double R1z;
extern double Beta;
extern double To;
extern double Ro;
extern double Tc;

// --- Sensors: Luminosity ---
extern float VM;
extern float VP;
extern float amps;
extern float microamps;
extern float lux;

// --- Sensors: IMU ---
extern LSM9DS1 imu;

// ==================== FUNCTION DECLARATIONS ====================

// Watchdog
void feedWatchdog();

// State persistence
void saveState();
void loadState();

// Authentication
bool verifyHMAC(const String& message, const String& receivedHMAC);
String calculateHMAC(const String& message);

// Beacon system
void sendBeacon();
unsigned long getBeaconInterval();
void registerGroundContact();

#endif // CONFIG_H
