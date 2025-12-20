# Orbital Temple - Hardware Test Protocol

**Version:** 1.21
**Total time:** ~4.5 hours + 7-day soak test

---

## Before You Start

Make sure you have:
- The satellite with battery charged
- Ground station computer with radio
- USB cable for serial monitoring
- Fresh SD card (FAT32 formatted)
- Multimeter (optional)

And confirm:
- You created `secrets.h` from `secrets.h.example` with your unique HMAC key
- Code compiled without errors

---

## Copy-Paste Test Commands

**IMPORTANT:** Replace `[HMAC]` with your calculated HMAC signature for each command.

Your satellite ID: `_______________` (fill in before testing)

### All Test Commands (in order)

```
# Step 4: Ping Test
SAT001-Ping&@#[HMAC]

# Step 5: Status/Telemetry
SAT001-Status&@#[HMAC]

# Step 6: Write File
SAT001-WriteFile&/test.txt@Hello from Earth#[HMAC]

# Step 7: Read File
SAT001-ReadFile&/test.txt@#[HMAC]

# Step 8: Delete File
SAT001-DeleteFile&/test.txt@#[HMAC]

# Step 9: Artwork Ascension
SAT001-artworkAscension&@QmTestCID12345678901234567890|Test Artist|Test Artwork#[HMAC]

# Step 9: List Artworks
SAT001-artworkList&@#[HMAC]

# Step 10: Wrong HMAC (should fail)
SAT001-Ping&@#wrongsignature123

# Step 11: Path Traversal (should fail)
SAT001-ReadFile&/../../../etc/passwd@#[HMAC]

# Step 13: Force Operational (skip antenna wait)
SAT001-ForceOperational&@#[HMAC]

# Step 16: Ping (to trigger contact)
SAT001-Ping&@#[HMAC]

# Step 17: Radiation Status
SAT001-GetRadStatus&@#[HMAC]

# Day 6 Soak Test: Ping
SAT001-Ping&@#[HMAC]

# Day 7 Soak Test: Status
SAT001-Status&@#[HMAC]
```

### Additional Useful Commands

```
# Get satellite state
SAT001-GetState&@#[HMAC]

# List directory
SAT001-ListDir&/@#[HMAC]

# List names directory
SAT001-ListDir&/names@#[HMAC]

# Write a name
SAT001-WriteFile&/names/test.txt@Test Name#[HMAC]

# Restart satellite
SAT001-MCURestart&@#[HMAC]

# Start accelerometer recording
SAT001-AccelRecord&@#[HMAC]

# List accelerometer recordings
SAT001-AccelList&@#[HMAC]

# Get accelerometer status
SAT001-AccelStatus&@#[HMAC]
```

### Cleanup Commands (after testing)

```
# Delete test file
SAT001-DeleteFile&/test.txt@#[HMAC]

# Delete test artwork log (if needed)
SAT001-DeleteFile&/artworks.log@#[HMAC]

# Delete test names
SAT001-DeleteFile&/names/test.txt@#[HMAC]
```

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

## Step 9: Artwork Ascension

Send an artworkAscension command:

```
SAT001-artworkAscension&@QmTestCID12345678901234567890|Test Artist|Test Artwork#[HMAC]
```

**What you should see:**

```
OK:ART_STORED|QmTestCID12345678901234567890
```

Then verify with artworkList:

```
SAT001-artworkList&@#[HMAC]
```

**What you should see:**

```
ART:LIST_START
ART:1|T+00:XX:XX|QmTestCID12345678901234567890|Test Artist|Test Artwork
ART:LIST_END|COUNT:1
```

---

## Step 10: Security - Wrong HMAC

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

## Step 11: Security - Path Traversal

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

## Step 12: State Persistence - Boot Count

Power cycle the satellite 3 times (unplug, wait 5 seconds, plug back in).

**What you should see:**

Each time you power on, the boot count should increment:
- Boot #3
- Boot #4
- Boot #5

This confirms EEPROM is saving state correctly.

---

## Step 13: Antenna Deployment Simulation

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

## Step 14: State Persistence - Antenna Memory

After the antenna has been marked as deployed, power cycle the satellite.

**What you should see:**

```
[STATE] Resuming operational state
```

The satellite should skip directly to OPERATIONAL mode and **never** try to deploy the antenna again.

This is critical - we don't want to re-heat the burn wire in space.

---

## Step 15: Beacon Timing - Before Contact

Reset the EEPROM (or use a fresh chip) so the satellite thinks it has never contacted ground.

**What you should see:**

Beacons every **4 minutes** with the message:
```
Andar com fe eu vou, que a fe nao costuma faia.|T+00:04:00|B:1|C:NO|V:3.8
```

**Debug: Beacon Countdown**

Every 5 minutes, you'll see a countdown status box on serial:
```
╔══════════════════════════════════════════════════════════╗
║             BEACON COUNTDOWN STATUS                      ║
╠══════════════════════════════════════════════════════════╣
║ Mode: SEARCHING     Contact: NO                          ║
║ Interval: 4 min                                          ║
║ Time since last beacon: 02:30                            ║
║ Time until next beacon: 01:30                            ║
║ Next message: Andar com fe...                            ║
╚══════════════════════════════════════════════════════════╝
```

---

## Step 16: Beacon Timing - After Contact

Send any valid command (like Ping).

**What you should see:**

1. The satellite responds `PONG|T+...`
2. Serial shows: `[BEACON] First ground contact established!`
3. The beacon interval changes to **1 hour**
4. The message changes to: `Ainda bem, que agora encontrei voce`

**Debug: Verify Contact Status**

