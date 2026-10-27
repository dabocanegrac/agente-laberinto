#pragma once

#include <Arduino.h>

// Giroscopio MPU6050 (módulo GY-521) por I2C, sin librerías externas.
// Solo se usa el eje Z: el carro gira en el plano del piso. El módulo va plano, con el chip hacia arriba.
// El rumbo se integra en cada loop; positivo = horario (a la derecha) visto desde arriba.

namespace imu {

bool iniciar(uint8_t sda, uint8_t scl);  // false si no responde en 0x68
bool ok();
uint8_t quienSoy();                      // WHO_AM_I: 0x68 en el MPU6050 (algunos clones dan otro valor)
void calibrar(int muestras = 400);       // con el carro QUIETO: mide el sesgo y pone el rumbo en 0
void actualizar(int signo);              // signo = 1 con el chip hacia arriba, -1 si quedó boca abajo
float rumbo();                           // grados acumulados desde la última calibración
float sesgo();                           // en unidades crudas del sensor

}  // namespace imu
