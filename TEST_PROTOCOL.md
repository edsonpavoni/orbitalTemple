# Orbital Temple - Hardware Test Protocol

**Version:** 1.21
**Duration:** ~4 hours + 7-day soak test
**Required:** Satellite, Ground Station, Multimeter, Thermal Chamber (optional)

---

## Before You Start

```
[ ] HMAC key changed in config.cpp (not default)
[ ] Code compiled without errors
[ ] Fresh SD card formatted (FAT32)
[ ] Battery fully charged
[ ] Ground station software ready
```

---

## Phase 1: Power-On Sanity (10 min)

Connect USB, open Serial Monitor (115200 baud).

| # | Test | Expected | ✓ |
|---|------|----------|---|
| 1.1 | Power on | Boot message appears within 3s | |
| 1.2 | No `while(!Serial)` hang | Boots without USB connected | |
| 1.3 | Watchdog init | `[SETUP] Watchdog configured: 60 second timeout` | |
| 1.4 | Status summary | Shows IMU, SD, Radio status | |

**If any fail → STOP. Fix before continuing.**

---

## Phase 2: Subsystem Tests (1 hour)

### 2A. Radio (LoRa)

| # | Test | Command | Expected | ✓ |
|---|------|---------|----------|---|
| 2A.1 | TX works | Wait for beacon | Ground station receives beacon | |
| 2A.2 | RX works | Send `Ping` | Satellite responds `PONG` | |
| 2A.3 | Sync word | Check frequency | 401.5 MHz RX, 468.5 MHz TX | |

### 2B. SD Card

| # | Test | Command | Expected | ✓ |
|---|------|---------|----------|---|
| 2B.1 | Write | `WriteFile&/test.txt@hello` | `OK:WRITTEN:5B` | |
| 2B.2 | Read | `ReadFile&/test.txt@` | Returns `hello` | |
| 2B.3 | List | `ListDir&/@` | Shows files | |
| 2B.4 | Capacity | `Status` | `SD:XX%` in telemetry | |
| 2B.5 | Delete | `DeleteFile&/test.txt@` | `OK:DELETED` | |

### 2C. Sensors

| # | Test | Command | Expected | ✓ |
|---|------|---------|----------|---|
| 2C.1 | Battery | `Status` | `BAT:X.XXV` (not 0.00V) | |
| 2C.2 | Temperature | `Status` | `TEMP:XX.XC` (reasonable) | |
| 2C.3 | Luminosity | Cover/uncover sensor | `LUX` value changes | |
| 2C.4 | IMU | Move satellite | Gyro/Accel values change | |

### 2D. Security

| # | Test | Command | Expected | ✓ |
|---|------|---------|----------|---|
| 2D.1 | Valid HMAC | Correct signature | Command executes | |
| 2D.2 | Invalid HMAC | Wrong signature | `ERR:AUTH_FAILED` | |
| 2D.3 | Wrong sat ID | Different ID | Ignored (no response) | |
| 2D.4 | Path traversal | `ReadFile&/../etc@` | `ERR:PATH_TRAVERSAL_BLOCKED` | |
| 2D.5 | Malformed | Missing delimiters | Rejected silently | |

---

## Phase 3: State Machine (30 min)

### 3A. Fresh Boot Sequence

1. Erase EEPROM (or use fresh chip)
2. Power on
3. Observe state transitions:

| # | State | Timing | Indicator | ✓ |
|---|-------|--------|-----------|---|
| 3A.1 | BOOT | Immediate | Serial: `[STATE] Boot complete` | |
| 3A.2 | WAIT_DEPLOY | 0-5 min | Beacons every 1 min | |
| 3A.3 | DEPLOYING | After 5 min | Burn wire activates (GPIO 27 HIGH) | |
| 3A.4 | OPERATIONAL | After deploy | Beacons every 1 hour | |

### 3B. Antenna Deployment

| # | Test | Action | Expected | ✓ |
|---|------|--------|----------|---|
| 3B.1 | Switch pressed | Hold switch | `AntSwitch: PRESSED` in setup | |
| 3B.2 | Burn wire | Enter DEPLOYING | GPIO 27 goes HIGH for 90s | |
| 3B.3 | Switch release | Release switch | `OK:ANTENNA_DEPLOYED` message | |
| 3B.4 | Burn wire off | After release | GPIO 27 goes LOW immediately | |

