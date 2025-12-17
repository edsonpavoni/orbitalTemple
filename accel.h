/*
 * Orbital Temple Satellite - Accelerometer Recording Module
 * Version: 1.21
 *
 * Records accelerometer data (30Hz) for 60 seconds
 * when requested by ground station. Data is stored to SD card
 * and can be downloaded via ReadFile command.
 *
 * First recording is automatically triggered after initial ground contact.
 *
 * DATA FORMAT (Binary):
 *   Header: 16 bytes
 *     - Magic: "ACCEL30" (7 bytes)
 *     - Version: 1 byte
 *     - Sample rate: 2 bytes (uint16_t, Hz)
 *     - Sample count: 2 bytes (uint16_t)
 *     - Reserved: 4 bytes
 *   Data: 12 bytes per sample
 *     - X: 4 bytes (float, in g)
 *     - Y: 4 bytes (float, in g)
 *     - Z: 4 bytes (float, in g)
 *
 * USAGE:
 *   1. Send AccelRecord command to start recording
 *   2. Wait 60 seconds (satellite sends progress updates)
 *   3. Send AccelList to see available recordings
 *   4. Send ReadFile&/accel/[filename] to download
 */

#ifndef ACCEL_H
#define ACCEL_H

#include <Arduino.h>

// Recording configuration
#define ACCEL_SAMPLE_RATE    30      // Hz (reduced from 60 for reliability)
#define ACCEL_DURATION_SEC   60      // seconds
#define ACCEL_TOTAL_SAMPLES  (ACCEL_SAMPLE_RATE * ACCEL_DURATION_SEC)  // 1800

// File header
#define ACCEL_MAGIC          "ACCEL30"
#define ACCEL_VERSION        1
#define ACCEL_HEADER_SIZE    16

// Data structure for one sample
struct AccelSample {
    float x;
    float y;
    float z;
};

// Recording state
typedef enum {
    ACCEL_IDLE,
    ACCEL_RECORDING,
    ACCEL_COMPLETE,
    ACCEL_ERROR
} AccelRecordingState;

// Recording context
struct AccelRecording {
    AccelRecordingState state;
    char filename[64];
    uint16_t samplesRecorded;
    unsigned long startTime;
    unsigned long lastSampleTime;
    unsigned long lastProgressTime;
};

extern AccelRecording accelRecording;

// Flag for first contact auto-recording (persisted to avoid re-triggering)
extern bool firstAccelRecordingDone;

// Initialize accelerometer recording system
void initAccelRecording();

// Check if first recording should be triggered (called on first ground contact)
void checkFirstContactRecording();

// Start a new 60-second recording
// Returns true if recording started
bool accelStartRecording();

// Called from main loop to record samples
// Must be called frequently (at least 30Hz)
void accelRecordingTick();

// Cancel current recording
void accelCancelRecording();

// Get recording status string
String getAccelStatus();

// List available recordings in /accel folder
void accelListRecordings();

#endif // ACCEL_H
