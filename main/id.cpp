/*
 * Orbital Temple Satellite - ID Implementation
 * Version: 1.21
 *
 * The satellite ID is a 32-character hex string used to identify
 * commands intended for this specific satellite.
 *
 * NOTE: This ID alone is NOT sufficient for security.
 * All commands must also include a valid HMAC signature.
 */

#include <Arduino.h>
#include "config.h"

void getId() {
    // Satellite ID - unique identifier for this satellite
    // This is used as part of the message addressing
    sat_id = "ab4ec7121663a28e7226dbaa238da777";

    Serial.print("[ID] Satellite ID: ");
    Serial.println(sat_id);
}
