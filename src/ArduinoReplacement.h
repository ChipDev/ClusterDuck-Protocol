#include <cstdlib>
#include <cstdarg>
#include <cstdio>
#include <new>
#include <optional>
#include <stdint.h>

typedef bool boolean;

int random(int max);

uint64_t millis();

void vTaskDelay(int ms);