#pragma once

// Pines del L298N
const uint8_t PIN_ENA = 5;   // PWM motor A
const uint8_t PIN_IN1 = 7;
const uint8_t PIN_IN2 = 8;
const uint8_t PIN_IN3 = 9;
const uint8_t PIN_IN4 = 10;
const uint8_t PIN_ENB = 6;   // PWM motor B

// Sensores TCRT5000 (salida AO)
const uint8_t PIN_SENSOR_IZQ = A0;
const uint8_t PIN_SENSOR_CEN = A1;
const uint8_t PIN_SENSOR_DER = A2;

// Constantes a calibrar en el sitio
const unsigned long T_CELDA_MS  = 800;  // TODO calibrar
const unsigned long T_GIRO90_MS = 400;  // TODO calibrar
const uint8_t PWM_BASE = 180;           // TODO calibrar
