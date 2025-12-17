# Orbital Temple Satellite Firmware - V1.21 Release Notes

**Release Date:** 2025-12-16
**Status:** READY FOR FLIGHT TESTING
**Previous Version:** V1.2

---

## Overview

V1.21 is a critical bug fix release that addresses all major issues identified in the V1.2 code review. This version is recommended for final integration testing before flight.

---

## Critical Fixes (P0)

### 1. Sensor Variable Shadowing - FIXED
**File:** `sensors.cpp:39-43, 46-62`

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
**Files:** `config.cpp`, `lora.cpp`, `loop.cpp`

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

## High Priority Fixes (P1)

### 4. Hardware Watchdog Timer - ADDED
**Files:** `config.h`, `config.cpp`, `setup.cpp`

**New in V1.21:**
- ESP32 hardware watchdog with 60-second timeout
- Automatic reset if code hangs
- `feedWatchdog()` called throughout all operations
- Survives infinite loops, deadlocks, hardware faults

```cpp
// Setup
esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
esp_task_wdt_add(NULL);

// In all long operations
feedWatchdog();
```

---

### 5. Input Validation - ADDED
**File:** `loop.cpp:validateMessage()`

**New validations:**
- Message length limits (7-500 chars)
- All delimiters present (-&@#)
- Delimiter order validation
- Satellite ID verification
- Command character validation (alphanumeric only)
- Path traversal prevention (`..` blocked)
- HMAC authentication required

```cpp
bool validateMessage(const String& msg, String& satId, String& command,
                     String& path, String& data, String& hmac) {
    // All validations performed before any command execution
}
```

---

## Medium Priority Fixes (P2)

### 6. Non-Blocking State Machine - IMPLEMENTED
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
    // ...
}
```

**Impact:** Satellite can receive commands at ANY time, even during deployment.

---

### 7. HMAC Authentication - ADDED
**Files:** `config.h`, `config.cpp`, `loop.cpp`

**New security features:**
- HMAC-SHA256 message authentication
- 32-byte secret key (configurable)
- Prevents unauthorized command execution
- Prevents message replay attacks

**Command Format:**
```
SAT_ID-COMMAND&PATH@DATA#HMAC

