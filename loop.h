#ifndef LOOP_H
#define LOOP_H

/*
 * Orbital Temple Satellite - Main Loop Module
 * Version: 1.21
 *
 * CHANGELOG from V1.2:
 * - REPLACED blocking delays with non-blocking state machine
 * - ADDED comprehensive input validation
 * - ADDED HMAC message authentication
 * - ADDED telemetry timestamps (mission elapsed time)
 * - ADDED watchdog feeding throughout all operations
 * - FIXED command parsing to handle missing delimiters safely
 */

// Main loop function - called repeatedly from Arduino loop()
void mainLoop();

// Process received message (with authentication)
void processMessage(const String& message);

// Send telemetry status
void sendTelemetry();

// Handle antenna deployment state machine
void handleAntennaDeployment();

// Utility: Get mission elapsed time string
String getMissionTime();

#endif // LOOP_H
