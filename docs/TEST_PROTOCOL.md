# Orbital Temple - Hardware Test Protocol

**Version:** 1.21
**Total time:** ~4 hours + 7-day soak test

---

## Before You Start

Make sure you have:
- The satellite with battery charged
- Ground station computer with radio
- USB cable for serial monitoring
- Fresh SD card (FAT32 formatted)
- Multimeter (optional)

And confirm:
- You changed the HMAC key in `config.cpp` (not the default one)
- Code compiled without errors

---

## Step 1: First Power On

Plug the USB cable into the satellite and open the Serial Monitor at 115200 baud.

**What you should see:**

```
=============================================
  ORBITAL TEMPLE SATELLITE
  Firmware Version: 1.21
  A memorial in outer space
=============================================

[SETUP] Initializing watchdog timer...
[SETUP] Watchdog configured: 60 second timeout
[SETUP] Initializing EEPROM...
[SETUP] Pins configured
[SETUP] Initializing LoRa radio...
[SETUP] Radio initialized successfully

=============================================
  SETUP COMPLETE - STATUS SUMMARY
=============================================
  IMU:      OK
  SD Card:  OK
  Radio:    OK
  Antenna:  PENDING
  Boot #:   1
=============================================
```

**If it hangs or shows errors, stop here and fix before continuing.**

---

## Step 2: Test Without USB

Unplug the USB cable. Power the satellite from battery only. Wait 10 seconds, then plug USB back in.

**What you should see:**

The satellite should have booted normally. The boot count should now be 2.

This confirms there's no `while(!Serial)` blocking issue.

---

## Step 3: Radio Transmit

Wait for the satellite to send a beacon (within 1 minute if no previous contact).

**What you should see on the ground station:**

```
Andar com fe eu vou, que a fe nao costuma faia.|T+00:01:00|B:2|C:NO|V:3.7
```

This confirms the radio is transmitting.

---

## Step 4: Radio Receive

From the ground station, send a Ping command:

```
SAT001-Ping&@#[your HMAC signature]
```

**What you should see:**

The satellite responds with:
```
PONG|T+00:02:15
```

This confirms the radio is receiving and processing commands.

---

## Step 5: Sensor Readings

Send a Status command:

```
SAT001-Status&@#[HMAC]
```

**What you should see:**

```
T+00:03:00|IMU:OK SD:OK RF:OK|BAT:3.85V|TEMP:24.5C|LUX:150.0|GYR:...|ACC:...|MAG:...|SD:87%|SEU:0
```

Check that:
- Battery voltage is reasonable (3.0V - 4.2V)
- Temperature is reasonable (not 0.00 or -999)
- Lux changes when you cover the sensor
- Gyro/Accel values change when you move the satellite

---

## Step 6: SD Card Write

Send a WriteFile command:

```
SAT001-WriteFile&/test.txt@Hello from Earth#[HMAC]
```

**What you should see:**

```
OK:WRITTEN:16B
```

---

## Step 7: SD Card Read

Send a ReadFile command:

```
SAT001-ReadFile&/test.txt@#[HMAC]
```

**What you should see:**

```
FILE:/test.txt,16
Hello from Earth
END:FILE
```

---

## Step 8: SD Card Delete

Send a DeleteFile command:

```
SAT001-DeleteFile&/test.txt@#[HMAC]
```

**What you should see:**

```
OK:DELETED
```

---

## Step 9: Security - Wrong HMAC

Send a command with an incorrect signature:

```
SAT001-Ping&@#wrongsignature123
```

**What you should see:**

```
ERR:AUTH_FAILED
```

The satellite should reject it.

---

## Step 10: Security - Path Traversal

Send a malicious path:

```
SAT001-ReadFile&/../../../etc/passwd@#[HMAC]
```

**What you should see:**

```
ERR:PATH_TRAVERSAL_BLOCKED
```

The attack is blocked.

---

## Step 11: State Persistence - Boot Count

Power cycle the satellite 3 times (unplug, wait 5 seconds, plug back in).

**What you should see:**

Each time you power on, the boot count should increment:
- Boot #3
- Boot #4
- Boot #5

This confirms EEPROM is saving state correctly.

---

## Step 12: Antenna Deployment Simulation

For this test, you need to simulate the antenna deployment:

