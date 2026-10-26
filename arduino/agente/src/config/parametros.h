#pragma once

// Parámetros del ejecutor. Se recalibran en el sitio sin recompilar: SET <nombre> <valor>, GET, SAVE
// (SAVE los guarda en Preferences; al reiniciar se cargan los guardados).
// Los valores por defecto son un punto de partida, no están calibrados.

struct Parametros {
  int t_celda   = 800;  // ms para avanzar una celda
  int t_giro    = 400;  // ms para girar 90° en el eje
  int pwm       = 180;  // PWM de avance (0-255)
  int pwm_giro  = 230;  // PWM de los giros: más alto, en el eje los N20 necesitan más torque
  int comp      = 0;    // se suma a la rueda derecha y se resta a la izquierda (si se desvía sin líneas)
  int kp        = 50;   // PWM que se le quita a una rueda cuando un sensor lateral pisa una línea
  int umbral    = 300;  // escala 0-1000: > umbral = negro
  int t_alinear = 250;  // ms que sigue avanzando después de que el frontal ve la línea de enfrente
  int t_freno   = 60;   // ms de freno activo al final de cada movimiento
  int pausa     = 250;  // ms quieto entre movimientos
};

struct DefParametro {
  const char* nombre;
  int Parametros::*campo;
};

const DefParametro DEF_PARAMETROS[] = {
  {"t_celda", &Parametros::t_celda},     {"t_giro", &Parametros::t_giro},
  {"pwm", &Parametros::pwm},             {"pwm_giro", &Parametros::pwm_giro},
  {"comp", &Parametros::comp},           {"kp", &Parametros::kp},
  {"umbral", &Parametros::umbral},       {"t_alinear", &Parametros::t_alinear},
  {"t_freno", &Parametros::t_freno},     {"pausa", &Parametros::pausa},
};
