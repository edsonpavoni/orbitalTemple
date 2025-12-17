/*
 * Orbital Temple Satellite - Image Transfer Module
 * Version: 1.21
 *
 * Allows uploading small images (64x64 pixels) to the satellite.
 *
 * PROTOCOL:
 *   1. ImageStart - Begin transfer, specify file and chunks
 *   2. ImageChunk - Send chunks (base64 encoded, numbered)
 *   3. ImageEnd   - Finalize and verify
 *
 * LIMITATIONS:
 *   - Max image size: 8 KB (enough for 64x64 JPEG)
 *   - Chunk size: 128 bytes of image data per chunk
 *   - Transfer time: ~1-2 minutes for 5KB image
 */

#ifndef IMAGE_H
#define IMAGE_H

#include <Arduino.h>

// Configuration
#define IMAGE_MAX_SIZE       8192    // 8 KB max image size
#define IMAGE_CHUNK_SIZE     128     // Bytes of image data per chunk
#define IMAGE_MAX_CHUNKS     64      // Max chunks (8192 / 128)
#define IMAGE_TIMEOUT_MS     60000   // 60 seconds timeout between chunks

// Transfer state
typedef enum {
    IMG_IDLE,           // No transfer in progress
    IMG_RECEIVING,      // Receiving chunks
    IMG_COMPLETE,       // All chunks received
    IMG_ERROR           // Error occurred
} ImageTransferState;

// Transfer context
struct ImageTransfer {
    ImageTransferState state;
    char filename[64];
    uint16_t totalChunks;
    uint16_t receivedChunks;
    uint16_t expectedSize;
    uint16_t currentSize;
    unsigned long lastChunkTime;
    bool chunkReceived[IMAGE_MAX_CHUNKS];  // Track which chunks received
};

extern ImageTransfer imageTransfer;

// Initialize image transfer system
void initImageTransfer();

// Start a new image transfer
// Returns: true if started, false if busy or invalid params
bool imageStart(const char* filename, uint16_t totalChunks, uint16_t expectedSize);

// Receive a chunk
// chunkNum: 0-indexed chunk number
// data: base64 encoded chunk data
// Returns: true if chunk accepted
bool imageChunk(uint16_t chunkNum, const char* base64Data);

// End transfer and verify
// Returns: true if image saved successfully
bool imageEnd();

// Cancel current transfer
void imageCancel();

// Get transfer status string
String getImageStatus();

// Check for timeout
void imageTimeoutCheck();

// Base64 decode helper
int base64Decode(const char* input, uint8_t* output, int maxOutputLen);

#endif // IMAGE_H
