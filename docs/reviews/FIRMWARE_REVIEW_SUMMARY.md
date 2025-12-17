# Orbital Temple Satellite Firmware Review Summary (V1.21)

**Date:** 2025-12-17
**Reviewer:** Gemini CLI

---

## 1. Overall Assessment

The Orbital Temple satellite firmware, version 1.21, represents a significant engineering achievement. It is clear that considerable effort has been invested in making this system robust, fault-tolerant, and feature-rich. The project's own `V1.21_REVIEW.md` and `TEST_PROTOCOL.md` documents are exceptionally thorough, demonstrating a high level of self-awareness regarding potential issues and a commitment to rigorous testing.

The firmware boasts impressive reliability features such as a non-blocking state machine, watchdog timer, EEPROM state persistence with CRC, adaptive beaconing, and a robust LoRa communication module with error recovery. The implementation of Triple Modular Redundancy (TMR) and CRC32 for radiation protection is particularly commendable. Critical fixes from previous versions (e.g., variable shadowing, inconsistent LoRa sync words, blocking delays) highlight a mature development process.

**However, despite these strengths, the firmware harbors a critical architectural flaw that poses a significant risk of mission failure for long-duration operation: the pervasive use of the Arduino `String` class.** This flaw, combined with a suboptimal interrupt handling mechanism, creates a substantial technical debt that must be addressed.

---

## 2. Key Findings from Project Documentation

The project's internal review documents (`V1.21_REVIEW.md` and `TEST_PROTOCOL.md`) provided an invaluable foundation for this review. They accurately identified several critical areas that my independent analysis corroborated:

*   **Identified Risks (Corroborated):**
    *   **Heap Fragmentation:** The `V1.21_REVIEW.md` accurately identified the `String` class as a source of heap fragmentation, warning of potential unpredictable crashes in long-running systems. My review confirms this is the most severe unmitigated risk.
    *   **Race Condition in Message Handling:** The `V1.21_REVIEW.md` correctly pointed out the risk of losing incoming messages due to `processMessage()` blocking the main loop while new packets arrive. My review confirmed the mechanism for this.
    *   **Unchecked Return Values:** The `V1.21_REVIEW.md` noted a lack of checks for critical functions (e.g., SD card operations). My review found that this has been **largely addressed and fixed**, particularly within the `memor.cpp` module.
    *   **Power Management:** The internal review correctly noted the lack of dynamic CPU frequency scaling and deep sleep modes.
    *   **TMR Catastrophic Failure:** The internal review noted the risk of `tmrRead()` returning `copy1` if all copies differed. My review found this has been **addressed and fixed** by implementing an MCU restart in such a scenario.

*   **Key Strengths (Corroborated):** Non-blocking state machine, watchdog, EEPROM persistence, HMAC authentication, adaptive beacon system, TMR/CRC radiation protection.
*   **Rigorous Testing:** The `TEST_PROTOCOL.md` defines a comprehensive suite of tests, notably including a 7-day soak test, which is crucial for uncovering latent issues like memory fragmentation.

---

## 3. Critical Risks Identified

Based on a line-by-line review, the following represent the most significant, unmitigated risks to the mission:

### 3.1. CRITICAL: Heap Fragmentation due to Pervasive `String` Class Usage

*   **Issue:** The Arduino `String` class is used extensively throughout the firmware for message construction, parsing, and status reporting (e.g., in `getMissionTime()`, `sendTelemetry()`, `validateMessage()`, `getSensorStatus()`, `getRadiationStatus()`, `getImageStatus()`, and many `sendMessage()` calls). Operations like concatenation (`+=`) and `substring()` can lead to frequent memory allocations and deallocations on the heap. This inevitably causes heap fragmentation over time.
*   **Impact:** In a long-duration mission (months to years), this will lead to a gradual degradation of available memory, making it impossible for `malloc()` (which backs `String` operations) to find contiguous blocks of memory. This will result in an unpredictable, unrecoverable crash, leading to mission failure. A 7-day soak test may not reveal this issue if the fragmentation accumulates slowly.
*   **Mitigation (Urgent):** All instances of `String` usage for message building, parsing, and status reporting must be replaced with fixed-size C-style character arrays (`char[]`) and memory-safe string manipulation functions like `snprintf()`, `strncpy()`, and `strlcpy()`.

