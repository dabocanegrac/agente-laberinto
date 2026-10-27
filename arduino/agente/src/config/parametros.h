#pragma once

// Parámetros del ejecutor. Se recalibran en el sitio sin recompilar: SET <nombre> <valor>, GET, SAVE
// (SAVE los guarda en Preferences; al reiniciar se cargan los guardados).
// Valores por defecto: calibrados el 26/10/2026 en el impreso de prueba (celda de 22 cm, cinta aislante
// sobre las líneas), con 4xAA a los motores y un piso con una leve pendiente hacia el sur.
// Con otra celda, otro piso u otras pilas hay que recalibrar t_celda y t_alinear (docs/HANDOFF.md).

struct Parametros {
  int t_celda   = 536;  // ms para avanzar una celda (a pwm)
  int t_giro    = 400;  // ms para girar 90° en el eje; solo si el giroscopio no responde
  int pwm       = 180;  // PWM de avance (0-255)
  int pwm_giro  = 230;  // PWM de los giros: más alto, en el eje los N20 necesitan más torque (con 170 no giran)
  int comp      = 0;    // se suma a la rueda derecha y se resta a la izquierda (si se desvía sin líneas)
  int kp        = 50;   // PWM que se le quita a una rueda cuando un sensor lateral pisa una línea
  int umbral    = 200;  // escala 0-1000: > umbral = negro (blanco ~25-85, cinta aislante ~400-690)
  int t_alinear = 0;    // ms que sigue avanzando después de que el frontal ve la línea de enfrente
                        // (0 por la bajada al sur; con PASO A hacia el este daba ~30)
  int t_freno   = 40;   // ms de freno al final de cada movimiento
  int pausa     = 120;  // ms quieto entre movimientos
  int pwm_freno = 150;  // contramarcha durante t_freno (0 = freno en corto del L298N, que es débil)
  int pwm_aprox = 140;  // PWM en la última celda antes de una pared: llega más lento y frena más cerca
  // Giroscopio (si no responde, los giros vuelven a ser por tiempo con t_giro)
  // grados antes de los 90° en que corta el giro: compensa la inercia, que no es igual hacia cada lado
  int giro_antic_r = 6;
  int giro_antic_l = 6;
  int giro_tol  = 4;    // si al terminar el giro el error es mayor, corrige con pulsos cortos
  int t_pulso   = 60;   // ms de cada pulso de corrección (a pwm_giro)
  int kr        = 8;    // PWM por grado de desvío del rumbo al avanzar (0 = no corregir con el giroscopio)
  int imu_signo = 1;    // 1 = chip hacia arriba; -1 si quedó boca abajo (el rumbo crece al revés)
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
  {"pwm_freno", &Parametros::pwm_freno}, {"pwm_aprox", &Parametros::pwm_aprox},
  {"giro_antic_r", &Parametros::giro_antic_r},
  {"giro_antic_l", &Parametros::giro_antic_l}, {"giro_tol", &Parametros::giro_tol},
  {"t_pulso", &Parametros::t_pulso},
  {"kr", &Parametros::kr},               {"imu_signo", &Parametros::imu_signo},
};
