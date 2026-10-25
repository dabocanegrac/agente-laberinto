// Prueba de alimentación (ESP32-S3, fase E4): funciona sin USB, con la batería de 9V al shield.
// Mueve los motores en ciclos con arranques bruscos para provocar picos de corriente.
// LED RGB:
//   azul fijo 5 s  -> arrancó; tiempo para poner el carro con las llantas en el aire
//   verde parpadeando -> corriendo bien
//   rojo fijo      -> el último reinicio fue por brownout (caída de voltaje): la alimentación no aguanta
//   amarillo fijo  -> el último reinicio fue por otra causa distinta al encendido normal

#include <esp_system.h>

const uint8_t PIN_ENA = 7;
const uint8_t PIN_IN1 = 15;
const uint8_t PIN_IN2 = 16;
const uint8_t PIN_IN3 = 17;
const uint8_t PIN_IN4 = 18;
const uint8_t PIN_ENB = 8;
const uint8_t PIN_RGB = 48;

const uint8_t PWM_PICO = 255;
const unsigned long T_MOV_MS = 800;
const unsigned long T_PAUSA_MS = 400;

void led(uint8_t r, uint8_t g, uint8_t b) {
  rgbLedWrite(PIN_RGB, r, g, b);
}

void parar() {
  analogWrite(PIN_ENA, 0);
  analogWrite(PIN_ENB, 0);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
}

// der/izq: +1 adelante, -1 atrás, 0 quieto
void mover(int der, int izq) {
  digitalWrite(PIN_IN1, der > 0);
  digitalWrite(PIN_IN2, der < 0);
  digitalWrite(PIN_IN3, izq > 0);
  digitalWrite(PIN_IN4, izq < 0);
  analogWrite(PIN_ENA, der ? PWM_PICO : 0);
  analogWrite(PIN_ENB, izq ? PWM_PICO : 0);
}

void paso(int der, int izq) {
  mover(der, izq);
  for (unsigned long t = 0; t < T_MOV_MS; t += 100) {
    led(0, (t / 100) % 2 ? 40 : 0, 0);
    delay(100);
  }
  parar();
  delay(T_PAUSA_MS);
}

void setup() {
  uint8_t pines[] = {PIN_ENA, PIN_ENB, PIN_IN1, PIN_IN2, PIN_IN3, PIN_IN4};
  for (uint8_t p : pines) pinMode(p, OUTPUT);
  parar();
  Serial.begin(115200);

  esp_reset_reason_t razon = esp_reset_reason();
  Serial.printf("# reset_reason=%d\n", razon);
  if (razon == ESP_RST_BROWNOUT) {
    led(60, 0, 0);
    Serial.println("# BROWNOUT: la alimentación no aguantó");
    while (true) delay(1000);
  }
  if (razon != ESP_RST_POWERON && razon != ESP_RST_SW && razon != ESP_RST_EXT) {
    led(60, 40, 0);
    Serial.println("# reinicio inesperado");
    while (true) delay(1000);
  }

  led(0, 0, 60);
  delay(5000);
}

void loop() {
  paso(+1, +1);  // adelante
  paso(-1, -1);  // atrás (inversión brusca tras la pausa)
  paso(+1, -1);  // girar a la izquierda en el sitio
  paso(-1, +1);  // girar a la derecha en el sitio
}
