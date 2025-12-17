/*
 * Orbital Temple Satellite - Main Loop Implementation
 * Version: 1.21
 *
 * CRITICAL FIXES from V1.2:
 *
 * 1. NON-BLOCKING STATE MACHINE:
 *    V1.2 used blocking delays (delay(300000) for 5 minutes, etc.)
 *    making the satellite completely unresponsive during these periods.
 *
 *    FIX: Implemented state machine using millis() for timing.
 *    Satellite can receive commands at ANY time.
 *
 * 2. INPUT VALIDATION:
 *    V1.2 had no validation on received messages. Missing delimiters
 *    caused indexOf() to return -1, leading to undefined behavior
 *    in substring() operations.
 *
 *    FIX: Validate all delimiters present before parsing.
 *    Validate path strings for directory traversal attacks.
 *    Limit string lengths.
 *
 * 3. HMAC AUTHENTICATION:
 *    V1.2 only checked plaintext satellite ID - anyone who knew the
 *    ID could send commands.
 *
 *    FIX: All commands must include HMAC signature.
 *    Message format: SAT_ID-COMMAND&PATH@DATA#HMAC
 *
 * 4. TELEMETRY TIMESTAMPS:
 *    V1.2 sent sensor data without timestamps.
 *
 *    FIX: All telemetry includes mission elapsed time.
 */

#include <Arduino.h>
#include "config.h"
#include "loop.h"
#include "lora.h"
#include "sensors.h"
#include "memor.h"
#include "radiation.h"
#include "image.h"

// ==================== LOCAL VARIABLES ====================
static unsigned long lastTelemetryTime = 0;
static String receivedData = "";

// ==================== MISSION TIME ====================
String getMissionTime() {
    unsigned long elapsed = millis() - missionStartTime;

    // Convert to hours:minutes:seconds
    unsigned long seconds = elapsed / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;

    char buffer[20];
    snprintf(buffer, sizeof(buffer), "T+%02lu:%02lu:%02lu", hours, minutes, seconds);
    return String(buffer);
}

// ==================== TELEMETRY ====================
void sendTelemetry() {
    feedWatchdog();

    // Read all sensors
    readBatteryVoltage();
    readLumi();
    readTemp();

    // Read IMU if available
    if (IMUOK) {
        if (imu.gyroAvailable()) imu.readGyro();
        if (imu.accelAvailable()) imu.readAccel();
        if (imu.magAvailable()) imu.readMag();
    }

    // Build telemetry message with timestamp
    // Format: TIME|SENSORS|BAT|TEMP|LUX|IMU
    String telemetry = getMissionTime();
    telemetry += "|";
    telemetry += getSensorStatus();
    telemetry += "|BAT:";
    telemetry += String(VT, 2);
    telemetry += "V|TEMP:";
    telemetry += String(Tc, 1);
    telemetry += "C|LUX:";
    telemetry += String(lux, 1);

    // Add IMU data if available
    if (IMUOK) {
        telemetry += "|GYR:";
        telemetry += String(imu.calcGyro(imu.gx), 1) + ",";
        telemetry += String(imu.calcGyro(imu.gy), 1) + ",";
        telemetry += String(imu.calcGyro(imu.gz), 1);
        telemetry += "|ACC:";
        telemetry += String(imu.calcAccel(imu.ax), 2) + ",";
        telemetry += String(imu.calcAccel(imu.ay), 2) + ",";
        telemetry += String(imu.calcAccel(imu.az), 2);
        telemetry += "|MAG:";
        telemetry += String(imu.calcMag(imu.mx), 1) + ",";
        telemetry += String(imu.calcMag(imu.my), 1) + ",";
        telemetry += String(imu.calcMag(imu.mz), 1);
    }

    // Add SD card capacity (Gemini review recommendation)
    if (SDOK) {
        telemetry += "|SD:";
        telemetry += String(getSDFreePercent());
        telemetry += "%";
    }

    // Add radiation protection status (SEU corrections)
    telemetry += "|SEU:";
    telemetry += String(seuCorrectionsTotal);

    Serial.println("[TELEM] " + telemetry);
    sendMessage(telemetry);

    // Log to SD card
    logToSD(telemetry.c_str());
}

