#pragma once

// Parámetros del ejecutor. Se recalibran en el sitio sin recompilar: SET <nombre> <valor>, GET, SAVE
// (SAVE los guarda en Preferences; al reiniciar se cargan los guardados).
// Valores por defecto: calibrados el 26/10/2026 en el impreso de prueba (celda de 22 cm, cinta aislante
// sobre las líneas), con 4xAA a los motores y un piso con una leve pendiente hacia el sur.
// Con otra celda, otro piso u otras pilas hay que recalibrar t_celda y t_alinear (docs/HANDOFF.md).
// Los giros (pwm_giro, pwm_freno, giro_*, t_pulso) se recalibraron el 31/10/2026 con el pack 2S de 18650,
// sin laberinto: una vuelta completa a cada lado para que el desnivel del piso no sesgue el antic.

struct Parametros {
  int t_celda   = 424;  // ms para avanzar una celda (a pwm)
  int t_giro    = 400;  // ms para girar 90° en el eje; solo si el giroscopio no responde
  int pwm       = 180;  // PWM de avance (0-255)
  int pwm_giro  = 165;  // PWM con que arranca el giro y de los pulsos de corrección (230 con 4xAA)
  int comp      = 0;    // se suma a la rueda derecha y se resta a la izquierda (si se desvía sin líneas)
  int kp        = 50;   // PWM que se le quita a una rueda cuando un sensor lateral pisa una línea
  int umbral    = 130;  // escala 0-1000: > umbral = negro (blanco ~25-85, cinta aislante ~400-690)
  int t_alinear = 0;    // ms que sigue avanzando después de que el frontal ve la línea de enfrente
                        // (0 por la bajada al sur; con PASO A hacia el este daba ~30)
  int t_freno   = 40;   // ms de freno al final de cada movimiento
  int pausa     = 120;  // ms quieto entre movimientos
  int pwm_freno = 105;  // contramarcha durante t_freno (0 = freno en corto del L298N, que es débil)
  int pwm_aprox = 140;  // PWM en la última celda antes de una pared: llega más lento y frena más cerca
  int pwm_min   = 120;  // PWM mínimo de una rueda al avanzar: debajo, los N20 se paran (zona muerta)
  int t_buscar  = 500;  // ms que sigue buscando la pared si no la vio en el tiempo del tramo (antes: una celda)
  int perdido   = 1;    // si no ve una pared que el mapa dice que está: 1 = parar con ERR, 0 = seguir la ruta
  // Giroscopio (si no responde, los giros vuelven a ser por tiempo con t_giro)
  // grados antes de los 90° en que corta el giro: compensa la inercia (con el control de velocidad es pareja)
  int giro_antic_r = 2;
  int giro_antic_l = 2;
  // Al mismo PWM la velocidad de giro cambia con el pack y el piso (105-300 °/s) y con ella la inercia al cortar.
  // Por eso el giro se controla por velocidad: giro_vel °/s, que bajan a giro_vel_min en los últimos giro_lento
  // grados; el PWM lo pone un PI (arranca en pwm_giro y puede ir en reversa para frenar).
  int giro_vel     = 120;
  int giro_vel_min = 50;
  int giro_lento   = 60;
  int kp_giro      = 6;   // PWM por cada 10 °/s de error de velocidad
  int ki_giro      = 10;  // PWM por segundo por cada °/s de error
  int giro_tol  = 3;    // si al terminar el giro el error es mayor, corrige con pulsos cortos
  int t_pulso   = 40;   // ms de cada pulso de corrección (a pwm_giro)
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
  {"t_buscar", &Parametros::t_buscar},   {"perdido", &Parametros::perdido},
  {"pwm_min", &Parametros::pwm_min},
  {"giro_antic_r", &Parametros::giro_antic_r},
  {"giro_antic_l", &Parametros::giro_antic_l}, {"giro_tol", &Parametros::giro_tol},
  {"giro_vel", &Parametros::giro_vel},         {"giro_vel_min", &Parametros::giro_vel_min},
  {"giro_lento", &Parametros::giro_lento},
  {"kp_giro", &Parametros::kp_giro},           {"ki_giro", &Parametros::ki_giro},
  {"t_pulso", &Parametros::t_pulso},
  {"kr", &Parametros::kr},               {"imu_signo", &Parametros::imu_signo},
};
