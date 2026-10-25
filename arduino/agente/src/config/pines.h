#pragma once

// Elige los pines según la placa. Ninguna capa por encima de la HAL usa #if de plataforma.
#if defined(ARDUINO_ARCH_ESP32)
  #include "pines_esp32s3.h"
#elif defined(ARDUINO_ARCH_AVR)
  #include "pines_uno.h"
#else
  #error "Placa no soportada"
#endif
