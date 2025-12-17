/*
 * Orbital Temple Satellite - Image Transfer Implementation
 * Version: 1.21
 */

#include "image.h"
#include "config.h"
#include "memor.h"
#include "lora.h"

// Global transfer context
ImageTransfer imageTransfer;

// Temporary file for receiving chunks
#define TEMP_IMAGE_FILE "/temp_image.bin"

// Base64 decoding table
static const uint8_t base64_table[128] = {
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
    64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
    64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
    64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64
};

// Base64 decode
int base64Decode(const char* input, uint8_t* output, int maxOutputLen) {
    if (input == NULL || output == NULL || maxOutputLen <= 0) {
        return 0;
    }
    int inputLen = strlen(input);
    int outputLen = 0;
    uint32_t buffer = 0;
    int bitsCollected = 0;

    for (int i = 0; i < inputLen && outputLen < maxOutputLen; i++) {
        char c = input[i];
        if (c == '=') break;  // Padding
        if (c < 0 || c >= 128) continue;  // Invalid char

        uint8_t value = base64_table[(int)c];
        if (value == 64) continue;  // Invalid char

        buffer = (buffer << 6) | value;
        bitsCollected += 6;

        if (bitsCollected >= 8) {
            bitsCollected -= 8;
            output[outputLen++] = (buffer >> bitsCollected) & 0xFF;
        }
    }

    return outputLen;
}

void initImageTransfer() {
    imageTransfer.state = IMG_IDLE;
    imageTransfer.filename[0] = '\0';
    imageTransfer.totalChunks = 0;
    imageTransfer.receivedChunks = 0;
    imageTransfer.expectedSize = 0;
    imageTransfer.currentSize = 0;
    imageTransfer.lastChunkTime = 0;

    for (int i = 0; i < IMAGE_MAX_CHUNKS; i++) {
        imageTransfer.chunkReceived[i] = false;
    }

    Serial.println("[IMG] Image transfer system initialized");
}

bool imageStart(const char* filename, uint16_t totalChunks, uint16_t expectedSize) {
    feedWatchdog();

    // Check if already busy
    if (imageTransfer.state == IMG_RECEIVING) {
        Serial.println("[IMG] ERROR: Transfer already in progress");
        sendMessage("ERR:IMG_BUSY");
        return false;
    }

    // Validate parameters
    if (totalChunks == 0 || totalChunks > IMAGE_MAX_CHUNKS) {
        Serial.printf("[IMG] ERROR: Invalid chunk count: %d\n", totalChunks);
        sendMessage("ERR:IMG_INVALID_CHUNKS");
        return false;
    }

    if (expectedSize == 0 || expectedSize > IMAGE_MAX_SIZE) {
        Serial.printf("[IMG] ERROR: Invalid size: %d\n", expectedSize);
        sendMessage("ERR:IMG_TOO_LARGE");
        return false;
    }

    if (!SDOK) {
        Serial.println("[IMG] ERROR: SD card not available");
        sendMessage("ERR:SD_NOT_AVAILABLE");
        return false;
    }

    // Check space
    if (!hasSDSpace(expectedSize + 1024)) {
        Serial.println("[IMG] ERROR: Not enough space");
        sendMessage("ERR:SD_FULL");
        return false;
    }

    // Initialize transfer
    strncpy(imageTransfer.filename, filename, sizeof(imageTransfer.filename) - 1);
    imageTransfer.filename[sizeof(imageTransfer.filename) - 1] = '\0';
    imageTransfer.totalChunks = totalChunks;
    imageTransfer.expectedSize = expectedSize;
    imageTransfer.receivedChunks = 0;
    imageTransfer.currentSize = 0;
    imageTransfer.lastChunkTime = millis();
    imageTransfer.state = IMG_RECEIVING;

    for (int i = 0; i < IMAGE_MAX_CHUNKS; i++) {
        imageTransfer.chunkReceived[i] = false;
    }

    // Delete temp file if exists
    SD.remove(TEMP_IMAGE_FILE);

    // Create empty temp file
    File f = SD.open(TEMP_IMAGE_FILE, FILE_WRITE);
    if (!f) {
        Serial.println("[IMG] ERROR: Cannot create temp file");
        sendMessage("ERR:IMG_FILE_ERROR");
        imageTransfer.state = IMG_ERROR;
        return false;
    }
    f.close();

    Serial.printf("[IMG] Transfer started: %s (%d chunks, %d bytes)\n",
                  filename, totalChunks, expectedSize);
    sendMessage("OK:IMG_START:" + String(totalChunks));

    return true;
}

