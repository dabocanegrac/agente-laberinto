#include "mpu6050.h"

#include <Wire.h>
#include <math.h>

namespace imu {

namespace {

const uint8_t DIRECCION = 0x68;          // AD0 sin conectar
const float LSB_POR_GRADO_S = 65.5f;     // escala de ±500 °/s
const int MUESTRAS_CAL = 400;            // ~0.4 s a 1 kHz
const float RUIDO_MAX_CAL = 25.0f;       // desviación cruda por encima de esto = el carro se movió (~0.4 °/s)
const int FALLOS_MAX = 25;               // lecturas seguidas fallidas (~50 ms) para dar el sensor por caído
const int CADA_REVISION = 25;            // cada ~50 ms se revisa que el MPU siga despierto
const uint8_t PWR_MGMT_1 = 0x01;         // despierto, reloj del giroscopio X (lo que escribe configurar())

volatile bool hay = false;
volatile bool cayo = false;
volatile bool pausado = false;           // durante calibrar() la tarea no integra
uint8_t whoami = 0;
float sesgoZ = 0;
float ruido = 0;
volatile float rumboGrados = 0;
volatile float velGrados = 0;
unsigned long tAnterior = 0;
int fallos = 0;
const int* signoImu = nullptr;

// Diagnóstico: si el voltaje cae, el MPU se puede reiniciar sin que el I2C falle. Arranca dormido
// (PWR_MGMT_1=0x40), el giroscopio da 0 y el rumbo deriva a -sesgo/65.5 °/s sin que nada lo detecte.
int cuentaRevision = 0;
volatile int nReinicios = 0;
volatile bool reinicio = false;
volatile uint8_t pwrVisto = PWR_MGMT_1;
volatile int16_t zUltimo = 0;
int igualesSeguidas = 0;
volatile int igualesMax = 0;             // lecturas seguidas idénticas: con el MPU vivo el ruido no deja pasar de pocas
volatile unsigned long nFallosI2c = 0;

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

bool configurar() {
  return escribir(0x6B, PWR_MGMT_1)      // PWR_MGMT_1: despierta, reloj del giroscopio X
         && escribir(0x1A, 0x03)         // CONFIG: filtro pasa bajos ~44 Hz (quita vibración de los motores)
         && escribir(0x1B, 0x08);        // GYRO_CONFIG: ±500 °/s
}

void revisarDespierto() {
  if (++cuentaRevision < CADA_REVISION) return;
  cuentaRevision = 0;
  uint8_t p;
  if (!leer(0x6B, &p, 1) || p == PWR_MGMT_1) return;
  pwrVisto = p;
  nReinicios++;
  reinicio = true;
  configurar();
}

bool leerZ(int16_t& z) {
  uint8_t b[2];
  if (!leer(0x47, b, 2)) return false;   // GYRO_ZOUT_H, GYRO_ZOUT_L
  z = (int16_t)((b[0] << 8) | b[1]);
  return true;
}

void actualizar() {
  if (!hay || pausado) return;
  int16_t z;
  if (!leerZ(z)) {
    nFallosI2c++;
    if (++fallos >= FALLOS_MAX) {        // se soltó un cable: mejor giros por tiempo que un rumbo congelado
      hay = false;
      cayo = true;
    }
    return;
  }
  fallos = 0;
  igualesSeguidas = z == zUltimo ? igualesSeguidas + 1 : 0;
  if (igualesSeguidas > igualesMax) igualesMax = igualesSeguidas;
  zUltimo = z;
  revisarDespierto();
  unsigned long t = micros();
  float dt = (t - tAnterior) * 1e-6f;
  tAnterior = t;
  // Con Z hacia arriba, positivo es antihorario; el rumbo se cuenta positivo a la derecha.
  float w = -*signoImu * (z - sesgoZ) / LSB_POR_GRADO_S;
  rumboGrados += w * dt;
  velGrados += 0.1f * (w - velGrados);   // a 500 Hz: constante de ~20 ms
}

void tarea(void*) {
  TickType_t siguiente = xTaskGetTickCount();
  for (;;) {
    actualizar();
    vTaskDelayUntil(&siguiente, pdMS_TO_TICKS(2));
  }
}

}  // namespace

bool iniciar(uint8_t sda, uint8_t scl, const int* signo) {
  signoImu = signo;
  Wire.begin(sda, scl, 400000);
  Wire.setTimeOut(5);                    // si se suelta un cable, que no bloquee
  hay = configurar() && leer(0x75, &whoami, 1);   // WHO_AM_I
  delay(100);                            // el giroscopio tarda en estabilizarse al despertar
  tAnterior = micros();
  // Más prioridad que el loop (1) y en el mismo núcleo: se adelanta al loop cada 2 ms.
  if (hay) xTaskCreatePinnedToCore(tarea, "imu", 3072, nullptr, 3, nullptr, ARDUINO_RUNNING_CORE);
  return hay;
}

bool ok() { return hay; }
uint8_t quienSoy() { return whoami; }
float rumbo() { return rumboGrados; }
float velocidad() { return velGrados; }
float sesgo() { return sesgoZ; }
float ruidoCal() { return ruido; }

bool seCayo() {
  bool c = cayo;
  cayo = false;
  return c;
}

bool seReinicio(uint8_t& pwr) {
  if (!reinicio) return false;
  reinicio = false;
  pwr = pwrVisto;
  return true;
}

void diagnostico(Stream& s) {
  s.printf("OK imu_diag reinicios=%d ult_pwr=0x%02X z=%d iguales_max=%d fallos_i2c=%lu\n", nReinicios, pwrVisto,
           zUltimo, igualesMax, nFallosI2c);
  igualesMax = 0;
}

bool calibrar() {
  if (!hay) return false;
  pausado = true;
  double suma = 0, suma2 = 0;
  int n = 0;
  for (int i = 0; i < MUESTRAS_CAL; i++) {
    int16_t z;
    if (leerZ(z)) {
      suma += z;
      suma2 += (double)z * z;
      n++;
    }
    delayMicroseconds(1000);
  }
  bool quieto = false;
  if (n > MUESTRAS_CAL / 2) {
    float media = suma / n;
    ruido = sqrt(max(0.0, suma2 / n - (double)media * media));
    quieto = ruido <= RUIDO_MAX_CAL;
    if (quieto) sesgoZ = media;          // si se movió, se queda con el sesgo anterior
  }
  rumboGrados = 0;
  tAnterior = micros();
  pausado = false;
  return quieto;
}

}  // namespace imu
