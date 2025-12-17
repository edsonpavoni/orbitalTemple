# Orbital Temple Satellite Firmware - V1.21 Release Notes

**Release Date:** 2025-12-17
**Status:** READY FOR FLIGHT TESTING
**Previous Version:** V1.2

---

## Overview

V1.21 is a critical bug fix release that addresses all major issues identified in multiple code reviews (internal, Claude AI, Gemini AI). This version includes radiation protection, artwork ascension via IPFS, and comprehensive input validation. Recommended for final integration testing before flight.

---

## Critical Fixes (P0)

### 1. Sensor Variable Shadowing - FIXED
**File:** `sensors.cpp`

**Problem in V1.2:**
```cpp
// V1.2 - BROKEN: Created LOCAL variables that shadowed globals
void readLumi(){
  float VM = analogRead(TL) * 5 / 1024.0;  // LOCAL, not global!
  float lux = microamps * 2.0;             // Also local!
}
```

**Fix in V1.21:**
```cpp
// V1.21 - FIXED: Assigns to GLOBAL variables
void readLumi(){
  VM = analogRead(TL) * 5 / 1024.0;        // Global VM
  lux = microamps * 2.0;                   // Global lux
}
```

**Impact:** In V1.2, all telemetry showed zeros. Now sensor data is correctly reported.

---

### 2. Duplicate Function Definitions - FIXED
**Files:** `config.cpp`, `lora.cpp`

**Problem in V1.2:**
- `receivedFlag` was defined in BOTH `lora.cpp` AND `loop.cpp`
- `setFlag()` was defined in BOTH files
- This violated C++ One Definition Rule (ODR)
- Radio interrupts updated one variable, loop checked another

**Fix in V1.21:**
- `receivedFlag` defined ONLY in `config.cpp`
- `setFlag()` defined ONLY in `lora.cpp`
- Declared `extern` where needed

---

### 3. LoRa Sync Word Inconsistency - FIXED
**File:** `config.h`, `lora.cpp`

**Problem in V1.2:**
```cpp
// V1.2 - INCONSISTENT:
startRadio():    sync word 0x10  // Initial setup
return2Rec():    sync word 0x12  // After transmission
sendMensage():   sync word 0x12  // For TX
```

**Fix in V1.21:**
```cpp
// V1.21 - CONSISTENT:
#define LORA_SYNC_WORD 0x12  // Single definition in config.h
// All functions use LORA_SYNC_WORD
```

**Impact:** In V1.2, satellite might stop receiving after first TX. Now consistent.

---

### 4. Boot Counter Bug - FIXED
**File:** `radiation.cpp`

**Problem:** Boot counter started at 2 on first boot instead of 1.

**Fix in V1.21:**
```cpp
if (loadStateWithCRC()) {
    // Existing state - increment
    bootCount++;
} else {
    // First boot starts at 1
    bootCount = 1;
}
```

---

### 5. ADC Resolution Race Condition - FIXED
**Files:** `sensors.cpp`, `setup.cpp`

**Problem:** `readLumi()` changed ADC resolution mid-execution, causing potential race conditions if interrupted.

**Fix in V1.21:**
- ADC resolution set once in `setup.cpp` (12-bit native)
- Never changed during operation
- `readLumi()` calculations updated for 12-bit (4096 levels)

---

## High Priority Fixes (P1)

### 6. Hardware Watchdog Timer - ADDED
**Files:** `config.h`, `config.cpp`, `setup.cpp`

- ESP32 hardware watchdog with 60-second timeout
- Automatic reset if code hangs
- `feedWatchdog()` called throughout all operations
- Survives infinite loops, deadlocks, hardware faults

---

### 7. Input Validation - ADDED
**File:** `loop.cpp:validateMessage()`

