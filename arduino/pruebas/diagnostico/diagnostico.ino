// Diagnóstico del circuito sin mover motores:
// LED 13 parpadea, mide Vcc con la referencia interna de 1.1V y lee los 3 sensores.
// Formato compatible con el Serial Plotter (115200 baudios).

const uint8_t PINES_MOTOR[] = {5, 6, 7, 8, 9, 10};  // ENA, ENB, IN1..IN4

long leerVccMv() {
  // Referencia AVcc, entrada = bandgap interno de 1.1V
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2);
  ADCSRA |= _BV(ADSC);
  while (bit_is_set(ADCSRA, ADSC)) {}
  long adc = ADCL;
  adc |= ADCH << 8;
  return 1125300L / adc;  // 1.1 * 1023 * 1000
}

void setup() {
  for (uint8_t p : PINES_MOTOR) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);  // motores apagados
  }
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);
  Serial.println(F("# diagnostico listo"));
}

void loop() {
  static bool led = false;
  led = !led;
  digitalWrite(LED_BUILTIN, led);

  long vcc = leerVccMv();
  analogRead(A0);  // lectura descartada tras cambiar la referencia del ADC
  int izq = analogRead(A0);
  int cen = analogRead(A1);
  int der = analogRead(A2);

  Serial.print(F("vcc_mV:")); Serial.print(vcc);
  Serial.print(F(",izq:"));   Serial.print(izq);
  Serial.print(F(",cen:"));   Serial.print(cen);
  Serial.print(F(",der:"));   Serial.println(der);
  delay(250);
}
