# Orbital Temple

Orbital Temple is an artwork and a satellite, aiming to be a radical inclusive temple, in a place where no wall can reach.

This is the firmware that keeps the promise to carry names beyond the sky, to hold them in orbit, to question the symbolic colonization of heaven as paradise; the origin of the rules that open or close its gateways; and the influence those beliefs have on the decisions we make every day.

---

## What is this?

It's a PocketQube satellite, with 5 × 5 × 5 cm before launch, and 5 × 5 × 30 cm after deploying its antennas in space. Weighs 250 grams of aluminum, gold, and memory.

### How it works

The user thinks of a life that should be remembered. They type the name in [this website](https://orbitaltemple.art).

And we will hold it. When the Orbital Temple aligns with our antennas on Earth, the name will rise. The satellite will store it in its memory and send a message to you: *"Today, at this hour, the name you sent ascended, and there it remains."*

### This repository

This repository contains the code that makes that possible.

The first artistic satellite from the Global South.
A temple with no walls. A cemetery with no ground.

---

## The Architecture of Remembrance

```
┌─────────────────────────────────────────────────────────┐
│                    ORBITAL TEMPLE                       │
│                     Firmware V1.21                      │
├─────────────────────────────────────────────────────────┤
│                                                         │
│   Earth ──── Radio (LoRa) ────► Satellite              │
│              468.5 MHz TX                               │
│              401.5 MHz RX                               │
│                                                         │
│   Names travel at the speed of light.                  │
│   They arrive. They stay. Forever.                     │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## How It Survives

Space is not gentle. Charged particles from distant stars pass through circuits
and flip bits — corrupting memory, erasing names. We built defenses.

| Protection | Purpose |
|------------|---------|
| **Triple Modular Redundancy** | Every critical memory stored three times. Two agree, one is wrong? The two prevail. |
| **CRC32 Checksums** | Mathematical seals on stored data. Corruption is detected. |
| **Watchdog Timer** | If the code freezes, the satellite awakens itself. 60 seconds of silence triggers rebirth. |
| **State Persistence** | The satellite remembers — even through power loss, even through the cold of eclipse. |

The temple protects what it holds.

---

## How It Speaks

The satellite does not shout. It whispers at intervals, conserving energy
for the long silence between passes.

| Condition | Beacon Interval |
|-----------|-----------------|
| Searching for Earth | Every **1 minute** — *"Andar com fé eu vou, que a fé não costuma faiá."* |
| Contact established | Every **1 hour** — *"Ainda bem, que agora encontrei você"* |
| Lost for 24 hours | Every **5 minutes** — *"Por mais distante, o errante navegante, quem jamais te esqueceria?"* |

When the ground station hears, it responds. When the satellite hears back,
it knows it is not alone.

---

## The Commands

All commands are signed with HMAC-SHA256. The temple does not open to strangers.

```
Format: SATELLITE_ID-COMMAND&PATH@DATA#SIGNATURE
```

| Command | What it does |
|---------|--------------|
| `Ping` | The satellite answers: *"I am alive"* |
| `Status` | Returns telemetry — battery, temperature, orientation, light |
| `WriteFile` | Inscribes a name into memory |
| `ReadFile` | Retrieves what was written |
| `GetState` | Reports mission state, boot count, antenna status |
| `GetRadStatus` | Reports radiation events — how many bits the cosmos tried to flip |

---

## The Body

```
├── main.ino        # Where it begins
├── config.h/cpp    # The constants of this universe
├── setup.h/cpp     # The awakening
├── loop.h/cpp      # The eternal cycle
├── lora.h/cpp      # The voice
├── sensors.h/cpp   # The senses
├── memor.h/cpp     # The memory
├── radiation.h/cpp # The shield
└── id.h/cpp        # The name of the temple itself
```

---

## The Numbers

| Specification | Value |
|---------------|-------|
| Dimensions | 50 × 50 × 50 mm |
| Mass | 250 grams |
| Altitude | 525 km |
| Orbit | Sun-synchronous, 97.6° inclination |
| Radio | LoRa SX1262 |
| Processor | ESP32 |
| Storage | SD Card |

---

## Building the Temple

```bash
# Using PlatformIO
pio run

# Using Arduino IDE
# Open main.ino, install libraries, compile
```

**Required Libraries:**
- RadioLib
- ArduinoJson
- SparkFunLSM9DS1
- ESP32 Board Support

---

## Before Flight

- [ ] Generate unique HMAC key (`openssl rand -hex 32`)
- [ ] Update `HMAC_KEY` in `config.cpp`
- [ ] Verify telemetry shows real sensor values
- [ ] Test command authentication
- [ ] Thermal test: -40°C to +85°C
- [ ] 7-day continuous operation test

---

## What This Code Remembers

Every boot is counted. Every radiation event is logged.
Every name, once written, persists.

The satellite will orbit for years, then decades,
slowly descending, until one day it becomes a shooting star —
carrying all those names back to Earth,
a brief light across the sky,
and then silence.

But until then, it listens. It remembers. It keeps the promise.

---


The Orbital Temple is an artwork,<br>
and a satellite.<br>
And although it has "temple" in its name,<br>
it becomes a temple only if<br>
you decide to see it as one.

It exists now as an evocation,<br>
like the one made by the artists<br>
in that primordial cave.<br>
And we are still in a cave,

the vast darkness of space,<br>
where Earth is the torch,<br>
the only place we know<br>
consciousness exists for sure.

The artist is still working,<br>
people are still dancing,<br>
serving the same impulse as our ancestors,<br>
painting, sculpting, designing our desired future,<br>
not to decorate but to invoke,<br>
so a new temple can be born.

And yes,<br>
you are invited to dance.

<a href="https://orbitaltemple.art">orbitaltemple.art</a><br>
Edson Pavoni