bool imageChunk(uint16_t chunkNum, const char* base64Data) {
    feedWatchdog();

    // Check state
    if (imageTransfer.state != IMG_RECEIVING) {
        Serial.println("[IMG] ERROR: No transfer in progress");
        sendMessage("ERR:IMG_NOT_STARTED");
        return false;
    }

    // Validate chunk number
    if (chunkNum >= imageTransfer.totalChunks) {
        Serial.printf("[IMG] ERROR: Invalid chunk number: %d\n", chunkNum);
        sendMessage("ERR:IMG_INVALID_CHUNK");
        return false;
    }

    // Check if already received (duplicate)
    if (imageTransfer.chunkReceived[chunkNum]) {
        Serial.printf("[IMG] Chunk %d already received, skipping\n", chunkNum);
        sendMessage("OK:IMG_DUP:" + String(chunkNum));
        return true;
    }

    // Decode base64
    uint8_t decoded[IMAGE_CHUNK_SIZE + 10];
    int decodedLen = base64Decode(base64Data, decoded, sizeof(decoded));

    if (decodedLen <= 0) {
        Serial.println("[IMG] ERROR: Base64 decode failed");
        sendMessage("ERR:IMG_DECODE");
        return false;
    }

    // Write to temp file at correct position
    File f = SD.open(TEMP_IMAGE_FILE, FILE_WRITE);
    if (!f) {
        Serial.println("[IMG] ERROR: Cannot open temp file");
        sendMessage("ERR:IMG_FILE_ERROR");
        return false;
    }

    // Seek to chunk position
    uint32_t pos = (uint32_t)chunkNum * IMAGE_CHUNK_SIZE;
    f.seek(pos);
    size_t written = f.write(decoded, decodedLen);
    f.close();

    if (written != (size_t)decodedLen) {
        Serial.println("[IMG] ERROR: Write failed");
        sendMessage("ERR:IMG_WRITE");
        return false;
    }

    // Mark chunk as received
    imageTransfer.chunkReceived[chunkNum] = true;
    imageTransfer.receivedChunks++;
    imageTransfer.currentSize += decodedLen;
    imageTransfer.lastChunkTime = millis();

    Serial.printf("[IMG] Chunk %d/%d received (%d bytes)\n",
                  chunkNum + 1, imageTransfer.totalChunks, decodedLen);

    // Send progress
    sendMessage("OK:IMG_CHUNK:" + String(chunkNum) + "/" + String(imageTransfer.totalChunks));

    return true;
}

bool imageEnd() {
    feedWatchdog();

    if (imageTransfer.state != IMG_RECEIVING) {
        Serial.println("[IMG] ERROR: No transfer in progress");
        sendMessage("ERR:IMG_NOT_STARTED");
        return false;
    }

    // Check if all chunks received
    if (imageTransfer.receivedChunks < imageTransfer.totalChunks) {
        // Find missing chunks
        String missing = "";
        int missingCount = 0;
        for (int i = 0; i < imageTransfer.totalChunks && missingCount < 5; i++) {
            if (!imageTransfer.chunkReceived[i]) {
                if (missing.length() > 0) missing += ",";
                missing += String(i);
                missingCount++;
            }
        }

        Serial.printf("[IMG] Missing %d chunks\n",
                      imageTransfer.totalChunks - imageTransfer.receivedChunks);
        sendMessage("ERR:IMG_MISSING:" + missing);
        return false;
    }

    // Rename temp file to final filename
    // First remove existing file if any
    SD.remove(imageTransfer.filename);

    // Rename temp to final
    if (!SD.rename(TEMP_IMAGE_FILE, imageTransfer.filename)) {
        Serial.println("[IMG] ERROR: Cannot rename file");
        sendMessage("ERR:IMG_RENAME");
        imageTransfer.state = IMG_ERROR;
        return false;
    }

    imageTransfer.state = IMG_COMPLETE;

    Serial.printf("[IMG] Transfer complete: %s (%d bytes)\n",
                  imageTransfer.filename, imageTransfer.currentSize);
    sendMessage("OK:IMG_COMPLETE:" + String(imageTransfer.filename) +
                ":" + String(imageTransfer.currentSize) + "B");

    // Reset for next transfer
    initImageTransfer();

    return true;
}

void imageCancel() {
    if (imageTransfer.state == IMG_RECEIVING) {
        SD.remove(TEMP_IMAGE_FILE);
        Serial.println("[IMG] Transfer cancelled");
        sendMessage("OK:IMG_CANCELLED");
    }
    initImageTransfer();
}

String getImageStatus() {
    String status = "IMG:";

    switch (imageTransfer.state) {
        case IMG_IDLE:
            status += "IDLE";
            break;
        case IMG_RECEIVING:
            status += "RX:" + String(imageTransfer.receivedChunks) +
                      "/" + String(imageTransfer.totalChunks);
            break;
        case IMG_COMPLETE:
            status += "COMPLETE";
            break;
        case IMG_ERROR:
            status += "ERROR";
            break;
    }

    return status;
}

void imageTimeoutCheck() {
    if (imageTransfer.state == IMG_RECEIVING) {
        unsigned long elapsed = millis() - imageTransfer.lastChunkTime;
        if (elapsed > IMAGE_TIMEOUT_MS) {
            Serial.println("[IMG] Transfer timeout");
            sendMessage("ERR:IMG_TIMEOUT");
            imageCancel();
        }
    }
}