Example:
ab4ec7121663a28e7226dbaa238da777-Status&@#a1b2c3d4e5f6g7h8
```

**IMPORTANT:** Change `HMAC_KEY` in `config.cpp` before flight!

---

## Lower Priority Fixes (P3)

### 8. State Persistence - ADDED
**Files:** `config.h`, `config.cpp`

**Now persisted in EEPROM:**
- Mission state (boot/wait/deploying/operational)
- Boot count
- Antenna deployment status
- Mission start time

**Impact:** If satellite reboots, it doesn't re-attempt antenna deployment.

---

### 9. SD Card Status Tracking - ADDED
**Files:** `config.h`, `sensors.cpp`, `memor.cpp`

**New in V1.21:**
- `SDOK` flag tracks SD card availability
- All SD operations check flag before executing
- Proper error messages if SD unavailable

---

### 10. Telemetry Timestamps - ADDED
**File:** `loop.cpp`

**New telemetry format:**
```
T+00:05:23|IMU:OK,SD:OK,RF:OK|BAT:4.12V|TEMP:25.3C|LUX:1523.4|...
```

**Format:** `T+HH:MM:SS` mission elapsed time prefix on all telemetry.

---

### 11. Memory-Safe Directory Listing - FIXED
**File:** `memor.cpp:listDir()`

**Problem in V1.2:**
```cpp
// V1.2 - Memory exhaustion risk
String msgld = "";
while (file) {
  msgld += "...";  // Keeps growing!
  sendMensage(msgld);
}
```

**Fix in V1.21:**
```cpp
// V1.21 - Send each entry separately
while (file) {
  String entry = "F:" + String(file.name());
  sendMessage(entry);  // Fixed size
  file = root.openNextFile();
}
```

---

### 12. Division-by-Zero Protection - ADDED
**File:** `sensors.cpp:readTemp()`

**New in V1.21:**
```cpp
double denominator = Vs - Vout;
if (fabs(denominator) < 0.01) {
    Serial.println("[TEMP] WARNING: Division by zero prevented!");
    Tc = -999.0;  // Error indicator
    return;
}
```

---

### 13. Adaptive Beacon System - ADDED
**Files:** `config.h`, `config.cpp`, `loop.cpp`

**New adaptive beacon feature:**

The satellite automatically adjusts beacon frequency based on ground contact status:

| Status | Interval | Purpose |
|--------|----------|---------|
| Before first contact | 1 minute | Help ground station find satellite |
| After contact established | 1 hour | Normal operation, save power |
| No contact for 24+ hours | 5 minutes | Re-acquisition mode |

**Easy to Configure:**
All timing values are in `config.h`:

```cpp
// BEACON CONFIGURATION - Easy to change!
#define BEACON_INTERVAL_NO_CONTACT   60000UL      // 1 minute
#define BEACON_INTERVAL_NORMAL       3600000UL    // 1 hour
#define BEACON_INTERVAL_LOST         300000UL     // 5 minutes
#define BEACON_LOST_THRESHOLD        86400000UL   // 24 hours
```

**Beacon Message Format:**
```
BEACON:ORBITAL_TEMPLE|T+00:05:23|B:5|C:YES|V:4.1
```
- `T+HH:MM:SS` - Mission elapsed time
- `B:N` - Boot count
- `C:YES/NO` - Ground contact established
- `V:X.X` - Battery voltage

---

## File Summary

| File | Lines | Description |
|------|-------|-------------|
| `main.ino` | 116 | Entry point with ASCII art header |
| `config.h` | 189 | Configuration, pins, constants, externs |
| `config.cpp` | 162 | Variable definitions, HMAC, state persistence |
| `setup.h` | 22 | Setup function declaration |
| `setup.cpp` | 150 | Initialization with watchdog |
| `loop.h` | 29 | Loop function declarations |
| `loop.cpp` | 434 | State machine, commands, validation |
| `lora.h` | 36 | LoRa function declarations |
| `lora.cpp` | 191 | Radio communication with retry logic |
| `sensors.h` | 34 | Sensor function declarations |
| `sensors.cpp` | 171 | Sensor reading with validation |
| `memor.h` | 46 | SD card function declarations |
| `memor.cpp` | 248 | File operations with safety checks |
| `id.h` | 15 | ID function declaration |
| `id.cpp` | 19 | Satellite ID |
| **TOTAL** | **2208** | |

---

## Testing Checklist

### Pre-Flight Tests Required:

- [ ] Compile without errors
- [ ] Verify sensor telemetry shows real values (not zeros)
- [ ] Verify LoRa RX works after TX
- [ ] Verify HMAC authentication rejects invalid messages
- [ ] Verify watchdog triggers on intentional hang
- [ ] Verify state persists across reboots
- [ ] Test all SD card commands
- [ ] Test antenna deployment sequence
- [ ] Run 7-day continuous soak test
- [ ] Thermal testing (-40°C to +85°C)
- [ ] Power cycle testing (random resets)

### Ground Station Updates Required:

- [ ] Update to include HMAC signature in commands
- [ ] Update command format: `SAT_ID-COMMAND&PATH@DATA#HMAC`
- [ ] Implement HMAC calculation with shared key
- [ ] Update telemetry parser for new format

---

## Breaking Changes

1. **Command format changed:** Must include HMAC signature
2. **Telemetry format changed:** Now includes timestamp prefix
3. **Function renamed:** `sendMensage()` → `sendMessage()`

---

## Known Limitations

1. **No encryption:** Messages are authenticated but not encrypted (visible to anyone listening)
2. **No replay protection:** HMAC prevents modification but not replay of valid commands
3. **Single frequency:** No frequency hopping implemented

---

## Recommendations for V1.22

If time permits before flight:

1. Add AES-128 encryption for confidentiality
2. Add sequence number for replay protection
3. Implement frequency hopping or backup frequency
4. Add health check beacon mode

---

## HMAC Key Configuration

**CRITICAL:** Change the HMAC key before flight!

Location: `config.cpp` line 12-17

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