// ==================== INPUT VALIDATION ====================
bool validateMessage(const String& msg, String& satId, String& command,
                     String& path, String& data, String& hmac) {
    // Message format: SAT_ID-COMMAND&PATH@DATA#HMAC
    // Minimum valid message: "X-Y&@#Z" = 7 characters

    if (msg.length() < 7) {
        Serial.println("[PARSE] Message too short");
        return false;
    }

    if (msg.length() > 500) {
        Serial.println("[PARSE] Message too long");
        return false;
    }

    // Find all delimiters
    int dashIdx = msg.indexOf('-');
    int ampIdx = msg.indexOf('&');
    int atIdx = msg.indexOf('@');
    int hashIdx = msg.indexOf('#');

    // Validate all delimiters present and in correct order
    if (dashIdx == -1 || ampIdx == -1 || atIdx == -1 || hashIdx == -1) {
        Serial.println("[PARSE] Missing delimiter(s)");
        return false;
    }

    if (!(dashIdx < ampIdx && ampIdx < atIdx && atIdx < hashIdx)) {
        Serial.println("[PARSE] Delimiters in wrong order");
        return false;
    }

    // Extract parts
    satId = msg.substring(0, dashIdx);
    command = msg.substring(dashIdx + 1, ampIdx);
    path = msg.substring(ampIdx + 1, atIdx);
    data = msg.substring(atIdx + 1, hashIdx);
    hmac = msg.substring(hashIdx + 1);

    // Validate satellite ID
    if (!satId.equals(sat_id)) {
        Serial.println("[PARSE] Wrong satellite ID");
        return false;
    }

    // Validate command is alphanumeric
    for (unsigned int i = 0; i < command.length(); i++) {
        char c = command.charAt(i);
        if (!isalnum(c)) {
            Serial.println("[PARSE] Invalid command characters");
            return false;
        }
    }

    // Validate path (no directory traversal)
    if (path.indexOf("..") != -1) {
        Serial.println("[PARSE] Path traversal blocked!");
        sendMessage("ERR:PATH_TRAVERSAL_BLOCKED");
        return false;
    }

    // Verify HMAC
    String messageToVerify = msg.substring(0, hashIdx);
    if (!verifyHMAC(messageToVerify, hmac)) {
        Serial.println("[AUTH] HMAC verification failed!");
        sendMessage("ERR:AUTH_FAILED");
        return false;
    }

    return true;
}

