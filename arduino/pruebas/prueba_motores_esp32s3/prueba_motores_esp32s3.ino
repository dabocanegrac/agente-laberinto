// Prueba de motores (ESP32-S3): cada motor 1 s adelante y 1 s atrás, por separado.
// No se mueve nada hasta recibir 'g' por Serial (115200 baudios).
// Sirve para identificar cuál motor es el izquierdo/derecho y su sentido de giro.

const uint8_t PIN_ENA = 7;
const uint8_t PIN_IN1 = 15;
const uint8_t PIN_IN2 = 16;
const uint8_t PIN_IN3 = 17;
const uint8_t PIN_IN4 = 18;
const uint8_t PIN_ENB = 8;

const uint8_t PWM_PRUEBA = 180;
const unsigned long T_MOV_MS = 1000;
const unsigned long T_PAUSA_MS = 1500;

void parar() {
  analogWrite(PIN_ENA, 0);
  analogWrite(PIN_ENB, 0);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
}

void mover(uint8_t en, uint8_t inA, uint8_t inB, bool adelante) {
  digitalWrite(inA, adelante ? HIGH : LOW);
  digitalWrite(inB, adelante ? LOW : HIGH);
  analogWrite(en, PWM_PRUEBA);
  delay(T_MOV_MS);
  parar();
  delay(T_PAUSA_MS);
}

void setup() {
  uint8_t pines[] = {PIN_ENA, PIN_ENB, PIN_IN1, PIN_IN2, PIN_IN3, PIN_IN4};
  for (uint8_t p : pines) pinMode(p, OUTPUT);
  parar();
  Serial.begin(115200);
  Serial.println(F("# prueba_motores lista, envia 'g' para empezar"));
}

void loop() {
  if (Serial.read() != 'g') return;

  Serial.println(F(">> Motor A (ENA, IN1=H IN2=L) 'adelante'"));
  mover(PIN_ENA, PIN_IN1, PIN_IN2, true);
  Serial.println(F(">> Motor A (ENA, IN1=L IN2=H) 'atras'"));
  mover(PIN_ENA, PIN_IN1, PIN_IN2, false);
  Serial.println(F(">> Motor B (ENB, IN3=H IN4=L) 'adelante'"));
  mover(PIN_ENB, PIN_IN3, PIN_IN4, true);
  Serial.println(F(">> Motor B (ENB, IN3=L IN4=H) 'atras'"));
  mover(PIN_ENB, PIN_IN3, PIN_IN4, false);
  Serial.println(F("# fin"));
}
