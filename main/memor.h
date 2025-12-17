#ifndef MEMOR_H
#define MEMOR_H

/*
 * Orbital Temple Satellite - Memory/SD Card Module
 * Version: 1.21
 *
 * CHANGELOG from V1.2:
 * - FIXED: listDir() memory exhaustion bug (unbounded string concatenation)
 * - Added SD card availability check before all operations
 * - Added file size limits for safety
 * - Improved error reporting
 */

#include "FS.h"

// List directory contents
// Sends results via LoRa in chunks to avoid memory exhaustion
void listDir(fs::FS &fs, const char *dirname, uint8_t levels);

// Create a directory
void createDir(fs::FS &fs, const char *path);

// Remove a directory
void removeDir(fs::FS &fs, const char *path);

// Read file contents
// Sends contents via LoRa in chunks (max 200 bytes per chunk)
void readFile(fs::FS &fs, const char *path);

// Write to file (overwrite)
void writeFile(fs::FS &fs, const char *path, const char *message);

// Append to file
void appendFile(fs::FS &fs, const char *path, const char *message);

// Rename file
void renameFile(fs::FS &fs, const char *path1, const char *path2);

// Delete file
void deleteFile(fs::FS &fs, const char *path);

// Test file I/O performance
void testFileIO(fs::FS &fs, const char *path);

// Check if SD card is available
bool isSDAvailable();

// Log message to SD card (for debugging)
void logToSD(const char *message);

// Get SD card capacity info
uint64_t getSDTotalMB();
uint64_t getSDUsedMB();
uint64_t getSDFreeMB();
uint8_t getSDFreePercent();

// Check if SD has enough space (returns false if < threshold)
bool hasSDSpace(size_t bytesNeeded);

// Minimum free space threshold (bytes) - reject writes below this
#define SD_MIN_FREE_BYTES  1048576  // 1 MB minimum free space

#endif // MEMOR_H
