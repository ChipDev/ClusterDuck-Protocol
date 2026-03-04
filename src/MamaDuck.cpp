/**
 * @file MamaDuck.cpp
 * @brief Implements a MamaDuck using the ClusterDuck Protocol (CDP).
 *
 * Periodically sends two types of messages through the CDP mesh:
 *   - Health data (counter + free memory) on topics::health every 10s
 *   - GPS location (lat/lon) on topics::location every 15s, only when fix valid
 *
 * GPS is read from the first responsive UART port (auto-detected).
 * Location messages are withheld until a valid GPS fix is acquired.
 *
 * @date 2025-05-07
 */

#include <string>
#include <unistd.h>
#include "CDP.h"
#include "ArduinoReplacement.h"
#include "ArduinoTimerReplacement.h"
#include "GpsReader.h"

// --- Function Declarations ---
bool runSensor(void*);
bool runGps(void*);

// --- Global Variables ---
MamaDuck duck("UI0NSSOX");
GpsReader GPS;
auto timer = timer_create_default();
const int SENSOR_INTERVAL_MS = 10000;   // Health message every 10 s
const int GPS_INTERVAL_MS    = 15000;   // GPS message every 15 s
int counter = 1;
bool setupOK = false;

/**
 * @brief Setup function to initialize the MamaDuck and GPS reader.
 *
 * - Initializes MamaDuck using default configuration.
 * - Opens the UART GPS device.
 * - Sets up periodic health and GPS transmissions.
 */
void setup() {
    if (duck.setupWithDefaults() != DUCK_ERR_NONE) {
        cdpPrintf("[MAMA] Failed to setup MamaDuck\n");
        return;
    }

    if (!GPS.begin()) {
        // Non-fatal: duck still operates, GPS messages will report FIX:0
        cdpPrintf("[MAMA] Warning: GPS init failed, continuing without GPS\n");
    }

    timer.every(SENSOR_INTERVAL_MS, runSensor);
    timer.every(GPS_INTERVAL_MS,    runGps);

    setupOK = true;
    cdpPrintf("[MAMA] Setup OK!\n");
}

/**
 * @brief Main loop: ticks timers, polls GPS UART, and runs the duck.
 */
void loop() {
    if (!setupOK) return;

    GPS.poll();       // Non-blocking — drains whatever bytes are available
    timer.tick();
    duck.run();
}

int main(int argc, char* argv[]) {
    setup();
    cdpPrintf("[MAMA] Entering main loop...\n");

    while (1) {
        loop();
        usleep(100000); // 100 ms
    }
    return 0;
}

/**
 * @brief Periodically sends health data (counter + free memory).
 *
 * @return true always (keeps the timer repeating)
 */
bool runSensor(void*) {
    std::string message = "C:" + std::to_string(counter) +
                          "|FM:" + std::to_string(freeMemory());
    cdpPrintf("[MAMA] Health: %s\n", message.c_str());

    int err = duck.sendData(topics::health, message);
    if (err == DUCK_ERR_NONE) {
        counter++;
        cdpPrintf("[MAMA] Health sent ok\n");
    } else {
        cdpPrintf("[MAMA] Health send failed: %d\n", err);
    }
    return true;
}

/**
 * @brief Periodically sends GPS location data — only when a valid fix exists.
 *
 * Skips silently if no fix is available. The next timer firing will try again.
 *
 * @return true always (keeps the timer repeating)
 */
bool runGps(void*) {
    if (!GPS.hasFix()) {
        cdpPrintf("[MAMA] GPS: no fix yet, skipping\n");
        return true;
    }

    std::string message = GPS.formatMessage();
    cdpPrintf("[MAMA] GPS: %s\n", message.c_str());

    int err = duck.sendData(topics::gps, message);
    if (err == DUCK_ERR_NONE) {
        cdpPrintf("[MAMA] GPS sent ok\n");
    } else {
        cdpPrintf("[MAMA] GPS send failed: %d\n", err);
    }
    return true;
}