// ==================== COMMAND PROCESSING ====================
void processMessage(const String& message) {
    feedWatchdog();

    Serial.println("[MSG] Processing: " + message);

    String satId, command, path, data, hmac;

    // Validate and parse message
    if (!validateMessage(message, satId, command, path, data, hmac)) {
        Serial.println("[MSG] Invalid message, ignoring");
        return;
    }

    Serial.println("[MSG] Valid message received");
    Serial.println("[MSG] Command: " + command);
    Serial.println("[MSG] Path: " + path);
    Serial.println("[MSG] Data: " + data);

    // Register ground contact (for beacon timing)
    registerGroundContact();

    // Convert path and data to C strings for SD operations
    const char* pathCStr = path.c_str();
    const char* dataCStr = data.c_str();

    // Process command
    if (command.equals("Status")) {
        Serial.println("[CMD] Status request");
        sendTelemetry();
    }
    else if (command.equals("Ping")) {
        Serial.println("[CMD] Ping");
        sendMessage("PONG|" + getMissionTime());
    }
    else if (command.equals("ListDir")) {
        Serial.println("[CMD] List directory");
        listDir(SD, pathCStr, 0);
    }
    else if (command.equals("CreateDir")) {
        Serial.println("[CMD] Create directory");
        createDir(SD, pathCStr);
    }
    else if (command.equals("RemoveDir")) {
        Serial.println("[CMD] Remove directory");
        removeDir(SD, pathCStr);
    }
    else if (command.equals("WriteFile")) {
        Serial.println("[CMD] Write file");
        writeFile(SD, pathCStr, dataCStr);
    }
    else if (command.equals("AppendFile")) {
        Serial.println("[CMD] Append to file");
        appendFile(SD, pathCStr, dataCStr);
    }
    else if (command.equals("ReadFile")) {
        Serial.println("[CMD] Read file");
        readFile(SD, pathCStr);
    }
    else if (command.equals("RenameFile")) {
        Serial.println("[CMD] Rename file");
        renameFile(SD, pathCStr, dataCStr);
    }
    else if (command.equals("DeleteFile")) {
        Serial.println("[CMD] Delete file");
        deleteFile(SD, pathCStr);
    }
    else if (command.equals("TestFileIO")) {
        Serial.println("[CMD] Test file I/O");
        testFileIO(SD, pathCStr);
    }
    else if (command.equals("MCURestart")) {
        Serial.println("[CMD] MCU restart requested");
        sendMessage("OK:RESTARTING");
        delay(500);
        saveState();
        ESP.restart();
    }
    else if (command.equals("GetState")) {
        Serial.println("[CMD] Get state");
        String stateMsg = "STATE:" + String((int)currentState);
        stateMsg += "|BOOTS:" + String(bootCount);
        stateMsg += "|ANT:" + String(antennaDeployed ? "DEPLOYED" : "PENDING");
        sendMessage(stateMsg);
    }
    else if (command.equals("ForceOperational")) {
        // Emergency command to skip antenna deployment
        Serial.println("[CMD] Force operational mode");
        antennaDeployed = true;
        currentState = STATE_OPERATIONAL;
        saveState();
        sendMessage("OK:FORCED_OPERATIONAL");
    }
    else if (command.equals("GetRadStatus")) {
        // Get radiation protection status
        Serial.println("[CMD] Get radiation status");
        String radMsg = "RAD:SEU_TOTAL:";
        radMsg += String(seuCorrectionsTotal);
        radMsg += "|LAST_SCRUB:";
        radMsg += String((millis() - lastScrubTime) / 1000);
        radMsg += "s_ago";
        sendMessage(radMsg);
    }
    // ==================== IMAGE TRANSFER COMMANDS ====================
    else if (command.equals("ImageStart")) {
        // Start image transfer
        // Path: filename, Data: totalChunks:expectedSize
        Serial.println("[CMD] Image start");
        if (path.length() == 0) {
            sendMessage("ERR:IMG_NO_FILENAME");
        } else {
            int colonIdx = data.indexOf(':');
            if (colonIdx == -1) {
                sendMessage("ERR:IMG_INVALID_PARAMS");
            } else {
                uint16_t totalChunks = data.substring(0, colonIdx).toInt();
                uint16_t expectedSize = data.substring(colonIdx + 1).toInt();
                imageStart(pathCStr, totalChunks, expectedSize);
            }
        }
    }
    else if (command.equals("ImageChunk")) {
        // Receive image chunk
        // Path: chunk number, Data: base64 encoded data
        Serial.println("[CMD] Image chunk");
        if (data.length() == 0) {
            sendMessage("ERR:IMG_EMPTY_CHUNK");
        } else {
            uint16_t chunkNum = path.toInt();
            imageChunk(chunkNum, dataCStr);
        }
    }
    else if (command.equals("ImageEnd")) {
        // Finalize image transfer
        Serial.println("[CMD] Image end");
        imageEnd();
    }
    else if (command.equals("ImageCancel")) {
        // Cancel current transfer
        Serial.println("[CMD] Image cancel");
        imageCancel();
    }
    else if (command.equals("ImageStatus")) {
        // Get image transfer status
        Serial.println("[CMD] Image status");
        sendMessage(getImageStatus());
    }
    else {
        Serial.println("[CMD] Unknown command: " + command);
        sendMessage("ERR:UNKNOWN_CMD:" + command);
    }
}