1. Hold the antenna switch pressed (GPIO 33 should read HIGH)
2. Let the satellite enter DEPLOYING state (after 5 minutes, or use `ForceOperational` to skip)
3. Observe GPIO 27 going HIGH (burn wire activation)
4. Release the antenna switch

**What you should see:**

```
[ANT] Switch released - antenna deployed!
OK:ANTENNA_DEPLOYED|T+00:05:30
```

The burn wire (GPIO 27) should turn OFF immediately when the switch is released.

---

## Step 13: State Persistence - Antenna Memory

After the antenna has been marked as deployed, power cycle the satellite.

**What you should see:**

```
[STATE] Resuming operational state
```

The satellite should skip directly to OPERATIONAL mode and **never** try to deploy the antenna again.

This is critical - we don't want to re-heat the burn wire in space.

---

## Step 14: Beacon Timing - Before Contact

Reset the EEPROM (or use a fresh chip) so the satellite thinks it has never contacted ground.

**What you should see:**

Beacons every **1 minute** with the message:
```
Andar com fe eu vou, que a fe nao costuma faia.
```

---

## Step 15: Beacon Timing - After Contact

Send any valid command (like Ping).

**What you should see:**

The beacon interval changes to **1 hour**. The message changes to:
```
Ainda bem, que agora encontrei voce
```

---

## Step 16: Radiation Protection Status

Send a GetRadStatus command:

```
SAT001-GetRadStatus&@#[HMAC]
```

**What you should see:**

```
RAD:SEU_TOTAL:0|LAST_SCRUB:45s_ago
```

The SEU count should be 0 (no bit flips detected). The scrub should happen every 60 seconds.

---

## Step 17: Watchdog Test

This is optional but recommended. Temporarily modify the code to create an infinite loop:

```cpp
while(true) { } // Intentional hang
```

Upload and run.

**What you should see:**

After 60 seconds, the satellite should automatically restart. You'll see the boot message again.

Don't forget to remove the infinite loop after testing!

---

## Step 18: Seven-Day Soak Test

This is the most important test. Leave the satellite running for 7 days with no manual intervention.

**Setup:**
- Power from battery + charger (simulating solar panel)
- Ground station logging all received beacons
- No touching, no commands, just observe

**Check every day:**

Day 1: Are beacons being received regularly?
Day 2: Is boot count still 1? (no crashes)
Day 3: Any ERROR messages in the log?
Day 4: Is SD card filling up unexpectedly?
Day 5: Are sensor values still reasonable?
Day 6: Send a Ping - does it respond?
Day 7: Send Status - all systems OK?

**Pass criteria:**
- Zero unexpected reboots (boot count stays at 1)
- Zero error messages
- All commands still work on day 7

---

## Final Checklist

Before declaring the satellite ready for flight:

```
[ ] All steps 1-18 passed
[ ] 7-day soak test: zero crashes
[ ] HMAC key is unique (not the default)
[ ] SD card is empty (no test files left)
[ ] Boot count reset to 0 for flight
[ ] Battery fully charged

Tested by: _______________________
Date: _______________

READY FOR FLIGHT: [ ] YES
```

---

## Quick Command Reference

All commands follow this format:
```
SAT001-COMMAND&PATH@DATA#HMAC
```

| Command | Example |
|---------|---------|
| Ping | `SAT001-Ping&@#[HMAC]` |
| Status | `SAT001-Status&@#[HMAC]` |
| Write a name | `SAT001-WriteFile&/names/maria.txt@Maria Silva#[HMAC]` |
| Read a file | `SAT001-ReadFile&/names/maria.txt@#[HMAC]` |
| List files | `SAT001-ListDir&/names@#[HMAC]` |
| Delete file | `SAT001-DeleteFile&/names/maria.txt@#[HMAC]` |
| Get state | `SAT001-GetState&@#[HMAC]` |
| Radiation status | `SAT001-GetRadStatus&@#[HMAC]` |
| Restart | `SAT001-MCURestart&@#[HMAC]` |
| Start image transfer | `SAT001-ImageStart&/image.jpg@40:5120#[HMAC]` |
| Send image chunk | `SAT001-ImageChunk&0@[base64 data]#[HMAC]` |
| End image transfer | `SAT001-ImageEnd&@#[HMAC]` |
| Cancel image | `SAT001-ImageCancel&@#[HMAC]` |
| Image status | `SAT001-ImageStatus&@#[HMAC]` |

---

*Ad Astra*