After sending the Ping, wait for the next countdown box (up to 5 min):
```
╔══════════════════════════════════════════════════════════╗
║             BEACON COUNTDOWN STATUS                      ║
╠══════════════════════════════════════════════════════════╣
║ Mode: CONNECTED     Contact: YES                         ║
║ Interval: 60 min                                         ║
║ Time since last beacon: 05:00                            ║
║ Time until next beacon: 55:00                            ║
║ Next message: Ainda bem...                               ║
╚══════════════════════════════════════════════════════════╝
```

**If Contact: NO after sending Ping:**
- Check if HMAC was correct (should see `[MSG] Valid message received`)
- Look for `[BEACON] First ground contact established!` in serial log
- Send `SAT001-GetState&@#[HMAC]` to check current state

**Troubleshooting: Beacon not received after 1 hour**

If Mode is CONNECTED but you don't receive a beacon:
1. Check the countdown is reaching 00:00
2. Look for `[BEACON] >>> INTERVAL REACHED - SENDING BEACON NOW <<<`
3. Check battery (beacon is skipped if < 3.3V)
4. Verify radio is on TX frequency (468.5 MHz)

---

## Step 17: Radiation Protection Status

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

## Step 18: Watchdog Test

This is optional but recommended. Temporarily modify the code to create an infinite loop:

```cpp
while(true) { } // Intentional hang
```

Upload and run.

**What you should see:**

After 60 seconds, the satellite should automatically restart. You'll see the boot message again.

Don't forget to remove the infinite loop after testing!

---

## Step 19: Seven-Day Soak Test

This is the most important test. Leave the satellite running for 7 days with no manual intervention.

**Setup:**
- Power from battery + charger (simulating solar panel)
- Ground station logging all received beacons
- Serial monitor connected (to capture hourly/daily logs)
- No touching, no commands, just observe

---

### Automatic Debug Logging (NEW)

The satellite now logs comprehensive status automatically:

**Every 1 Hour - Hourly Status:**
```
╔═══════════════════════════════════════════════════════════════╗
║              SOAK TEST - HOURLY STATUS                        ║
╠═══════════════════════════════════════════════════════════════╣
║ Uptime: 1d 05:30:00                                           ║
║ Boot Count: 1       Free Heap: 245000 bytes                   ║
╠═══════════════════════════════════════════════════════════════╣
║ Beacons Sent: 28       Skipped (low bat): 0                   ║
║ Commands OK: 5         Failed: 0                              ║
║ TX Errors: 0           RX Errors: 0                           ║
║ Radio Resets: 0                                               ║
╠═══════════════════════════════════════════════════════════════╣
║ Battery: 3.85V   Temp: 25.3C   Contact: YES                   ║
║ IMU: OK   SD: OK   RF: OK                                     ║
╚═══════════════════════════════════════════════════════════════╝
```

**Every 24 Hours - Daily Summary:**
```
╔═══════════════════════════════════════════════════════════════╗
║         *** SOAK TEST - DAILY SUMMARY ***                     ║
║                    DAY 3 COMPLETE                              ║
╠═══════════════════════════════════════════════════════════════╣
║ Total Uptime: 3d 00:00:00                                     ║
║ Boot Count: 1     (should be 1 for clean test)                ║
║ Free Heap: 243000 bytes                                       ║
╠═══════════════════════════════════════════════════════════════╣
║ COMMUNICATION STATS:                                          ║
║   Beacons Sent: 72                                            ║
║   Beacons Skipped: 0      (low battery)                       ║
║   Commands Received: 0                                        ║
║   Commands Failed: 0                                          ║
╠═══════════════════════════════════════════════════════════════╣
║ ERROR COUNTS:                                                 ║
║   TX Errors: 0                                                ║
║   RX Errors: 0                                                ║
║   Radio Resets: 0                                             ║
╠═══════════════════════════════════════════════════════════════╣
║ HEALTH: Battery=3.82V Temp=24.1C                              ║
║ STATUS: HEALTHY ✓                                             ║
╚═══════════════════════════════════════════════════════════════╝
```

**SD Card Logging:**

All hourly and daily logs are also saved to `/log.txt` on the SD card. Even if you lose serial connection, you can read the log after the test:

```
SAT001-ReadFile&/log.txt@#[HMAC]
```

---

### What to Monitor

| Day | Check | Expected |
|-----|-------|----------|
| 1 | Hourly logs appearing? | Yes, every hour |
| 2 | Boot Count | Still 1 |
| 3 | Free Heap | Not decreasing significantly |
| 4 | TX/RX Errors | Zero or very low |
| 5 | Beacons Sent | Increasing (24/day if connected) |
| 6 | Send Ping | Responds with PONG |
| 7 | Send Status | All sensors OK |

---

### Red Flags (Stop Test & Investigate)

- **Boot Count > 1**: Satellite crashed and restarted
- **Free Heap < 50000**: Possible memory leak
- **STATUS: CHECK REQUIRED**: Multiple errors detected
- **TX Errors > 10**: Radio TX problem
- **RX Errors > 10**: Radio RX problem
- **Radio Resets > 0**: Radio needed recovery
- **No hourly logs**: Satellite may have frozen

---

### Pass Criteria

```
[ ] Boot Count stayed at 1 (zero crashes)
[ ] STATUS: HEALTHY on all daily summaries
[ ] TX Errors: 0
[ ] RX Errors: 0
[ ] Radio Resets: 0
[ ] Commands work on Day 6-7
[ ] Free Heap stable (no significant decrease)
```

---

## Final Checklist

Before declaring the satellite ready for flight:

```
[ ] All steps 1-19 passed
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
| Artwork ascension | `SAT001-artworkAscension&@QmCID...\|Artist Name\|Work Title#[HMAC]` |
| List artworks | `SAT001-artworkList&@#[HMAC]` |

---

*Ad Astra*
