#include "mpu6050.h"

#include <Wire.h>

namespace imu {

namespace {

const uint8_t DIRECCION = 0x68;          // AD0 sin conectar
const float LSB_POR_GRADO_S = 65.5f;     // escala de ±500 °/s

bool hay = false;
uint8_t whoami = 0;
float sesgoZ = 0;
float rumboGrados = 0;
unsigned long tAnterior = 0;

bool escribir(uint8_t reg, uint8_t valor) {
  Wire.beginTransmission(DIRECCION);
  Wire.write(reg);
  Wire.write(valor);
  return Wire.endTransmission() == 0;
}

bool leer(uint8_t reg, uint8_t* buf, size_t n) {
  Wire.beginTransmission(DIRECCION);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint16_t)DIRECCION, n) != n) return false;
  for (size_t i = 0; i < n; i++) buf[i] = Wire.read();
  return true;
}

bool leerZ(int16_t& z) {
  uint8_t b[2];
  if (!leer(0x47, b, 2)) return false;   // GYRO_ZOUT_H, GYRO_ZOUT_L
  z = (int16_t)((b[0] << 8) | b[1]);
  return true;
}

}  // namespace

bool iniciar(uint8_t sda, uint8_t scl) {
  Wire.begin(sda, scl, 400000);
  Wire.setTimeOut(5);                    // si se suelta un cable, que no bloquee el loop
  hay = escribir(0x6B, 0x01)             // PWR_MGMT_1: despierta, reloj del giroscopio X
        && escribir(0x1A, 0x03)          // CONFIG: filtro pasa bajos ~44 Hz (quita vibración de los motores)
        && escribir(0x1B, 0x08)          // GYRO_CONFIG: ±500 °/s
        && leer(0x75, &whoami, 1);       // WHO_AM_I
  delay(100);                            // el giroscopio tarda en estabilizarse al despertar
  tAnterior = micros();
  return hay;
}

bool ok() { return hay; }
uint8_t quienSoy() { return whoami; }
float rumbo() { return rumboGrados; }
float sesgo() { return sesgoZ; }

void calibrar(int muestras) {
  if (!hay) return;
  long suma = 0;
  int n = 0;
  for (int i = 0; i < muestras; i++) {
    int16_t z;
    if (leerZ(z)) {
      suma += z;
      n++;
    }
    delayMicroseconds(1000);
  }
  if (n > 0) sesgoZ = (float)suma / n;
  rumboGrados = 0;
  tAnterior = micros();
}

void actualizar(int signo) {
  if (!hay) return;
  int16_t z;
  if (!leerZ(z)) return;
  unsigned long t = micros();
  float dt = (t - tAnterior) * 1e-6f;
  tAnterior = t;
  // Con Z hacia arriba, positivo es antihorario; el rumbo se cuenta positivo a la derecha.
  rumboGrados -= signo * (z - sesgoZ) / LSB_POR_GRADO_S * dt;
}

}  // namespace imu
