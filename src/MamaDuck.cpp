#include <string>
#include <unistd.h>
#include "CDP.h"
#include "ArduinoReplacement.h"
#include "ArduinoTimerReplacement.h"

// --- Function Declarations ---
bool runSensor(void *);

// --- Global Variables ---
MamaDuck duck("MAMADUCK"); 
auto timer = timer_create_default();  
const int INTERVAL_MS = 10000;        
int counter = 1;                      
bool setupOK = false;                 

void setup() {
    printf("[MAMA] MamaDuck setup function called.\n");

    if (duck.setupWithDefaults() != DUCK_ERR_NONE) {
        cdpPrintf("[MAMA] Failed to setup MamaDuck\n");
        return;
    }

    timer.every(INTERVAL_MS, runSensor); 
    setupOK = true;
    cdpPrintf("[MAMA] Setup OK!\n");
}

void loop() {
    if (!setupOK) {
        return; 
    }
    
    timer.tick();
    duck.run();
}

/**
 * Cleaned up Main
 */
int main (int argc, char *argv[]) {
    setup();
    printf("[MAMA] Entering main loop...\n");
    
    while(1){
        loop();
        usleep(100000); 
    }
    return 0;
}

bool runSensor(void *) {
    bool success;

    std::string message = "C:" + std::to_string(counter) + "|" + "FM:" + std::to_string(freeMemory());
    cdpPrintf("[MAMA] Generating sensor data: %s\n", message.c_str());

    // sendData returns DUCK_ERR_NONE (0) on success
    int err = duck.sendData(topics::health, message);
    
    if (err == DUCK_ERR_NONE) {
        counter++;
        cdpPrintf("[MAMA] runSensor: Packet sent successfully.\n");
        success = true;
    } else {
        cdpPrintf("[MAMA] runSensor: Send failed with code %d\n", err);
        success = false;
    }
    return success;
}