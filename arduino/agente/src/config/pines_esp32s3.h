#pragma once

// ESP32-S3 N16R8 de 44 pines sobre el shield de expansión (jack DC de 6.5-9V).
// No usar: GPIO 35-37 (PSRAM), 0/3/45/46 (arranque), 19/20 (USB nativo), 43/44 (UART).

// Pines del L298N (verificado 23/10/2026 con prueba_motores_esp32s3)
// Motor A = llanta derecha, motor B = llanta izquierda.
// Adelante: IN1=H IN2=L (der) e IN3=H IN4=L (izq). No hace falta invertir ninguno.
const uint8_t PIN_DER_EN  = 7;   // ENA, PWM
const uint8_t PIN_DER_AD  = 15;  // IN1, HIGH = adelante
const uint8_t PIN_DER_AT  = 16;  // IN2, HIGH = atrás
const uint8_t PIN_IZQ_EN  = 8;   // ENB, PWM
const uint8_t PIN_IZQ_AD  = 17;  // IN3, HIGH = adelante
const uint8_t PIN_IZQ_AT  = 18;  // IN4, HIGH = atrás

// Sensores TCRT5000 (salida AO) en ADC1, alimentados a 3.3V (filas G/V/S del shield).
// Referencia 23/10/2026 (normalizado 0-1000): blanco ~35-57, cinta aislante ~250-860, aire 1000.
const uint8_t PIN_SENSOR_IZQ = 4;
const uint8_t PIN_SENSOR_CEN = 5;
const uint8_t PIN_SENSOR_DER = 6;
const int ADC_MAX = 4095;

const uint8_t PIN_LED = 48;  // RGB (WS2812)
