/**
 * @file MamaDuck.cpp
 * @brief Implements a MamaDuck using the ClusterDuck Protocol (CDP).
 * 
 * This example firmware periodically sends sensor health data (counter and free memory) 
 * through a CDP mesh network. It also relays messages that it receives from other ducks
 * that has not seen yet.
 * 
 * @date 2025-05-07
 */

#include <string>
#include "CDP.h"
#include "ArduinoReplacement.h"
#include "ArduinoTimerReplacement.h"
#ifdef SERIAL_PORT_USBVIRTUAL
#define Serial SERIAL_PORT_USBVIRTUAL
#endif

// --- Function Declarations ---
bool runSensor(void *);

// --- Global Variables ---
MamaDuck duck("MAMADUCK"); // Device ID, MUST be 8 bytes and unique from other ducks;
auto timer = timer_create_default();  // Creating a timer with default settings
const int INTERVAL_MS = 10000;        // Interval in milliseconds between runSensor call
int counter = 1;                      // Counter for the sensor data  
bool setupOK = false;                 // Flag to check if setup is complete

/**
 * @brief Setup function to initialize the MamaDuck
 *
 * - Sets up the Duck device ID (exactly 8 bytes).
 * - Initializes MamaDuck using default configuration.
 * - Sets up periodic execution of sensor data transmissions.
 */
void setup() {

    if (duck.setupWithDefaults() != DUCK_ERR_NONE) {
        cdpPrintf("[MAMA] Failed to setup MamaDuck\n");
        return;
    }

    timer.every(INTERVAL_MS, runSensor); // Triggers runSensor every INTERVAL_MS

    setupOK = true;
    cdpPrintf("[MAMA] Setup OK!\n");
}

/**
 * @brief Main loop runs continuously.
 *
 * Executes scheduled tasks and maintains Duck operation.
 */
void loop() {
    if (!setupOK) {
        return; 
    }
    timer.tick();

    duck.run();
}

/**
 * @brief Periodically executed to gather and send health data.
 *
 * Collects the current counter value and available free memory, formats them
 * into a string, and transmits this data via CDP.
 *
 * @param unused Unused parameter required by the timer callback signature
 * @return true if data was successfully sent, false otherwise
 */
bool runSensor(void *) {
    bool failure;

    std::string message = "C:" + std::to_string(counter) + "|" + "FM:" + std::to_string(freeMemory());
    cdpPrintf("[MAMA] sensor data: ");
    cdpPrintf("%s\n", message.c_str());

    failure = duck.sendData(topics::health, message);
    if (!failure) {
        counter++;
        cdpPrintf("[MAMA] runSensor ok.\n");
    } else {
        cdpPrintf("[MAMA] runSensor failed.\n");
    }
    return true;
}


int main (int argc, char *argv[]) {
    setup();
    loop();

    return 0;
}