### 3.2. HIGH: Race Condition & Lost Messages in LoRa Reception

*   **Issue:** The current interrupt handling mechanism for LoRa messages (`setFlag()` setting `receivedFlag = true`) combined with the processing in `mainLoop()` is vulnerable. If a new LoRa packet arrives while a long-running command (e.g., an SD card `writeFile` operation in `processMessage()`) is being executed, the new packet will likely overwrite the radio's hardware buffer or be lost due to the main loop being blocked from reading it in time. The `receivedFlag` will simply be reset by the ISR, but the data will be gone.
*   **Impact:** Commands or critical telemetry from the ground station can be lost, making the satellite unresponsive or uncommandable under high-traffic conditions.
*   **Mitigation (Urgent):** Implement a message queue (e.g., a simple ring buffer of `char` arrays) between the ISR and the main loop. The ISR's only job should be to move the received packet from the radio's FIFO into this buffer. The main loop would then pull messages from this queue for processing.

---

## 4. Module-by-Module Review

### 4.1. `config.h`
*   **Strengths:** Well-structured, centralized configuration, clear comments, correct use of `UL` suffix for timing, correct `extern const` for HMAC key.
*   **Concerns:**
    *   **CRITICAL:** Declares `extern String sat_id` and `extern String MsR`, propagating the `String` class dependency.
    *   **High:** Large number of global variables. While common in Arduino, this can lead to maintainability issues.
    *   **Minor:** Some cryptic global variable names (e.g., `contE`, `contR`).

### 4.2. `main.ino`
*   **Strengths:** Excellent documentation, clear version history, highly modular design delegating logic to `setupGeneral()` and `mainLoop()`. No blocking `delay()` calls.
*   **Concerns:** None. This file serves as a clean entry point.

### 4.3. `setup.h` / `setup.cpp`
*   **Strengths:**
    *   **Highly Robust:** Implements a fault-tolerant initialization sequence. Watchdog timer is correctly initialized and fed frequently.
    *   **Non-Blocking Init:** Peripheral initialization functions set status flags (`IMUOK`, `SDOK`, `RFOK`) instead of blocking on failure, allowing the satellite to boot even with degraded capabilities.
    *   **Power Saving:** `WiFi.mode(WIFI_OFF)` and `btStop()` are correctly called early.
    *   **Safety:** Burn wire GPIO `R1` is set `LOW` on startup.
    *   **EEPROM/Radiation:** Calls `initRadiationProtection()` to load state and enable TMR.
*   **Concerns:** None of significance. Minor `delay()` calls are acceptable in the one-time setup phase.

### 4.4. `loop.h` / `loop.cpp`
*   **Strengths:**
    *   **Solid State Machine:** Well-structured, non-blocking `mainLoop()` with `millis()` for timing transitions.
    *   **Robust Antenna Deployment:** The `handleAntennaDeployment()` sub-state machine is well-implemented, with heating, cooling, retries, and state persistence.
    *   **Excellent Input Validation:** `validateMessage()` performs comprehensive checks for message format, path traversal, and HMAC signature, greatly enhancing security.
*   **Concerns:**
    *   **CRITICAL (Direct Source of Fragmentation):** Pervasive use of `String` for message building (`sendTelemetry()`, `getImageStatus()`, `getMissionTime()`) and parsing (`processMessage()`, `validateMessage()`). `getMissionTime()` and `getSensorStatus()` functions *return* `String` objects, guaranteeing heap allocations.
    *   **HIGH (Race Condition Confirmed):** As detailed in Section 3.2, the interrupt handling logic (ISR only sets a flag) combined with potentially long `processMessage()` calls (especially SD operations) will lead to lost messages.
    *   **Medium:** Blocking `delay(5000)` in `STATE_ERROR` makes the satellite unresponsive during recovery attempts.

