/*
 * Orbital Temple Satellite - Accelerometer Recording Implementation
 * Version: 1.21
 */

#include "accel.h"
#include "config.h"
#include "memor.h"
#include "lora.h"

// Global recording context
AccelRecording accelRecording;

// Flag for first contact auto-recording (prevents re-triggering on reboot)
bool firstAccelRecordingDone = false;

// EEPROM address for first recording flag (after other state data)
#define EEPROM_FIRST_ACCEL_ADDR 200

// Interval between samples in milliseconds (30Hz = ~33.33ms)
#define SAMPLE_INTERVAL_MS   (1000 / ACCEL_SAMPLE_RATE)

// Progress update interval (every 10 seconds)
#define PROGRESS_INTERVAL_MS 10000

// File handle kept open during recording for efficiency
static File accelFile;

void initAccelRecording() {
    accelRecording.state = ACCEL_IDLE;
    accelRecording.filename[0] = '\0';
    accelRecording.samplesRecorded = 0;
    accelRecording.startTime = 0;
    accelRecording.lastSampleTime = 0;
    accelRecording.lastProgressTime = 0;

    // Load first recording flag from EEPROM
    uint8_t flag = EEPROM.read(EEPROM_FIRST_ACCEL_ADDR);
    firstAccelRecordingDone = (flag == 0xAA);  // 0xAA = recording done
    Serial.printf("[ACCEL] First recording flag: %s\n",
                  firstAccelRecordingDone ? "DONE" : "PENDING");

    // Create /accel directory if it doesn't exist
    if (SDOK) {
        if (!SD.exists("/accel")) {
            SD.mkdir("/accel");
            Serial.println("[ACCEL] Created /accel directory");
        }
    }

    Serial.println("[ACCEL] Accelerometer recording system initialized");
}

// Called when first ground contact is established
void checkFirstContactRecording() {
    // Only trigger once per satellite lifetime
    if (firstAccelRecordingDone) {
        Serial.println("[ACCEL] First recording already done, skipping");
        return;
    }

    // Check if we can record (not already recording, SD and IMU available)
    if (accelRecording.state == ACCEL_RECORDING) {
        Serial.println("[ACCEL] Already recording, skipping auto-record");
        return;
    }

    Serial.println("[ACCEL] === FIRST GROUND CONTACT - AUTO RECORDING ===");

    // Start recording
    if (accelStartRecording()) {
        // Mark as done and persist to EEPROM
        firstAccelRecordingDone = true;
        EEPROM.write(EEPROM_FIRST_ACCEL_ADDR, 0xAA);
        EEPROM.commit();
        Serial.println("[ACCEL] First contact recording started and flag persisted");
    } else {
        Serial.println("[ACCEL] Auto-recording failed (will retry on next contact)");
    }
}

bool accelStartRecording() {
    feedWatchdog();

    // Check if already recording
    if (accelRecording.state == ACCEL_RECORDING) {
        Serial.println("[ACCEL] ERROR: Recording already in progress");
        sendMessage("ERR:ACCEL_BUSY");
        return false;
    }

    // Check SD card
    if (!SDOK) {
        Serial.println("[ACCEL] ERROR: SD card not available");
        sendMessage("ERR:SD_NOT_AVAILABLE");
        return false;
    }

    // Check IMU
    if (!IMUOK) {
        Serial.println("[ACCEL] ERROR: IMU not available");
        sendMessage("ERR:IMU_NOT_AVAILABLE");
        return false;
    }

    // Calculate required space: header + samples
    size_t requiredSpace = ACCEL_HEADER_SIZE + (ACCEL_TOTAL_SAMPLES * sizeof(AccelSample));
    if (!hasSDSpace(requiredSpace + 1024)) {
        Serial.println("[ACCEL] ERROR: Not enough SD space");
        sendMessage("ERR:SD_FULL");
        return false;
    }

    // Generate filename with timestamp
    unsigned long timestamp = millis();
    snprintf(accelRecording.filename, sizeof(accelRecording.filename),
             "/accel/rec_%lu.bin", timestamp);

    // Create file and write header
    accelFile = SD.open(accelRecording.filename, FILE_WRITE);
    if (!accelFile) {
        Serial.println("[ACCEL] ERROR: Cannot create file");
        sendMessage("ERR:ACCEL_FILE_ERROR");
        return false;
    }

    // Write header
    uint8_t header[ACCEL_HEADER_SIZE];
    memset(header, 0, ACCEL_HEADER_SIZE);
    memcpy(header, ACCEL_MAGIC, 7);
    header[7] = ACCEL_VERSION;
    uint16_t sampleRate = ACCEL_SAMPLE_RATE;
    uint16_t sampleCount = ACCEL_TOTAL_SAMPLES;
    memcpy(header + 8, &sampleRate, 2);
    memcpy(header + 10, &sampleCount, 2);

    if (accelFile.write(header, ACCEL_HEADER_SIZE) != ACCEL_HEADER_SIZE) {
        Serial.println("[ACCEL] ERROR: Failed to write header");
        accelFile.close();
        SD.remove(accelRecording.filename);
        sendMessage("ERR:ACCEL_WRITE_ERROR");
        return false;
    }

    // Initialize recording state
    accelRecording.state = ACCEL_RECORDING;
    accelRecording.samplesRecorded = 0;
    accelRecording.startTime = millis();
    accelRecording.lastSampleTime = 0;
    accelRecording.lastProgressTime = millis();

    Serial.printf("[ACCEL] Recording started: %s\n", accelRecording.filename);
    Serial.printf("[ACCEL] %d samples @ %d Hz for %d seconds\n",
                  ACCEL_TOTAL_SAMPLES, ACCEL_SAMPLE_RATE, ACCEL_DURATION_SEC);

    sendMessage("OK:ACCEL_RECORDING:" + String(ACCEL_DURATION_SEC) + "s");

    return true;
}

