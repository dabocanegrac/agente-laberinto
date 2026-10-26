#pragma once

// El firmware es para la ESP32-S3 N16R8 (DEC-06).
#if !defined(ARDUINO_ARCH_ESP32)
  #error "Este firmware es para la ESP32-S3. En el IDE: placa ESP32S3 Dev Module"
#endif
#include "pines_esp32s3.h"
