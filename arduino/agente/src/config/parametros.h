#pragma once

// Valores por defecto. Se recalibran en el sitio (SET ... / SAVE) sin recompilar.
const unsigned long T_CELDA_MS  = 800;  // TODO calibrar
const unsigned long T_GIRO90_MS = 400;  // TODO calibrar
const uint8_t PWM_BASE = 180;           // TODO calibrar
const int UMBRAL_NEGRO = 300;           // escala normalizada 0-1000: > umbral = negro. TODO calibrar en el sitio
