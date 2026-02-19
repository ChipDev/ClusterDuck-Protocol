#include "ArduinoReplacement.h"
#include <time.h>
#include <stdint.h>
#include <unistd.h>

int random(int max) {
    return rand() % max;
}

uint64_t millis() {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC_RAW, &t);  // change CLOCK_MONOTONIC_RAW to CLOCK_MONOTONIC on non linux computers
  return t.tv_sec * 1000 + (t.tv_nsec + 500000) / 1000000;
}

//FreeRTOS replacement
void vTaskDelay(int ms) { 
  usleep(ms * 1000);
}