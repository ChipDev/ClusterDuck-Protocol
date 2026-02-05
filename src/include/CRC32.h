#pragma once

#include <cstddef>
#include <cstdint>

class CRC32 {
 public:
  static uint32_t calculate(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < length; ++i) {
      crc ^= data[i];
      for (int j = 0; j < 8; ++j) {
        crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
      }
    }

    return ~crc;
  }
};
