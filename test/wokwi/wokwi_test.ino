/*
 * Orbital Temple - Wokwi Simulation Test
 *
 * This is a simplified version for testing in Wokwi simulator.
 * https://wokwi.com
 *
 * WHAT THIS TESTS:
 * - State machine transitions
 * - Watchdog timer
 * - Antenna deployment logic
 * - Serial output
 *
 * WHAT THIS CANNOT TEST:
 * - LoRa radio (not supported in Wokwi)
 * - SD card operations (limited support)
 * - IMU sensor (not connected)
 *
 * HOW TO USE:
 * 1. Go to https://wokwi.com
 * 2. Create new ESP32 project
 * 3. Copy this code
 * 4. Copy diagram.json for wiring
 * 5. Click Run
 */

#include <esp_task_wdt.h>

// Pin definitions (matching satellite hardware)
#define AntSwitch 33    // Antenna deployment switch
#define R1 27           // Burn wire relay
#define VBAT_DR 35      // Battery voltage

// Watchdog configuration
#define WDT_TIMEOUT_SECONDS 10  // Shorter for testing

// State machine
enum MissionState {
    STATE_BOOT,
    STATE_WAIT_DEPLOY,
    STATE_DEPLOYING,
    STATE_OPERATIONAL
};

MissionState currentState = STATE_BOOT;
unsigned long stateStartTime = 0;
bool antennaDeployed = false;

// Timing (shorter for simulation)
#define DEPLOY_WAIT_TIME 5000   // 5 seconds instead of 5 minutes
#define DEPLOY_HEAT_TIME 3000   // 3 seconds
#define DEPLOY_COOL_TIME 2000   // 2 seconds

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=============================================");
    Serial.println("  ORBITAL TEMPLE - WOKWI SIMULATION");
    Serial.println("=============================================");
    Serial.println();

    // Initialize watchdog
    Serial.println("[SETUP] Initializing watchdog...");
    esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);
    esp_task_wdt_add(NULL);

    // Configure pins
    pinMode(AntSwitch, INPUT_PULLUP);
    pinMode(R1, OUTPUT);
    digitalWrite(R1, LOW);
    pinMode(VBAT_DR, INPUT);

    Serial.println("[SETUP] Pins configured");
    Serial.println("[SETUP] Antenna switch: " + String(digitalRead(AntSwitch) ? "PRESSED" : "RELEASED"));

    // Read battery voltage (simulated by potentiometer)
    int rawADC = analogRead(VBAT_DR);
    float voltage = (rawADC / 4095.0) * 3.3 * 2;  // Voltage divider
    Serial.println("[SETUP] Battery: " + String(voltage) + "V");

    Serial.println();
    Serial.println("[SETUP] Entering main loop...");
    Serial.println();

    stateStartTime = millis();
}

void loop() {
    unsigned long now = millis();

    // Feed watchdog
    esp_task_wdt_reset();

    // State machine
    switch (currentState) {
        case STATE_BOOT:
            Serial.println("[STATE] Boot -> Wait Deploy");
            currentState = STATE_WAIT_DEPLOY;
            stateStartTime = now;
            break;

        case STATE_WAIT_DEPLOY:
            if (now - stateStartTime >= DEPLOY_WAIT_TIME) {
                Serial.println("[STATE] Wait complete -> Deploying");
                currentState = STATE_DEPLOYING;
                stateStartTime = now;

                // Start burn wire
                Serial.println("[ANT] Starting burn wire...");
                digitalWrite(R1, HIGH);
            }
            break;

        case STATE_DEPLOYING:
            // Check if antenna deployed (switch released)
            if (digitalRead(AntSwitch) == LOW) {
                Serial.println("[ANT] Switch released - DEPLOYED!");
                digitalWrite(R1, LOW);
                antennaDeployed = true;
                currentState = STATE_OPERATIONAL;
                stateStartTime = now;
            }
            // Timeout check
            else if (now - stateStartTime >= DEPLOY_HEAT_TIME) {
                Serial.println("[ANT] Heat time complete, stopping");
                digitalWrite(R1, LOW);

                // Check again
                if (digitalRead(AntSwitch) == LOW) {
                    Serial.println("[ANT] Deployed after cooling!");
                    antennaDeployed = true;
                }

                currentState = STATE_OPERATIONAL;
                stateStartTime = now;
            }
            break;

        case STATE_OPERATIONAL:
            // Normal operation - print status every 5 seconds
            static unsigned long lastStatus = 0;
            if (now - lastStatus >= 5000) {
                int rawADC = analogRead(VBAT_DR);
                float voltage = (rawADC / 4095.0) * 3.3 * 2;

                Serial.println("----------------------------------------");
                Serial.println("STATUS @ T+" + String((now - stateStartTime) / 1000) + "s");
                Serial.println("  State: OPERATIONAL");
                Serial.println("  Antenna: " + String(antennaDeployed ? "DEPLOYED" : "PENDING"));
                Serial.println("  Battery: " + String(voltage, 2) + "V");
                Serial.println("  Uptime: " + String(now / 1000) + "s");
                Serial.println("----------------------------------------");

                lastStatus = now;
            }
            break;
    }

    delay(100);  // Small delay for simulation
}