void accelRecordingTick() {
    // Only process if recording
    if (accelRecording.state != ACCEL_RECORDING) {
        return;
    }

    unsigned long now = millis();

    // Check if it's time for a new sample
    if (now - accelRecording.lastSampleTime < SAMPLE_INTERVAL_MS) {
        return;
    }

    // Feed watchdog during long recording
    feedWatchdog();

    // Read accelerometer
    if (imu.accelAvailable()) {
        imu.readAccel();
    }

    // Create sample
    AccelSample sample;
    sample.x = imu.calcAccel(imu.ax);
    sample.y = imu.calcAccel(imu.ay);
    sample.z = imu.calcAccel(imu.az);

    // Write to file
    size_t written = accelFile.write((uint8_t*)&sample, sizeof(AccelSample));
    if (written != sizeof(AccelSample)) {
        Serial.println("[ACCEL] ERROR: Write failed");
        accelFile.close();
        accelRecording.state = ACCEL_ERROR;
        sendMessage("ERR:ACCEL_WRITE_FAILED");
        return;
    }

    accelRecording.samplesRecorded++;
    accelRecording.lastSampleTime = now;

    // Send progress update every 10 seconds
    if (now - accelRecording.lastProgressTime >= PROGRESS_INTERVAL_MS) {
        int percent = (accelRecording.samplesRecorded * 100) / ACCEL_TOTAL_SAMPLES;
        Serial.printf("[ACCEL] Progress: %d/%d (%d%%)\n",
                      accelRecording.samplesRecorded, ACCEL_TOTAL_SAMPLES, percent);
        sendMessage("ACCEL:PROGRESS:" + String(percent) + "%");
        accelRecording.lastProgressTime = now;
    }

    // Check if recording complete
    if (accelRecording.samplesRecorded >= ACCEL_TOTAL_SAMPLES) {
        // Flush and close file
        accelFile.flush();
        accelFile.close();

        accelRecording.state = ACCEL_COMPLETE;

        unsigned long duration = now - accelRecording.startTime;
        size_t fileSize = ACCEL_HEADER_SIZE + (accelRecording.samplesRecorded * sizeof(AccelSample));

        Serial.printf("[ACCEL] Recording complete: %d samples in %lu ms\n",
                      accelRecording.samplesRecorded, duration);
        Serial.printf("[ACCEL] File: %s (%d bytes)\n",
                      accelRecording.filename, fileSize);

        sendMessage("OK:ACCEL_COMPLETE:" + String(accelRecording.filename) +
                    ":" + String(fileSize) + "B");

        // Reset for next recording
        accelRecording.state = ACCEL_IDLE;
    }
}

void accelCancelRecording() {
    if (accelRecording.state == ACCEL_RECORDING) {
        accelFile.close();
        SD.remove(accelRecording.filename);
        Serial.println("[ACCEL] Recording cancelled");
        sendMessage("OK:ACCEL_CANCELLED");
    }
    accelRecording.state = ACCEL_IDLE;
}

String getAccelStatus() {
    String status = "ACCEL:";

    switch (accelRecording.state) {
        case ACCEL_IDLE:
            status += "IDLE";
            break;
        case ACCEL_RECORDING:
            {
                int percent = (accelRecording.samplesRecorded * 100) / ACCEL_TOTAL_SAMPLES;
                status += "REC:" + String(percent) + "%";
            }
            break;
        case ACCEL_COMPLETE:
            status += "COMPLETE";
            break;
        case ACCEL_ERROR:
            status += "ERROR";
            break;
    }

    return status;
}

void accelListRecordings() {
    if (!SDOK) {
        sendMessage("ERR:SD_NOT_AVAILABLE");
        return;
    }

    File dir = SD.open("/accel");
    if (!dir || !dir.isDirectory()) {
        sendMessage("ACCEL:NO_RECORDINGS");
        return;
    }

    sendMessage("ACCEL:RECORDINGS");
    delay(50);

    int count = 0;
    File file = dir.openNextFile();
    while (file && count < 20) {  // Limit to 20 files
        if (!file.isDirectory()) {
            String info = "ACCEL:F:" + String(file.name()) + "," + String(file.size());
            sendMessage(info);
            delay(50);
            count++;
        }
        file = dir.openNextFile();
    }

    dir.close();
    sendMessage("ACCEL:END:" + String(count));
}
