# Wokwi Simulation

Test the Orbital Temple state machine in your browser.

## Quick Start

1. Go to [wokwi.com](https://wokwi.com)
2. Create new ESP32 project
3. Copy `wokwi_test.ino` code
4. Replace diagram.json with ours
5. Click **Run**

## What You'll See

```
=============================================
  ORBITAL TEMPLE - WOKWI SIMULATION
=============================================

[SETUP] Initializing watchdog...
[SETUP] Pins configured
[SETUP] Antenna switch: PRESSED
[SETUP] Battery: 1.65V

[SETUP] Entering main loop...

[STATE] Boot -> Wait Deploy
[STATE] Wait complete -> Deploying
[ANT] Starting burn wire...
```

## Simulated Components

| Component | Pin | Purpose |
|-----------|-----|---------|
| LED (green) | GPIO 27 | Burn wire indicator |
| Button | GPIO 33 | Antenna deploy switch |
| Potentiometer | GPIO 35 | Battery voltage |

## Testing

1. **Watch state transitions** - Boot → Wait → Deploy → Operational
2. **Press the button** - Simulates antenna switch pressing
3. **Release the button** - Triggers "DEPLOYED" message
4. **Adjust potentiometer** - Changes battery voltage reading

## Timing (Accelerated for Testing)

| Phase | Real Satellite | Simulation |
|-------|----------------|------------|
| Wait before deploy | 5 minutes | 5 seconds |
| Burn wire heating | 90 seconds | 3 seconds |
| Cooling | 90 seconds | 2 seconds |

## Limitations

- No LoRa radio simulation
- No SD card (limited)
- No IMU sensor
- Simplified state machine

## Screenshot

When running, the serial monitor shows real-time status updates every 5 seconds, including battery voltage and uptime.