// ==================== ANTENNA DEPLOYMENT STATE MACHINE ====================
void handleAntennaDeployment() {
    unsigned long now = millis();
    unsigned long elapsed = now - stateStartTime;

    switch (antennaState) {
        case ANT_IDLE:
            // Check switch state
            if (digitalRead(AntSwitch) == HIGH) {
                // Switch pressed, start heating
                Serial.println("[ANT] Switch pressed, starting burn wire heating");
                digitalWrite(R1, HIGH);
                antennaState = ANT_HEATING;
                stateStartTime = now;
            } else {
                // Switch released - antenna deployed!
                Serial.println("[ANT] Switch released - antenna deployed!");
                digitalWrite(R1, LOW);
                antennaDeployed = true;
                antennaState = ANT_COMPLETE;
                currentState = STATE_OPERATIONAL;
                saveState();
                sendMessage("OK:ANTENNA_DEPLOYED|" + getMissionTime());
            }
            break;

        case ANT_HEATING:
            feedWatchdog();

            if (elapsed >= DEPLOY_HEAT_TIME) {
                // Done heating, start cooling
                Serial.println("[ANT] Heating complete, cooling down");
                digitalWrite(R1, LOW);
                antennaState = ANT_COOLING;
                stateStartTime = now;
            }

            // Check if switch released during heating
            if (digitalRead(AntSwitch) == LOW) {
                Serial.println("[ANT] Switch released during heating - success!");
                digitalWrite(R1, LOW);
                antennaDeployed = true;
                antennaState = ANT_COMPLETE;
                currentState = STATE_OPERATIONAL;
                saveState();
                sendMessage("OK:ANTENNA_DEPLOYED|" + getMissionTime());
            }
            break;

        case ANT_COOLING:
            feedWatchdog();

            if (elapsed >= DEPLOY_COOL_TIME) {
                // Check if deployment successful
                if (digitalRead(AntSwitch) == LOW) {
                    Serial.println("[ANT] Deployment successful after cooling");
                    antennaDeployed = true;
                    antennaState = ANT_COMPLETE;
                    currentState = STATE_OPERATIONAL;
                    saveState();
                    sendMessage("OK:ANTENNA_DEPLOYED|" + getMissionTime());
                } else {
                    // Still not deployed, need to retry
                    deployRetryCount++;
                    Serial.printf("[ANT] Deployment attempt %d failed\n", deployRetryCount);

                    if (deployRetryCount >= DEPLOY_MAX_RETRIES) {
                        Serial.println("[ANT] Max retries reached!");
                        sendMessage("ERR:ANT_DEPLOY_FAILED|" + getMissionTime());
                        // Continue to operational anyway - we tried our best
                        currentState = STATE_OPERATIONAL;
                        saveState();
                    } else {
                        // Wait before retry
                        antennaState = ANT_RETRY_WAIT;
                        stateStartTime = now;
                        sendMessage("WARN:ANT_RETRY_WAIT|" + getMissionTime());
                    }
                }
            }
            break;

        case ANT_RETRY_WAIT:
            feedWatchdog();

            if (elapsed >= DEPLOY_RETRY_WAIT) {
                Serial.println("[ANT] Retry wait complete, attempting again");
                antennaState = ANT_IDLE;
                stateStartTime = now;
            }

            // Check if switch released during wait
            if (digitalRead(AntSwitch) == LOW) {
                Serial.println("[ANT] Switch released during wait - success!");
                antennaDeployed = true;
                antennaState = ANT_COMPLETE;
                currentState = STATE_OPERATIONAL;
                saveState();
                sendMessage("OK:ANTENNA_DEPLOYED|" + getMissionTime());
            }
            break;

        case ANT_COMPLETE:
            // Nothing to do, deployment complete
            break;
    }
}

