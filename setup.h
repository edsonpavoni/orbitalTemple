#ifndef SETUP_H
#define SETUP_H

/*
 * Orbital Temple Satellite - Setup Module
 * Version: 1.21
 *
 * CHANGELOG from V1.2:
 * - Added watchdog timer initialization
 * - Added EEPROM initialization for state persistence
 * - Added state loading from previous boot
 * - Improved error handling during initialization
 * - No longer hangs on peripheral failures
 */

// Main setup function - called once from Arduino setup()
void setupGeneral();

#endif // SETUP_H
