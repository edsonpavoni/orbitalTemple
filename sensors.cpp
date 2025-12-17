/*
 * Orbital Temple Satellite - Sensors Implementation
 * Version: 1.21
 *
 * CRITICAL FIXES from V1.2:
 *
 * 1. FIXED VARIABLE SHADOWING BUG:
 *    In V1.2, readLumi() and readTemp() declared LOCAL variables
 *    (e.g., "float VM = ...") that shadowed the GLOBAL variables.
 *    This meant sensor readings were stored in local variables that
 *    were discarded, and the globals (used by StatusMsg) stayed at 0.
 *
 *    FIX: Remove type declarations to assign to globals instead.
 *    Before: float VM = analogRead(TL) * 5 / 1024.0;  // LOCAL
 *    After:  VM = analogRead(TL) * 5 / 1024.0;        // GLOBAL
 *
 * 2. Added proper SD card status tracking with SDOK flag
 *
 * 3. Added division-by-zero protection in temperature reading
 */

#include <Arduino.h>
#include <stdint.h>
#include "config.h"

// ==================== IMU INITIALIZATION ====================
void BeginIMU() {
    Wire.begin();

    if (!imu.begin()) {
        Serial.println("[IMU] FAILED to initialize LSM9DS1!");
        IMUOK = false;
    } else {
        Serial.println("[IMU] LSM9DS1 initialized successfully");
        IMUOK = true;
    }
}

// ==================== SD CARD INITIALIZATION ====================
void SDBegin() {
    // Initialize SPI with custom pins
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

    // Attempt to mount SD card
    if (!SD.begin(SD_CS)) {
        Serial.println("[SD] Card Mount FAILED!");
        SDOK = false;
        return;
    }

    // Check card type
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] No SD card attached!");
        SDOK = false;
        return;
    }

    // Success
    SDOK = true;

    // Print card info
    Serial.print("[SD] Card Type: ");
    switch (cardType) {
        case CARD_MMC:  Serial.println("MMC");  break;
        case CARD_SD:   Serial.println("SDSC"); break;
        case CARD_SDHC: Serial.println("SDHC"); break;
        default:        Serial.println("UNKNOWN"); break;
    }

    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("[SD] Card Size: %lluMB\n", cardSize);
    Serial.printf("[SD] Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
    Serial.printf("[SD] Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));
}

// ==================== BATTERY VOLTAGE ====================
void readBatteryVoltage() {
    // Small delay for ADC stability
    delay(10);

    // Read raw ADC value
    VM1 = analogRead(VBAT_DR);

    // Convert to voltage
    // ADC is 12-bit (0-4095), reference is 3.3V
    // Voltage divider factor is 2 (assuming equal resistors)
    // Calibration factor 3.8/3.3 accounts for actual reference
    VE = (VM1 * 3.3f) / 4095.0f;

    // Total battery voltage (accounting for voltage divider)
    VT = VE * 2.0f;

    // Sanity check
    if (VT < 0.0f || VT > 10.0f) {
        Serial.println("[BAT] WARNING: Voltage reading out of range!");
        VT = -1.0f;  // Error indicator
    }
}

// ==================== LUMINOSITY SENSOR ====================
void readLumi() {
    // Small delay for ADC stability
    delay(10);

    // Read raw value (using standard 12-bit resolution)
    // NOTE: ADC resolution is set once in setup, never changed mid-execution
    // to prevent race conditions with interrupts
    int rawValue = analogRead(TL);

    // CRITICAL FIX: Assign to GLOBAL variables, not local ones!
    // In V1.2 these were declared as "float VM = ..." which created
    // LOCAL variables that shadowed the globals. The globals never
    // got updated and StatusMsg() always showed zeros.

    // Voltage from sensor (assuming 5V reference through level shifter)
    // Using 12-bit ADC (4096 levels)
    VM = rawValue * 5.0f / 4096.0f;

    // Percentage of full scale
    VP = rawValue / 4096.0f * 100.0f;

    // Current through sensor (10k load resistor)
    amps = VM / 10000.0f;

    // Convert to microamps
    microamps = amps * 1000000.0f;

    // Convert to lux (sensor-specific calibration factor)
    lux = microamps * 2.0f;

    // Debug output
    Serial.printf("[LUX] Raw: %d, Voltage: %.3fV, Lux: %.1f\n", rawValue, VM, lux);
}

// ==================== TEMPERATURE SENSOR ====================
void readTemp() {
    // Small delay for ADC stability
    delay(10);

    // Read raw ADC value
    float adc = analogRead(ThermistorPin);

    // Sanity check on ADC reading
    if (adc >= 4000.0f) {
        // Sensor likely disconnected or shorted to VCC
        Serial.println("[TEMP] WARNING: ADC reading too high, sensor error!");
        Tc = -999.0;  // Error indicator
        return;
    }

    if (adc <= 50.0f) {
        // Sensor likely shorted to GND
        Serial.println("[TEMP] WARNING: ADC reading too low, sensor error!");
        Tc = -999.0;  // Error indicator
        return;
    }

    // Calculate voltage
    double Vout = adc * Vs / adcMax;

    // CRITICAL FIX: Division by zero protection
    // If Vout equals Vs, the denominator becomes zero
    double denominator = Vs - Vout;
    if (fabs(denominator) < 0.01) {
        Serial.println("[TEMP] WARNING: Division by zero prevented!");
        Tc = -999.0;  // Error indicator
        return;
    }

    // Calculate thermistor resistance
    double Rt = R1z * Vout / denominator;

    // Additional sanity check on resistance
    if (Rt <= 0.0 || Rt > 1000000.0) {
        Serial.println("[TEMP] WARNING: Calculated resistance out of range!");
        Tc = -999.0;  // Error indicator
        return;
    }

    // Steinhart-Hart equation (simplified B-parameter equation)
    // T = 1 / (1/To + (1/Beta) * ln(Rt/Ro))
    double lnRatio = log(Rt / Ro);
    double T = 1.0 / (1.0 / To + lnRatio / Beta);

    // Convert Kelvin to Celsius
    // CRITICAL FIX: Assign to GLOBAL Tc, not a local variable!
    // In V1.2, "double Tc" was declared locally, shadowing the global.
    Tc = T - 273.15;

    // Sanity check on final temperature
    if (Tc < -50.0 || Tc > 150.0) {
        Serial.printf("[TEMP] WARNING: Temperature %.1f C seems unreasonable!\n", Tc);
        // Don't reset to error value, but log the warning
    }

    Serial.printf("[TEMP] ADC: %.0f, Rt: %.0f ohms, Temp: %.1f C\n", adc, Rt, Tc);
}

// ==================== SENSOR STATUS ====================
String getSensorStatus() {
    String status = "";

    status += "IMU:";
    status += IMUOK ? "OK" : "FAIL";
    status += ",SD:";
    status += SDOK ? "OK" : "FAIL";
    status += ",RF:";
    status += RFOK ? "OK" : "FAIL";

    return status;
}
