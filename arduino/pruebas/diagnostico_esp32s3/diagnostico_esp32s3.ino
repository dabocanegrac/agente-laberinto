// Diagnóstico de sensores en la ESP32-S3: no toca los pines de los motores.
// Correrlo con las pilas de los motores desconectadas.
// LED RGB parpadea, lee los 3 sensores (crudo 0-4095 y normalizado 0-1000).
// Formato compatible con el Serial Plotter (115200 baudios).

const uint8_t PIN_SENSOR_IZQ = 4;  // ADC1
const uint8_t PIN_SENSOR_CEN = 5;  // ADC1
const uint8_t PIN_SENSOR_DER = 6;  // ADC1
const uint8_t PIN_RGB = 48;
const int ADC_MAX = 4095;

int normalizar(int crudo) {
  return (long)crudo * 1000 / ADC_MAX;
}

void setup() {
  analogReadResolution(12);
  Serial.begin(115200);
  delay(500);
  Serial.println("# diagnostico_esp32s3 listo");
}

void loop() {
  static bool led = false;
  led = !led;
  rgbLedWrite(PIN_RGB, 0, led ? 40 : 0, 0);

  int izq = analogRead(PIN_SENSOR_IZQ);
  int cen = analogRead(PIN_SENSOR_CEN);
  int der = analogRead(PIN_SENSOR_DER);

  Serial.printf("izq:%d,cen:%d,der:%d,izq_n:%d,cen_n:%d,der_n:%d\n",
                izq, cen, der, normalizar(izq), normalizar(cen), normalizar(der));
  delay(250);
}