### 4.5. `lora.h` / `lora.cpp`
*   **Strengths:**
    *   **Highly Resilient:** Excellent retry mechanisms for initialization and transmission.
    *   **Radio Recovery:** Includes `radioNeedsRecovery()` and `recoverRadio()` functions, demonstrating robust self-healing capability.
    *   **Safety Critical:** Always returns to receive mode (`returnToReceive()`) after transmission, preventing the radio from getting stuck in TX.
    *   **Correct ISR:** `setFlag()` is a minimal and safe ISR, correctly placed in IRAM.
    *   **Sync Word Fix:** Correctly uses `LORA_SYNC_WORD` for consistency.
*   **Concerns:**
    *   **CRITICAL:** `sendMessage(const String& message)` forces the use of `String` objects for all transmissions, contributing to heap fragmentation.
    *   **Minor:** Cryptic global error counter names (`contE`, `contR`).

### 4.6. `sensors.h` / `sensors.cpp`
*   **Strengths:**
    *   **Critical Bug Fix:** Correctly addressed the "variable shadowing" bug from V1.2.
    *   **Robust Peripheral Init:** `BeginIMU()` and `SDBegin()` are non-blocking and set status flags correctly.
    *   **Good Sanity Checks:** Sensor reading functions include checks for out-of-range values and division-by-zero.
*   **Concerns:**
    *   **CRITICAL:** `getSensorStatus()` returns a `String`, contributing to heap fragmentation.
    *   **CRITICAL:** `readLumi()` unsafely changes the global ADC resolution (`analogReadResolution(10)` then `(12)`) in the middle of execution. This can lead to incorrect readings by other ADC-using functions if an interrupt or task switch occurs. All ADC reads should maintain a consistent resolution.
    *   **High:** Blocking `delay(10)` calls in each sensor read function make the system slightly less responsive and work against the non-blocking architecture.
    *   **Medium:** Use of "magic numbers" for voltage divider ratios (e.g., `2.0f`) and ADC calibration. These should be named constants.

### 4.7. `radiation.h` / `radiation.cpp`
*   **Strengths:**
    *   **Excellent SEU Mitigation:** Correct implementation of TMR for critical RAM variables, CRC32 for EEPROM data integrity, and periodic scrubbing.
    *   **Robust Catastrophic Failure Handling:** The `tmrRead()` function correctly restarts the MCU if all three TMR copies differ, which is the safest response to severe corruption.
    *   **Well-designed Interface:** Uses C++ templates for TMR, enhancing reusability and type safety.
*   **Concerns:**
    *   **CRITICAL:** `getRadiationStatus()` returns a `String`, contributing to heap fragmentation.
    *   **High:** Logic flaw in `initRadiationProtection()` causes `bootCount` to start at 2 on the very first boot, not 1, deviating from `TEST_PROTOCOL.md` expectations.
    *   **Medium:** Simplified state restoration in `loadStateWithCRC()` based solely on `antennaDeployed` might lose nuanced mission state (e.g., a saved `STATE_ERROR`).

### 4.8. `id.h` / `id.cpp`
*   **Strengths:** Simple, clear purpose. Correctly notes ID is not for security.
*   **Concerns:** Contributes to the overall `String` class dependency.

### 4.9. `memor.h` / `memor.cpp`
*   **Strengths:**
    *   **EXEMPLARY MEMORY SAFETY:** Functions consistently use `const char*` for paths and data, decoupling this module from the `String` class.
    *   **Extremely Robust:** Thorough error checking for all file operations, retry mechanisms for writes/appends, liberal `feedWatchdog()` calls, and proper file handle closing.
    *   **Resource Management:** Implements `hasSDSpace()` with a minimum free space threshold, preventing writes to a full card.
    *   **Chunking:** `listDir()` and `readFile()` correctly use chunking for LoRa transmission, avoiding memory exhaustion.
    *   **Addressed Fixes:** Successfully addressed the "unchecked return values" and "memory exhaustion in `listDir()`" issues flagged in `V1.21_REVIEW.md`.
*   **Concerns:**
    *   **Medium:** Still uses `String` for constructing response messages (`sendMessage("OK:DIR_CREATED:" + String(path));`), despite the core logic being memory-safe. This should be refactored to use `char` buffers and `snprintf`.

