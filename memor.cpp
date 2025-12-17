/*
 * Orbital Temple Satellite - Memory/SD Card Implementation
 * Version: 1.21
 *
 * CRITICAL FIXES from V1.2:
 *
 * 1. FIXED MEMORY EXHAUSTION IN listDir():
 *    In V1.2, listDir() used unbounded string concatenation:
 *      while (file) {
 *        msgld += "DIR: " + String(file.name());  // Keeps growing!
 *        sendMensage(msgld);  // Sends larger and larger strings
 *      }
 *    This could exhaust heap memory with many files.
 *
 *    FIX: Send each file entry separately, don't accumulate.
 *
 * 2. Added SD card availability check (SDOK) before operations
 *
 * 3. Added proper file handle closing in all error paths
 */

#include <Arduino.h>
#include "config.h"
#include "memor.h"
#include "lora.h"

// Maximum chunk size for LoRa transmission
#define LORA_CHUNK_SIZE 200

// ==================== SD AVAILABILITY CHECK ====================
bool isSDAvailable() {
    if (!SDOK) {
        Serial.println("[SD] ERROR: SD card not available!");
        sendMessage("ERR:SD_NOT_AVAILABLE");
        return false;
    }
    return true;
}

// ==================== LIST DIRECTORY ====================
void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    if (!isSDAvailable()) return;

    // Feed watchdog - directory listing can take time
    feedWatchdog();

    Serial.printf("[SD] Listing directory: %s\n", dirname);

    File root = fs.open(dirname);
    if (!root) {
        Serial.println("[SD] Failed to open directory");
        sendMessage("ERR:OPEN_DIR_FAILED");
        return;
    }

    if (!root.isDirectory()) {
        Serial.println("[SD] Not a directory");
        sendMessage("ERR:NOT_A_DIRECTORY");
        root.close();
        return;
    }

    // Send header
    sendMessage("DIR:" + String(dirname));
    delay(100);

    File file = root.openNextFile();
    int fileCount = 0;
    const int MAX_FILES = 100;  // Limit to prevent infinite loops

    while (file && fileCount < MAX_FILES) {
        // Feed watchdog during long operations
        feedWatchdog();

        // FIXED: Send each entry separately instead of accumulating
        // This prevents unbounded memory growth
        String entry = "";

        if (file.isDirectory()) {
            entry = "D:" + String(file.name());
            Serial.print("[SD]   DIR: ");
            Serial.println(file.name());

            // Recurse if requested (with limit)
            if (levels > 0) {
                listDir(fs, file.path(), levels - 1);
            }
        } else {
            entry = "F:" + String(file.name()) + "," + String(file.size());
            Serial.printf("[SD]   FILE: %s  SIZE: %d\n", file.name(), file.size());
        }

        // Send this entry
        sendMessage(entry);
        delay(50);  // Small delay between transmissions

        file = root.openNextFile();
        fileCount++;
    }

    // Send end marker
    sendMessage("END:DIR");

    root.close();
    Serial.printf("[SD] Listed %d items\n", fileCount);
}

// ==================== CREATE DIRECTORY ====================
void createDir(fs::FS &fs, const char *path) {
    if (!isSDAvailable()) return;

    Serial.printf("[SD] Creating directory: %s\n", path);

    if (fs.mkdir(path)) {
        Serial.println("[SD] Directory created");
        sendMessage("OK:DIR_CREATED:" + String(path));
    } else {
        Serial.println("[SD] mkdir failed");
        sendMessage("ERR:MKDIR_FAILED");
    }
}

// ==================== REMOVE DIRECTORY ====================
void removeDir(fs::FS &fs, const char *path) {
    if (!isSDAvailable()) return;

    Serial.printf("[SD] Removing directory: %s\n", path);

    if (fs.rmdir(path)) {
        Serial.println("[SD] Directory removed");
        sendMessage("OK:DIR_REMOVED");
    } else {
        Serial.println("[SD] rmdir failed");
        sendMessage("ERR:RMDIR_FAILED");
    }
}

// ==================== READ FILE ====================
void readFile(fs::FS &fs, const char *path) {
    if (!isSDAvailable()) return;

    feedWatchdog();

    Serial.printf("[SD] Reading file: %s\n", path);

    File file = fs.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        Serial.println("[SD] Failed to open file for reading");
        sendMessage("ERR:OPEN_FILE_FAILED");
        if (file) file.close();
        return;
    }

    // Send header with file size
    size_t fileSize = file.size();
    sendMessage("FILE:" + String(path) + "," + String(fileSize));
    delay(100);

    // Read and send in chunks
    char buffer[LORA_CHUNK_SIZE + 1];
    size_t totalSent = 0;
    int chunkNum = 0;

    while (file.available() && totalSent < fileSize) {
        feedWatchdog();

        size_t bytesRead = file.readBytes(buffer, LORA_CHUNK_SIZE);
        buffer[bytesRead] = '\0';

        sendMessage(String(buffer));
        delay(50);

        totalSent += bytesRead;
        chunkNum++;

        Serial.printf("[SD] Sent chunk %d, %d/%d bytes\n", chunkNum, totalSent, fileSize);
    }

    sendMessage("END:FILE");
    file.close();

    Serial.printf("[SD] File read complete, %d bytes in %d chunks\n", totalSent, chunkNum);
}