### 3C. State Persistence

| # | Test | Action | Expected | ✓ |
|---|------|--------|----------|---|
| 3C.1 | Boot count | Power cycle 3x | Boot count increments each time | |
| 3C.2 | Antenna memory | Deploy, then reboot | Skips to OPERATIONAL | |
| 3C.3 | No re-deploy | Multiple reboots | Never re-heats burn wire | |

---

## Phase 4: Edge Cases (30 min)

### 4A. Watchdog

| # | Test | Action | Expected | ✓ |
|---|------|--------|----------|---|
| 4A.1 | Normal operation | Run for 5 min | No unexpected resets | |
| 4A.2 | Forced hang | (Code mod: infinite loop) | Resets after 60s | |

### 4B. Radiation Protection (Simulated)

| # | Test | Action | Expected | ✓ |
|---|------|--------|----------|---|
| 4B.1 | TMR scrub | Wait 60s | `[RAD]` messages if corrections | |
| 4B.2 | SEU counter | `GetRadStatus` | Returns `RAD:SEU_TOTAL:0` | |

### 4C. Error Recovery

| # | Test | Action | Expected | ✓ |
|---|------|--------|----------|---|
| 4C.1 | SD removed | Remove SD during operation | `ERR:SD_NOT_AVAILABLE` | |
| 4C.2 | SD reinsert | Reboot with SD | SD works again | |
| 4C.3 | Radio recovery | (If radio fails) | Auto-retry, then restart | |

---

## Phase 5: Beacon Timing (1 hour)

| # | Condition | Expected Interval | ✓ |
|---|-----------|-------------------|---|
| 5.1 | Before first contact | 1 minute | |
| 5.2 | After receiving command | 1 hour | |
| 5.3 | No contact for 24h | 5 minutes | |

**Test 5.3:** Set `BEACON_LOST_THRESHOLD` to 5 minutes temporarily, wait for mode change.

---

## Phase 6: Soak Test (7 days)

**Setup:**
- Power via solar panel simulator OR battery + charger
- Ground station logging all beacons
- No manual intervention

**Monitor:**

| Day | Check | Expected | ✓ |
|-----|-------|----------|---|
| 1 | Beacons received | Regular interval | |
| 2 | Boot count | Still 1 (no crashes) | |
| 3 | Memory | No `CRITICAL` or `ERROR` logs | |
| 4 | SD free space | Not filling unexpectedly | |
| 5 | Telemetry values | Sensors still reading | |
| 6 | Command response | `Ping` → `PONG` | |
| 7 | Final status | All systems nominal | |

**PASS criteria:** Zero unexpected reboots, zero errors, all commands work.

---

## Phase 7: Thermal (Optional but Recommended)

| # | Temp | Duration | Check | ✓ |
|---|------|----------|-------|---|
| 7.1 | +60°C | 2 hours | Responds to Ping | |
| 7.2 | -20°C | 2 hours | Responds to Ping | |
| 7.3 | Cycle | 5 cycles | No damage, boots normally | |

---

## Final Sign-Off

```
Date: _______________

[ ] All Phase 1-4 tests passed
[ ] 7-day soak test passed (zero crashes)
[ ] Thermal test passed (if done)
[ ] HMAC key is UNIQUE (not default)
[ ] SD card contains no test files
[ ] Boot count reset to 0
[ ] Battery fully charged

Tested by: _______________________

Ready for flight: [ ] YES  [ ] NO
```

---

## Quick Reference: Test Commands

```
Format: SAT_ID-COMMAND&PATH@DATA#HMAC

Ping          SAT001-Ping&@#[HMAC]
Status        SAT001-Status&@#[HMAC]
WriteFile     SAT001-WriteFile&/names/test.txt@Maria Silva#[HMAC]
ReadFile      SAT001-ReadFile&/names/test.txt@#[HMAC]
ListDir       SAT001-ListDir&/@#[HMAC]
DeleteFile    SAT001-DeleteFile&/names/test.txt@#[HMAC]
GetState      SAT001-GetState&@#[HMAC]
GetRadStatus  SAT001-GetRadStatus&@#[HMAC]
MCURestart    SAT001-MCURestart&@#[HMAC]
```

---

*Ad Astra*
