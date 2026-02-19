#ifdef ESP32
#include "../DuckEsp.h"
int freeMemory() {
  return duckesp::freeHeapMemory();
}
#elif defined(__linux__)
#include <fstream>
#include <string>
#include <cstdio>
int freeMemory() {
  std::ifstream meminfo("/proc/meminfo");
  std::string line;
  while (std::getline(meminfo, line)) {
    if (line.find("MemAvailable:") == 0) {
      long kb = 0;
      sscanf(line.c_str(), "MemAvailable: %ld kB", &kb);
      return (int)(kb * 1024);
    }
  }
  return -1;
}
#else
#ifdef __arm__
extern "C" char* sbrk(int incr);
#else
extern char* __brkval;
#endif
int freeMemory() {
  char top;
#ifdef __arm__
  return &top - reinterpret_cast<char*>(sbrk(0));
#elif defined(CORE_TEENSY) || (ARDUINO > 103 && ARDUINO != 151)
  return &top - __brkval;
#else
  return -1;
#endif
}
#endif
