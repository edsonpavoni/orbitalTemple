#ifndef SENSORS_H
#define SENSORS_H

/*
 * Orbital Temple Satellite - Sensors Module
 * Version: 1.21
 *
 * CHANGELOG from V1.2:
 * - FIXED: Variable shadowing bug where sensor readings never updated globals
 * - Added proper SD card status tracking (SDOK flag)
 * - Added division-by-zero protection in temperature reading
 * - Added sensor health status reporting
 */

// Initialize IMU sensor
// Sets IMUOK = false if initialization fails (no longer hangs)
void BeginIMU();

// Initialize SD card
// Sets SDOK = true on success, false on failure
void SDBegin();

// Read battery voltage
// Updates global: VT (total voltage)
void readBatteryVoltage();

// Read luminosity sensor
// Updates globals: VM, VP, amps, microamps, lux
void readLumi();

// Read temperature from thermistor
// Updates global: Tc (temperature in Celsius)
// Returns -999.0 on sensor error
void readTemp();

// Get sensor health status string
String getSensorStatus();

#endif // SENSORS_H