### 4.10. `image.h` / `image.cpp`
*   **Strengths:**
    *   **Robust Protocol:** Well-designed and implemented multi-stage image transfer protocol with chunking, out-of-order reception, duplicate detection, and timeouts.
    *   **Intelligent Error Reporting:** Explicitly lists missing chunks to the ground station, enabling efficient re-transmissions.
    *   **Atomic Operations:** Uses temporary file and atomic `rename()` for safe image finalization.
    *   **Strong Validation:** Comprehensive parameter and state validation.
*   **Concerns:**
    *   **CRITICAL:** `getImageStatus()` and all message-building for `sendMessage()` rely heavily on the `String` class, contributing to heap fragmentation.

---

## 5. Key Recommendations (Actionable Steps)

### 5.1. URGENT: Eliminate `String` Class Usage (Heap Fragmentation)

This is the highest priority. It requires a systematic refactor across the entire codebase.
*   **Action:**
    1.  **Replace `String` return types:** Refactor all functions that return `String` (e.g., `getMissionTime()`, `getSensorStatus()`, `getRadiationStatus()`, `getImageStatus()`) to accept a `char* buffer` and `size_t len` and write their output using `snprintf()`.
    2.  **Replace `String` parameters:** Refactor `sendMessage()` to accept `const char*`.
    3.  **Replace `String` concatenation:** All message building (e.g., `telemetry += ...`, `status += ...`, `sendMessage("HEADER:" + String(value))`) must be rewritten to use `char` buffers and `snprintf()`.
    4.  **Replace `String` parsing:** Replace `String::indexOf()`, `String::substring()`, and `String::equals()` with C-string equivalents (`strstr()`, `strncmp()`, manual parsing with `char*`).

### 5.2. URGENT: Implement Message Queue for LoRa Reception (Race Condition)

*   **Action:**
    1.  Create a small, fixed-size ring buffer (e.g., `char receivedMessageQueue[QUEUE_SIZE][MAX_MESSAGE_LEN]`) and associated read/write pointers.
    2.  Modify the `setFlag()` ISR: Instead of just setting a boolean, it should attempt to copy the received data (`radio.readData()`) directly into the next available slot in the `receivedMessageQueue` and update the write pointer. Crucially, the ISR should *not* perform any complex processing, just copy the raw bytes.
    3.  Modify `mainLoop()`: Instead of checking `receivedFlag`, it should check if there are messages in the `receivedMessageQueue`. If so, it dequeues a message and passes it to `processMessage()`. This decouples reception from processing.

### 5.3. HIGH: Standardize ADC Resolution

*   **Action:** Ensure `analogReadResolution()` is set once globally during `setup()` (ideally to 12-bit, the native resolution of the ESP32 ADC) and never changed mid-execution. Adjust calculations in `readLumi()` to convert the 12-bit raw value to its equivalent 10-bit range if needed for sensor calibration.

### 5.4. HIGH: Fix Boot Counter Logic

*   **Action:** Modify `initRadiationProtection()` to correctly increment the `bootCount` to 1 on the very first boot and to `(last_boot_count + 1)` on subsequent boots.

### 5.5. MEDIUM: Eliminate Blocking `delay()` from Operational Code

*   **Action:** Replace `delay(5000)` in the `STATE_ERROR` handler with a non-blocking timer (`millis()`) to allow the system to remain responsive even in error states. Remove `delay(10)` calls in sensor reading functions; if stabilization is needed, use non-blocking methods or dual-read techniques.

---

## 6. Final Assessment

The Orbital Temple firmware is an incredibly ambitious and complex project that has achieved a high degree of functional robustness. The developers have clearly invested deeply in building a reliable system for the challenging space environment.

**However, the identified critical risks related to memory management and LoRa message handling are severe and will likely lead to mission failure in the long term if not addressed.** These are not minor bugs; they are architectural flaws that require significant refactoring.

**My recommendation is to perform an immediate refactoring cycle focused on eliminating `String` class usage and implementing a robust message queue.** The existing `memor.cpp` module serves as an excellent blueprint for how memory-safe string handling should be implemented across the entire codebase.

Once these critical issues are resolved, the comprehensive `TEST_PROTOCOL.md`, especially the 7-day soak test, will be invaluable in verifying the long-term stability and reliability of the firmware.

*Ad Astra*