// ==================== WRITE FILE ====================
void writeFile(fs::FS &fs, const char *path, const char *message) {
    if (!isSDAvailable()) return;

    feedWatchdog();

    // Check available space
    size_t msgLen = strlen(message);
    if (!hasSDSpace(msgLen)) {
        Serial.println("[SD] ERROR: Not enough space!");
        sendMessage("ERR:SD_FULL");
        return;
    }

    Serial.printf("[SD] Writing file: %s\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("[SD] Failed to open file for writing");
        sendMessage("ERR:OPEN_FILE_FAILED");
        return;
    }

    size_t bytesWritten = file.print(message);
    file.close();

    if (bytesWritten > 0) {
        Serial.printf("[SD] File written, %d bytes\n", bytesWritten);
        sendMessage("OK:WRITTEN:" + String(bytesWritten) + "B");
    } else {
        Serial.println("[SD] Write failed");
        sendMessage("ERR:WRITE_FAILED");
    }
}

// ==================== APPEND FILE ====================
void appendFile(fs::FS &fs, const char *path, const char *message) {
    if (!isSDAvailable()) return;

    feedWatchdog();

    // Check available space
    size_t msgLen = strlen(message);
    if (!hasSDSpace(msgLen)) {
        Serial.println("[SD] ERROR: Not enough space!");
        sendMessage("ERR:SD_FULL");
        return;
    }

    Serial.printf("[SD] Appending to file: %s\n", path);

    File file = fs.open(path, FILE_APPEND);
    if (!file) {
        Serial.println("[SD] Failed to open file for appending");
        sendMessage("ERR:OPEN_FILE_FAILED");
        return;
    }

    size_t bytesWritten = file.print(message);
    file.close();

    if (bytesWritten > 0) {
        Serial.printf("[SD] Appended %d bytes\n", bytesWritten);
        sendMessage("OK:APPENDED:" + String(bytesWritten) + "B");
    } else {
        Serial.println("[SD] Append failed");
        sendMessage("ERR:APPEND_FAILED");
    }
}

// ==================== RENAME FILE ====================
void renameFile(fs::FS &fs, const char *path1, const char *path2) {
    if (!isSDAvailable()) return;

    Serial.printf("[SD] Renaming file %s to %s\n", path1, path2);

    if (fs.rename(path1, path2)) {
        Serial.println("[SD] File renamed");
        sendMessage("OK:RENAMED");
    } else {
        Serial.println("[SD] Rename failed");
        sendMessage("ERR:RENAME_FAILED");
    }
}

// ==================== DELETE FILE ====================
void deleteFile(fs::FS &fs, const char *path) {
    if (!isSDAvailable()) return;

    Serial.printf("[SD] Deleting file: %s\n", path);

    if (fs.remove(path)) {
        Serial.println("[SD] File deleted");
        sendMessage("OK:DELETED");
    } else {
        Serial.println("[SD] Delete failed");
        sendMessage("ERR:DELETE_FAILED");
    }
}

// ==================== TEST FILE I/O ====================
void testFileIO(fs::FS &fs, const char *path) {
    if (!isSDAvailable()) return;

    feedWatchdog();

    Serial.println("[SD] Starting file I/O test...");

    // Read test
    File file = fs.open(path);
    if (!file) {
        Serial.println("[SD] Failed to open file for test");
        sendMessage("ERR:TEST_OPEN_FAILED");
        return;
    }

    static uint8_t buf[512];
    size_t len = file.size();
    size_t flen = len;
    uint32_t start = millis();

    while (len) {
        feedWatchdog();
        size_t toRead = (len > 512) ? 512 : len;
        file.read(buf, toRead);
        len -= toRead;
    }

    uint32_t readTime = millis() - start;
    file.close();

    String result = "READ:" + String(flen) + "B/" + String(readTime) + "ms";
    Serial.println(result);
    sendMessage(result);

    // Write test (smaller to avoid SD wear)
    file = fs.open(path, FILE_WRITE);
    if (!file) {
        sendMessage("ERR:TEST_WRITE_OPEN_FAILED");
        return;
    }

    start = millis();
    for (int i = 0; i < 256; i++) {  // Write 128KB
        feedWatchdog();
        file.write(buf, 512);
    }
    uint32_t writeTime = millis() - start;
    file.close();

    result = "WRITE:" + String(256 * 512) + "B/" + String(writeTime) + "ms";
    Serial.println(result);
    sendMessage(result);
}

// ==================== LOG TO SD ====================
void logToSD(const char *message) {
    if (!SDOK) return;

    // Check space before logging
    if (!hasSDSpace(1024)) {
        Serial.println("[SD] WARNING: Low space, skipping log");
        return;
    }

    File logFile = SD.open("/log.txt", FILE_APPEND);
    if (logFile) {
        // Add timestamp (mission elapsed time)
        unsigned long elapsed = millis() - missionStartTime;
        logFile.printf("[%lu] %s\n", elapsed, message);
        logFile.close();
    }
}

// ==================== SD CARD CAPACITY ====================
uint64_t getSDTotalMB() {
    if (!SDOK) return 0;
    return SD.totalBytes() / (1024 * 1024);
}

uint64_t getSDUsedMB() {
    if (!SDOK) return 0;
    return SD.usedBytes() / (1024 * 1024);
}

uint64_t getSDFreeMB() {
    if (!SDOK) return 0;
    return (SD.totalBytes() - SD.usedBytes()) / (1024 * 1024);
}

uint8_t getSDFreePercent() {
    if (!SDOK) return 0;
    uint64_t total = SD.totalBytes();
    if (total == 0) return 0;
    uint64_t free = total - SD.usedBytes();
    return (uint8_t)((free * 100) / total);
}

bool hasSDSpace(size_t bytesNeeded) {
    if (!SDOK) return false;
    uint64_t freeBytes = SD.totalBytes() - SD.usedBytes();
    return freeBytes > (bytesNeeded + SD_MIN_FREE_BYTES);
}
