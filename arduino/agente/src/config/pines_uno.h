#pragma once

// Arduino UNO R3 (ATmega328P, CH340). Plan B de la demo.

// Pines del L298N (verificado 20/10/2026 con prueba_motores)
// Motor A = llanta derecha, motor B = llanta izquierda.
// Adelante: IN1=H IN2=L (der) e IN3=H IN4=L (izq). No hace falta invertir ninguno.
const uint8_t PIN_DER_EN  = 5;   // ENA, PWM
const uint8_t PIN_DER_AD  = 7;   // IN1, HIGH = adelante
const uint8_t PIN_DER_AT  = 8;   // IN2, HIGH = atrás
const uint8_t PIN_IZQ_EN  = 6;   // ENB, PWM
const uint8_t PIN_IZQ_AD  = 9;   // IN3, HIGH = adelante
const uint8_t PIN_IZQ_AT  = 10;  // IN4, HIGH = atrás

// Sensores TCRT5000 (salida AO), alimentados a 5V.
// Referencia 20/10/2026 en interior: mesa blanca ~30-40, cinta aislante ~450-880, aire ~970 (crudo, 0-1023).
// Ojo: el mousepad negro da ~44 (refleja IR).
const uint8_t PIN_SENSOR_IZQ = A0;
const uint8_t PIN_SENSOR_CEN = A1;
const uint8_t PIN_SENSOR_DER = A2;
const int ADC_MAX = 1023;

const uint8_t PIN_LED = 13;
