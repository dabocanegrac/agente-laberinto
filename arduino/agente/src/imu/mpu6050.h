#pragma once

#include <Arduino.h>

// Giroscopio MPU6050 (módulo GY-521) por I2C, sin librerías externas.
// Solo se usa el eje Z: el carro gira en el plano del piso. El módulo va plano, con el chip hacia arriba.
// El rumbo se integra en una tarea propia a 500 Hz: nada del loop (Wi-Fi, OTA, consola) lo puede frenar.
// Rumbo positivo = horario (a la derecha) visto desde arriba.

namespace imu {

bool iniciar(uint8_t sda, uint8_t scl, const int* signo);  // signo: puntero a imu_signo (1 o -1)
bool ok();                     // false si no respondió al iniciar o dejó de responder
bool seCayo();                 // true una sola vez cuando deja de responder (para avisarlo en el log)
bool seReinicio(uint8_t& pwr); // true una sola vez cuando se reinició y hubo que reconfigurarlo; pwr = PWR_MGMT_1 visto
void diagnostico(Stream& s);   // una línea "OK imu_diag ..." (y reinicia iguales_max)
uint8_t quienSoy();            // WHO_AM_I: 0x68 en el MPU6050 (algunos clones dan otro valor)
bool calibrar();               // con el carro QUIETO: mide el sesgo y pone el rumbo en 0; false si se movió
float rumbo();                 // grados acumulados desde la última calibración
float velocidad();             // °/s, filtrada (~20 ms), mismo signo que el rumbo
float sesgo();                 // en unidades crudas del sensor
float ruidoCal();              // desviación de la última calibración (crudo); quieto da pocas unidades

}  // namespace imu