// ==================== MAIN LOOP ====================
void mainLoop() {
    unsigned long now = millis();

    // Feed watchdog regularly
    if (now - lastWdtFeed >= WDT_FEED_INTERVAL) {
        feedWatchdog();
    }

    // Radiation protection - scrub TMR variables periodically
    radiationProtectionTick();

    // State machine
    switch (currentState) {
        case STATE_BOOT:
            // Initial state after power-on
            Serial.println("[STATE] Boot complete, waiting before deployment");
            currentState = STATE_WAIT_DEPLOY;
            stateStartTime = now;
            break;

        case STATE_WAIT_DEPLOY:
            // Non-blocking wait before antenna deployment
            // Still check for incoming messages during this time!
            if (now - stateStartTime >= DEPLOY_WAIT_TIME) {
                Serial.println("[STATE] Wait complete, starting deployment");
                currentState = STATE_DEPLOYING;
                antennaState = ANT_IDLE;
                stateStartTime = now;
            }

            // Send beacon during wait phase (helps ground station find us)
            {
                unsigned long beaconInterval = getBeaconInterval();
                if (now - lastBeaconTime >= beaconInterval) {
                    sendBeacon();
                }
            }

            // Check for incoming messages (allows abort command)
            if (receivedFlag) {
                receivedFlag = false;
                int state = radio.readData(receivedData);
                if (state == RADIOLIB_ERR_NONE) {
                    Serial.println("[LORA] Received during wait: " + receivedData);
                    processMessage(receivedData);
                }
            }
            break;

        case STATE_DEPLOYING:
            // Handle antenna deployment state machine
            handleAntennaDeployment();

            // Still check for incoming messages
            if (receivedFlag) {
                receivedFlag = false;
                int state = radio.readData(receivedData);
                if (state == RADIOLIB_ERR_NONE) {
                    processMessage(receivedData);
                }
            }
            break;

        case STATE_OPERATIONAL:
            // Normal operation - send first beacon if just entered
            if (stateStartTime == 0) {
                Serial.println("[STATE] Entering operational mode");
                // Send initial beacon
                sendBeacon();
                stateStartTime = now;
                lastTelemetryTime = now;
                lastBeaconTime = now;
            }

            // ==================== ADAPTIVE BEACON ====================
            // Beacon interval depends on ground contact status:
            // - Before first contact: every 1 minute (frequent)
            // - After contact established: every 1 hour (normal)
            // - After 24h without contact: every 5 minutes (lost mode)
            {
                unsigned long beaconInterval = getBeaconInterval();
                if (now - lastBeaconTime >= beaconInterval) {
                    sendBeacon();
                    // lastBeaconTime is updated inside sendBeacon()
                }
            }

            // Periodic telemetry (in addition to beacon)
            if (now - lastTelemetryTime >= STATUS_INTERVAL) {
                sendTelemetry();
                lastTelemetryTime = now;
            }

            // Check for image transfer timeout
            imageTimeoutCheck();

            // Check for radio recovery
            if (radioNeedsRecovery()) {
                Serial.println("[STATE] Radio needs recovery");
                if (!recoverRadio()) {
                    Serial.println("[STATE] Radio recovery failed, restarting...");
                    saveState();
                    ESP.restart();
                }
            }

            // Process incoming messages
            if (receivedFlag) {
                receivedFlag = false;
                int state = radio.readData(receivedData);
                if (state == RADIOLIB_ERR_NONE) {
                    Serial.println("[LORA] Received: " + receivedData);
                    processMessage(receivedData);
                } else {
                    Serial.printf("[LORA] Read error: %d\n", state);
                }
            }
            break;

        case STATE_ERROR:
            // Error state - try to recover (non-blocking)
            {
                static unsigned long lastRecoveryAttempt = 0;
                const unsigned long RECOVERY_INTERVAL = 5000;

                if (now - lastRecoveryAttempt >= RECOVERY_INTERVAL) {
                    Serial.println("[STATE] Error state, attempting recovery");
                    feedWatchdog();

                    if (recoverRadio()) {
                        currentState = STATE_OPERATIONAL;
                        stateStartTime = 0;
                    }
                    lastRecoveryAttempt = now;
                }
            }
            break;
    }
}