**New validations:**
- Message length limits (7-500 chars)
- All delimiters present (-&@#)
- Delimiter order validation
- Satellite ID verification
- Command character validation (alphanumeric only)
- Path traversal prevention (`..` blocked)
- HMAC authentication required

---

### 8. Radiation Protection (TMR + CRC) - ADDED
**Files:** `radiation.h`, `radiation.cpp`

Protection against Single Event Upsets (SEUs) from charged particles in space:

| Technique | Protection |
|-----------|------------|
| **Triple Modular Redundancy (TMR)** | Critical variables stored 3x with 2-of-3 voting |
| **CRC32 checksums** | EEPROM data verified on boot |
| **Periodic scrubbing** | Every 60 seconds, TMR checked/corrected |
| **Catastrophic failure handling** | Auto-restart if all 3 TMR copies differ |

**Protected variables:** mission state, antenna state, boot count, hardware flags

**New command:** `GetRadStatus` - returns total SEU corrections

---

### 9. Non-Blocking State Machine - IMPLEMENTED
**File:** `loop.cpp`

**Problem in V1.2:**
```cpp
// V1.2 - BLOCKING: Satellite unresponsive for 5 minutes!
delay(1000);
for(int i = 0; i <= 99; i++){
  delay(3000);  // 300 seconds blocking
}
```

**Fix in V1.21:**
```cpp
// V1.21 - NON-BLOCKING: Uses millis() and state machine
switch (currentState) {
    case STATE_WAIT_DEPLOY:
        if (now - stateStartTime >= DEPLOY_WAIT_TIME) {
            currentState = STATE_DEPLOYING;
        }
        // Still check for incoming commands!
        if (receivedFlag) { /* process */ }
        break;
}
```

**Impact:** Satellite can receive commands at ANY time, even during deployment.

---

### 10. Non-Blocking Error Recovery - FIXED
**File:** `loop.cpp`

**Problem:** `STATE_ERROR` used blocking `delay(5000)`.

**Fix in V1.21:**
```cpp
// Non-blocking with millis() timer
static unsigned long lastRecoveryAttempt = 0;
if (now - lastRecoveryAttempt >= RECOVERY_INTERVAL) {
    // Attempt recovery
    lastRecoveryAttempt = now;
}
```

---

## New Features

### 11. Accelerometer Recording System - ADDED
**Files:** `accel.h`, `accel.cpp`, `loop.cpp`, `config.cpp`

Record 60 seconds of accelerometer data at 30Hz for orbital dynamics analysis.

**Features:**
- 30Hz sampling rate (1,800 samples per recording)
- Binary format: 16-byte header + 12 bytes/sample (~22KB total)
- Automatic first recording on initial ground contact
- Manual recording via command
- Files stored in `/accel/` directory
- Retrieved via existing `ReadFile` command

**Data Format:**
```
Header (16 bytes):
  - Magic: "ACCEL30" (7 bytes)
  - Version: 1 byte
  - Sample rate: 2 bytes (uint16_t)
  - Sample count: 2 bytes (uint16_t)
  - Reserved: 4 bytes

Per sample (12 bytes):
  - X: float (4 bytes, in g)
  - Y: float (4 bytes, in g)
  - Z: float (4 bytes, in g)
```

**Commands:**
| Command | Description |
|---------|-------------|
| `AccelRecord` | Start 60-second recording |
| `AccelStatus` | Get recording status |
| `AccelList` | List available recordings |
| `AccelCancel` | Cancel current recording |

**Auto-Recording:** First accelerometer recording is automatically triggered on initial ground contact and persisted in EEPROM.

---

### 14. HMAC Authentication - ADDED
**Files:** `config.h`, `config.cpp`, `loop.cpp`

- HMAC-SHA256 message authentication
- 32-byte secret key (truncated to 8 bytes for LoRa bandwidth)
- Prevents unauthorized command execution

**Command Format:**
```
SAT_ID-COMMAND&PATH@DATA#HMAC

Example:
ab4ec7121663a28e7226dbaa238da777-Status&@#a1b2c3d4e5f6g7h8
```

**IMPORTANT:** Change `HMAC_KEY` in `config.cpp` before flight!

---

### 13. Adaptive Beacon System - ADDED
**Files:** `config.h`, `config.cpp`, `loop.cpp`

| Contact Status | Beacon Interval | Message |
|----------------|-----------------|---------|
| Before first contact | 1 minute | "Andar com fe eu vou..." |
| After contact established | 1 hour | "Ainda bem, que agora encontrei voce" |
| No contact for 24+ hours | 5 minutes | "Por mais distante..." |

**Beacon Message Format:**
```
[Message]|T+00:05:23|B:5|C:YES|V:4.1
```

---

### 15. SD Card Retry Logic - ADDED
**File:** `memor.cpp`

Names are sacred - we retry to ensure they are saved:
- 3 retry attempts for write/append operations
- 100ms delay between retries
- Proper error reporting after all retries fail

---

### 16. State Persistence - ADDED
**Files:** `config.cpp`, `radiation.cpp`

**Persisted in EEPROM with CRC32 protection:**
- Mission state
- Boot count
- Antenna deployment status
- Mission start time

**Impact:** If satellite reboots, it doesn't re-attempt antenna deployment.

---

## Testing Infrastructure

### Unit Tests - ADDED
**Location:** `test/`

| Test Suite | Tests | Status |
|------------|-------|--------|
| Parser Fuzz Tests | 35 | All Pass |
| TMR Unit Tests | 15 | All Pass |

**Parser tests cover:**
- Valid commands
- Missing/wrong delimiters
- Path traversal attacks
- Invalid HMAC
- Unicode in data
- Edge cases

**TMR tests cover:**
- Voting logic
- Single bit corruption recovery
- Catastrophic failure detection

### Wokwi Simulation - ADDED
**Location:** `test/wokwi/`

Browser-based ESP32 simulation for state machine testing without hardware.

---

## File Summary

| File | Lines | Description |
|------|-------|-------------|
| `main.ino` | 112 | Entry point with ASCII art header |
| `config.h` | 246 | Configuration, pins, constants, externs |
| `config.cpp` | 317 | Variable definitions, HMAC, beacon system |
| `setup.h` | 20 | Setup function declaration |
| `setup.cpp` | 188 | Initialization with watchdog, ADC setup |
| `loop.h` | 33 | Loop function declarations |
| `loop.cpp` | 626 | State machine, commands, validation |
| `lora.h` | 42 | LoRa function declarations |
| `lora.cpp` | 249 | Radio communication with retry logic |
| `sensors.h` | 40 | Sensor function declarations |
| `sensors.cpp` | 213 | Sensor reading with validation |
| `memor.h` | 65 | SD card function declarations |
| `memor.cpp` | 424 | File operations with retry logic |
| `radiation.h` | 156 | TMR templates, CRC functions |
| `radiation.cpp` | 293 | SEU protection implementation |
| `accel.h` | 98 | Accelerometer recording protocol |
| `accel.cpp` | 297 | Accelerometer recording implementation |
| `id.h` | 16 | ID function declaration |
| `id.cpp` | 23 | Satellite ID |
| **TOTAL** | **~3,800** | |

---

## Testing Checklist

### Pre-Flight Tests Required:

- [ ] Compile without errors
- [ ] Verify sensor telemetry shows real values (not zeros)
- [ ] Verify LoRa RX works after TX
- [ ] Verify HMAC authentication rejects invalid messages
- [ ] Verify watchdog triggers on intentional hang
- [ ] Verify state persists across reboots
- [ ] Verify boot count starts at 1
- [ ] Test all SD card commands
- [ ] Test accelerometer recording (AccelRecord, AccelList)
- [ ] Test artwork ascension (artworkAscension, artworkList)
- [ ] Verify auto-recording triggers on first ground contact
- [ ] Test antenna deployment sequence
- [ ] Run 7-day continuous soak test
- [ ] Power cycle testing (random resets)

### Ground Station Updates Required:

- [ ] Update to include HMAC signature in commands
- [ ] Update command format: `SAT_ID-COMMAND&PATH@DATA#HMAC`
- [ ] Implement HMAC calculation with shared key
- [ ] Update telemetry parser for new format

**Full test protocol:** [`docs/TEST_PROTOCOL.md`](docs/TEST_PROTOCOL.md)

---

## Breaking Changes

1. **Command format changed:** Must include HMAC signature
2. **Telemetry format changed:** Now includes timestamp prefix
3. **Function renamed:** `sendMensage()` → `sendMessage()`
4. **New commands added:** artworkAscension, GetRadStatus, AccelRecord

---

## Known Limitations

1. **No encryption:** Messages are authenticated but not encrypted
2. **No replay protection:** HMAC prevents modification but not replay
3. **String class usage:** May cause heap fragmentation over long periods (monitor in soak test)

---

## V1.2 vs V1.21 Comparison

| Aspect | V1.2 | V1.21 |
|--------|------|-------|
| Telemetry | Broken (zeros) | Working |
| Security | None | HMAC auth |
| Watchdog | None | 60s timeout |
| Blocking delays | 5-20 min unresponsive | Non-blocking |
| State persistence | None | EEPROM + CRC |
| Beacon | Fixed interval | Adaptive |
| Radiation protection | None | TMR + CRC32 |
| Artwork ascension | None | IPFS-based storage |
| Accelerometer recording | None | 30Hz for 60s |
| Boot counter | Bug (started at 2) | Fixed (starts at 1) |
| ADC handling | Race condition | Fixed |
| Unit tests | None | 50 tests |

---

## HMAC Key Configuration

**CRITICAL:** Change the HMAC key before flight!

Location: `config.cpp` line 18-23

```cpp
const uint8_t HMAC_KEY[HMAC_KEY_LENGTH] = {
    // Replace with your own 32-byte key!
    // Generate with: openssl rand -hex 32
    0x4f, 0x72, 0x62, 0x69, ...
};
```

---

*AD ASTRA - TO THE STARS*

*Orbital Temple Satellite Firmware V1.21*
