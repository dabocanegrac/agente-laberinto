// Firmware del agente (ESP32-S3): recibe el mapa, calcula la ruta a bordo con BFS y la recorre.
//
// Comandos, una línea cada uno, por Serial (115200) o por TCP en 192.168.4.1:23 (red agente-laberinto):
//   MAPA              empieza a recibir un mapa (formato de grilla, ver src/planner/mapa.h)
//   ...líneas...      inicio, meta y filas de la grilla
//   FIN               termina el mapa: lo valida, lo guarda y calcula la ruta
//   PLAN              vuelve a calcular la ruta con el mapa guardado
//   RUTA              imprime la última ruta
//   COSTO_GIRO <v>    costo de L/R relativo a F (por defecto 0.5) y vuelve a planear
//   GO                recorre la ruta. El carro arranca con el eje en el centro de la celda de inicio
//   PASO F [n]        avanza n celdas (por tiempo); PASO L / PASO R gira 90°;
//   PASO A            avanza hasta ver una línea con el frontal y se alinea (para calibrar t_alinear)
//   SET <p> <v>       cambia un parámetro (ver src/config/parametros.h); GET los muestra; SAVE los guarda
//   SENS              lectura de los 3 sensores (0-1000, > umbral = negro); SENS ON / SENS OFF continuo
//   IMU               rumbo del giroscopio; IMU ON / IMU OFF continuo; IMU CAL lo calibra (carro quieto)
//   RED_SSID <nombre> / RED_CLAVE <clave>   hotspot al que se conecta además de su red propia
//   RED               estado del hotspot e IP; RED CONECTAR (re)conecta; RED BORRAR lo olvida
//   INFO              estado
//   STOP              para los motores (también corta un GO)
// Respuestas: "OK ...", "ERR ...", "RUTA F F L ..."; el log empieza con '#'.
// Lo más fácil es mandarlo desde el PC: python -m pc.enviar <mapa.txt>, y la consola: python -m pc.consola
//
// Cómo recorre la ruta: junta las F seguidas y avanza n * t_celda, corrigiendo con los sensores laterales
// (si uno pisa una línea, le quita kp a la rueda del otro lado). Si el mapa dice que al final hay pared,
// avanza hasta que el frontal ve esa línea y sigue t_alinear ms: eso borra el error acumulado.
// Con giroscopio (MPU6050): los giros cortan a 90° - giro_antic_r / giro_antic_l del rumbo objetivo (múltiplos de 90° desde
// el inicio del GO, así el error no se acumula) y al avanzar se corrige el rumbo con kr. Sin giroscopio, los
// giros son por tiempo (t_giro). Durante los giros se ignoran los sensores de línea.
//
// OTA: espota / Arduino IDE, hostname "agente-laberinto".
// LED RGB: azul = sin mapa, verde = ruta lista, blanco = moviéndose, rojo = último reinicio por brownout.

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <esp_system.h>
#include <lwip/sockets.h>

#include "src/config/pines.h"
#include "src/config/parametros.h"
#include "src/planner/bfs.h"
#include "src/imu/mpu6050.h"

// Red propia del carro (siempre prendida, de respaldo). Además se conecta al hotspot guardado con
// RED_SSID / RED_CLAVE (en Preferences, no en el código: el repo es público); así el PC queda en la misma
// red que el carro sin perder internet. El hotspot del celular filtra mDNS y broadcast: pc/enviar.py
// encuentra el carro barriendo la subred (y recuerda la última IP).
const char* WIFI_SSID = "agente-laberinto";
const char* WIFI_PASS = "laberinto2026";
const char* OTA_PASS  = "laberinto2026";
const char* HOSTNAME  = "agente-laberinto";

WiFiServer servidorTcp(23);
WiFiClient cliente;
Preferences prefs;
esp_reset_reason_t razonReset;
char redSsid[33] = "";
char redClave[65] = "";

planner::LectorMapa lector;
planner::Mapa mapa;
planner::Ruta ruta;
bool hayMapa = false;
bool hayRuta = false;
float costoGiro = 0.5f;
Parametros par;

// Va antes de la primera función: el IDE de Arduino declara las funciones arriba de todo.
struct Lectura {
  int izq, cen, der;   // 0-1000, > umbral = negro
};

void led(uint8_t r, uint8_t g, uint8_t b) { rgbLedWrite(PIN_LED, r, g, b); }

void ledEstado() {
  if (razonReset == ESP_RST_BROWNOUT) led(40, 0, 0);
  else if (hayRuta) led(0, 40, 0);
  else led(0, 0, 40);
}

// Manda al cliente TCP solo si el socket lo acepta sin esperar; si no, la línea se pierde.
// cliente.println puede bloquear el loop hasta 10 s (NetworkClient::write reintenta 10 x 1 s) si el PC
// se fue de la red, y eso con los motores andando saca al carro del laberinto.
void enviarSinBloquear(const char* s, size_t n) {
  int fd = cliente.fd();
  if (fd < 0) return;
  fd_set set;
  FD_ZERO(&set);
  FD_SET(fd, &set);
  timeval cero = {0, 0};
  if (select(fd + 1, nullptr, &set, nullptr, &cero) > 0) send(fd, s, n, MSG_DONTWAIT);
}

// Log que va a la vez por Serial y al cliente TCP (lo que pasa durante un GO).
void logf(const char* fmt, ...) {
  char buf[128];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
  va_end(ap);
  n = constrain(n, 0, (int)sizeof(buf) - 3);
  Serial.println(buf);
  buf[n++] = '\r';
  buf[n++] = '\n';
  if (cliente && cliente.connected()) enviarSinBloquear(buf, n);
}

// ---------- motores ----------

void parar() {
  analogWrite(PIN_DER_EN, 0);
  analogWrite(PIN_IZQ_EN, 0);
  digitalWrite(PIN_DER_AD, LOW);
  digitalWrite(PIN_DER_AT, LOW);
  digitalWrite(PIN_IZQ_AD, LOW);
  digitalWrite(PIN_IZQ_AT, LOW);
}

void motor(uint8_t en, uint8_t inAd, uint8_t inAt, int pwm) {
  digitalWrite(inAd, pwm > 0);
  digitalWrite(inAt, pwm < 0);
  analogWrite(en, constrain(abs(pwm), 0, 255));
}

int ultDer = 0, ultIzq = 0;   // último PWM pedido, para saber hacia dónde frenar

// PWM con signo: positivo = adelante.
void ruedas(int der, int izq) {
  ultDer = der;
  ultIzq = izq;
  motor(PIN_DER_EN, PIN_DER_AD, PIN_DER_AT, der);
  motor(PIN_IZQ_EN, PIN_IZQ_AD, PIN_IZQ_AT, izq);
}

int signo(int v) { return (v > 0) - (v < 0); }

// Con pwm_freno > 0: contramarcha (cada rueda al revés de como iba), que para casi en seco.
// Si no, freno activo del L298N: las dos entradas en HIGH cortocircuitan el motor.
void frenar() {
  if (par.pwm_freno > 0) {
    motor(PIN_DER_EN, PIN_DER_AD, PIN_DER_AT, -signo(ultDer) * par.pwm_freno);
    motor(PIN_IZQ_EN, PIN_IZQ_AD, PIN_IZQ_AT, -signo(ultIzq) * par.pwm_freno);
    return;
  }
  digitalWrite(PIN_DER_AD, HIGH);
  digitalWrite(PIN_DER_AT, HIGH);
  digitalWrite(PIN_IZQ_AD, HIGH);
  digitalWrite(PIN_IZQ_AT, HIGH);
  analogWrite(PIN_DER_EN, 255);
  analogWrite(PIN_IZQ_EN, 255);
}

// ---------- sensores ----------

int normalizar(int crudo) { return (long)crudo * 1000 / ADC_MAX; }

Lectura leerSensores() {
  return {normalizar(analogRead(PIN_SENSOR_IZQ)), normalizar(analogRead(PIN_SENSOR_CEN)),
          normalizar(analogRead(PIN_SENSOR_DER))};
}

bool negro(int v) { return v > par.umbral; }

// ---------- ejecutor: máquina de estados sin delay (STOP y OTA siguen funcionando) ----------

enum Fase { QUIETO, AVANZAR, BUSCAR_LINEA, ALINEAR, GIRAR, PULSO, PAUSA };
const char* NOMBRE_FASE[] = {"QUIETO", "AVANZAR", "BUSCAR_LINEA", "ALINEAR", "GIRAR", "PULSO", "PAUSA"};
const int MAX_PULSOS = 8;

struct Ejecutor {
  Fase fase = QUIETO;
  unsigned long t0 = 0;        // inicio de la fase
  unsigned long dur = 0;       // duración de la fase (en BUSCAR_LINEA, tiempo máximo)
  bool enRuta = false;         // GO (sigue con la próxima acción) o PASO (para al terminar)
  int idx = 0;                 // próxima acción de la ruta
  planner::Estado pose;        // dónde cree el mapa que está el carro
  int celdas = 0;              // F del grupo actual
  bool alinear = false;        // al final del grupo hay pared: buscar la línea con el frontal
  int dirGiro = 0;             // +1 derecha, -1 izquierda (giro en curso)
  bool ajustarGiro = false;    // el último movimiento fue un giro: corregir con pulsos si quedó mal
  int pulsos = 0;              // pulsos de corrección del giro actual
  float rumboObj = 0;          // rumbo que debería tener (giroscopio), en grados
  unsigned long tGo = 0;

  unsigned long transcurrido() const { return millis() - t0; }
  void entrar(Fase f, unsigned long d) {
    fase = f;
    t0 = millis();
    dur = d;
  }
  bool moviendo() const { return fase != QUIETO; }
} ej;

void terminarMovimiento() {
  frenar();
  ej.entrar(PAUSA, par.pausa);
}

void empezarAvance(int n, bool alinear) {
  ej.celdas = n;
  ej.alinear = alinear;
  ej.ajustarGiro = false;
  ej.pulsos = 0;
  if (!ej.enRuta) ej.rumboObj = imu::rumbo();   // PASO: mantener el rumbo con el que arranca
  ej.entrar(AVANZAR, (unsigned long)n * par.t_celda);
  led(20, 20, 20);
}

// d = +1 derecha (horario), -1 izquierda.
void empezarGiro(int d) {
  ej.dirGiro = d;
  ej.ajustarGiro = true;
  ej.pulsos = 0;
  // En el GO el objetivo es absoluto (múltiplos de 90° desde el inicio); en un PASO, 90° desde donde está.
  ej.rumboObj = (ej.enRuta ? ej.rumboObj : imu::rumbo()) + d * 90;
  ruedas(-d * par.pwm_giro, d * par.pwm_giro);
  ej.entrar(GIRAR, par.t_giro);
  led(20, 20, 20);
}

void detener(const char* motivo) {
  parar();
  ej.fase = QUIETO;
  ej.enRuta = false;
  ledEstado();
  if (motivo) logf("%s", motivo);
}

// Arranca la próxima acción de la ruta: las F seguidas van juntas.
void siguienteAccion() {
  if (!ej.enRuta) {
    detener("OK fin paso");
    return;
  }
  if (ej.idx >= ruta.n) {
    detener(nullptr);
    logf("OK META en %.1f s", (millis() - ej.tGo) / 1000.0f);
    return;
  }
  static const int8_t DF[] = {-1, 0, 1, 0}, DC[] = {0, 1, 0, -1};
  char a = ruta.movimientos[ej.idx];
  if (a == 'F') {
    int n = 0;
    while (ej.idx < ruta.n && ruta.movimientos[ej.idx] == 'F') {
      ej.idx++;
      n++;
    }
    ej.pose.fila += DF[ej.pose.ori] * n;
    ej.pose.col += DC[ej.pose.ori] * n;
    bool pared = mapa.hay_pared(ej.pose.fila, ej.pose.col, ej.pose.ori);
    logf("# F x%d -> (%d,%d,%c)%s", n, ej.pose.fila, ej.pose.col, planner::LETRA_ORI[ej.pose.ori],
         pared ? " alinear con la pared" : "");
    empezarAvance(n, pared);
  } else {
    ej.idx++;
    int d = a == 'R' ? 1 : -1;
    ej.pose.ori = (ej.pose.ori + 4 + d) % 4;
    logf("# %c -> %c", a, planner::LETRA_ORI[ej.pose.ori]);
    empezarGiro(d);
  }
}

// Ya está en la última celda antes de una pared (o buscándola): ir a pwm_aprox para frenar más cerca.
bool aproximando() {
  return ej.fase == BUSCAR_LINEA || ej.fase == ALINEAR ||
         (ej.alinear && ej.transcurrido() >= (unsigned long)max(0, ej.celdas - 1) * par.t_celda);
}

// Avanza derecho corrigiendo el rumbo con el giroscopio y la posición con los laterales.
void avanzarCorrigiendo() {
  Lectura l = leerSensores();
  int base = aproximando() ? par.pwm_aprox : par.pwm;
  int der = base + par.comp, izq = base - par.comp;
  if (imu::ok() && par.kr > 0) {
    // desvio > 0: se torció a la derecha, así que la rueda derecha va más rápido para volver.
    int c = constrain((int)(par.kr * (imu::rumbo() - ej.rumboObj)), -60, 60);
    der += c;
    izq -= c;
  }
  if (negro(l.izq) && !negro(l.der)) der -= par.kp;        // se fue a la izquierda: girar a la derecha
  else if (negro(l.der) && !negro(l.izq)) izq -= par.kp;   // se fue a la derecha: girar a la izquierda
  ruedas(der, izq);
}

const unsigned long T_MAX_GO = 90000;   // si en 90 s no llegó, algo salió mal: parar

void atenderEjecutor() {
  if (ej.enRuta && millis() - ej.tGo > T_MAX_GO) {
    detener("ERR GO: pasaron 90 s sin llegar a la meta");
    return;
  }
  switch (ej.fase) {
    case QUIETO:
      return;
    case AVANZAR:
      if (ej.alinear) {
        // A partir de la última celda, el frontal puede ver la línea de enfrente.
        if (ej.transcurrido() >= (unsigned long)max(0, ej.celdas - 1) * par.t_celda &&
            negro(leerSensores().cen)) {
          ej.entrar(ALINEAR, par.t_alinear);
        } else if (ej.transcurrido() >= ej.dur) {
          ej.entrar(BUSCAR_LINEA, par.t_celda);   // se quedó corto: buscarla hasta una celda más
        }
      } else if (ej.transcurrido() >= ej.dur) {
        terminarMovimiento();
        return;
      }
      avanzarCorrigiendo();
      return;
    case BUSCAR_LINEA:
      if (negro(leerSensores().cen)) {
        ej.entrar(ALINEAR, par.t_alinear);
      } else if (ej.transcurrido() >= ej.dur) {
        logf("# no vio la línea de enfrente: sigue por tiempo");
        terminarMovimiento();
        return;
      }
      avanzarCorrigiendo();
      return;
    case ALINEAR:
      if (ej.transcurrido() >= ej.dur) {
        terminarMovimiento();
        return;
      }
      avanzarCorrigiendo();
      return;
    case GIRAR:
      if (!imu::ok()) {
        if (ej.transcurrido() >= ej.dur) terminarMovimiento();
      } else if ((ej.rumboObj - imu::rumbo()) * ej.dirGiro <=
                 (ej.dirGiro > 0 ? par.giro_antic_r : par.giro_antic_l)) {
        terminarMovimiento();
      } else if ((ej.rumboObj - imu::rumbo()) * ej.dirGiro > 110) {
        // falta cada vez más en vez de menos: el giroscopio cuenta al revés
        detener("ERR el rumbo va al revés del giro: SET imu_signo -1 (o 1) y SAVE");
      } else if (ej.transcurrido() >= 3 * ej.dur) {
        logf("# el giroscopio no llegó a los 90° en 3 x t_giro: para");
        terminarMovimiento();
      }
      return;
    case PULSO:
      // Sin contramarcha: con un pulso tan corto, la reversa deshace lo que giró.
      if (ej.transcurrido() >= ej.dur) {
        ruedas(0, 0);
        ej.entrar(PAUSA, par.pausa);
      }
      return;
    case PAUSA:
      if (ej.transcurrido() >= (unsigned long)par.t_freno) parar();
      if (ej.transcurrido() >= ej.dur) {
        if (imu::ok() && ej.ajustarGiro) {
          float falta = ej.rumboObj - imu::rumbo();   // > 0: falta girar a la derecha
          if (fabsf(falta) > par.giro_tol && ej.pulsos < MAX_PULSOS) {
            int d = falta > 0 ? 1 : -1;
            ej.pulsos++;
            ruedas(-d * par.pwm_giro, d * par.pwm_giro);
            ej.entrar(PULSO, par.t_pulso);
            return;
          }
        }
        if (imu::ok()) logf("# rumbo=%.1f objetivo=%.0f pulsos=%d", imu::rumbo(), ej.rumboObj, ej.pulsos);
        siguienteAccion();
      }
      return;
  }
}

// ---------- parámetros ----------

void cargarParametros() {
  for (const DefParametro& d : DEF_PARAMETROS) par.*d.campo = prefs.getInt(d.nombre, par.*d.campo);
}

void guardarParametros() {
  for (const DefParametro& d : DEF_PARAMETROS) prefs.putInt(d.nombre, par.*d.campo);
}

void imprimirParametros(Stream& s) {
  s.print("OK");
  for (const DefParametro& d : DEF_PARAMETROS) s.printf(" %s=%d", d.nombre, par.*d.campo);
  s.println();
}

bool cambiarParametro(const char* nombre, int valor) {
  for (const DefParametro& d : DEF_PARAMETROS) {
    if (strcasecmp(d.nombre, nombre) == 0) {
      par.*d.campo = valor;
      return true;
    }
  }
  return false;
}

bool sensContinuo = false;
bool imuContinuo = false;
unsigned long ultimoSens = 0;

const char* nombreRazon(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:  return "POWERON";
    case ESP_RST_EXT:      return "EXT";
    case ESP_RST_SW:       return "SW (OTA o restart)";
    case ESP_RST_PANIC:    return "PANIC";
    case ESP_RST_INT_WDT:  return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT:      return "WDT";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    default:               return "OTRO";
  }
}

// ---------- planear ----------

void imprimirRuta(Stream& s) {
  if (!hayRuta) {
    s.println("ERR no hay ruta");
    return;
  }
  s.print("RUTA");
  for (int i = 0; i < ruta.n; i++) {
    s.print(' ');
    s.print(ruta.movimientos[i]);
  }
  s.println();
}

void planear(Stream& s) {
  if (!hayMapa) {
    s.println("ERR no hay mapa");
    return;
  }
  unsigned long t0 = micros();
  hayRuta = planner::bfs(mapa, costoGiro, ruta);
  unsigned long us = micros() - t0;
  ledEstado();
  if (!hayRuta) {
    s.println("ERR el mapa no tiene ruta de inicio a meta");
    return;
  }
  imprimirRuta(s);
  s.printf("# acciones=%d costo=%g expandidos=%d tiempo=%lu us\n", ruta.n, ruta.costo, ruta.expandidos, us);
}

void guardarMapa() {
  prefs.putBytes("mapa", &mapa, sizeof(mapa));
}

void cargarMapa() {
  if (prefs.getBytesLength("mapa") == sizeof(mapa)) {
    prefs.getBytes("mapa", &mapa, sizeof(mapa));
    hayMapa = true;
  }
  costoGiro = prefs.getFloat("costo_giro", costoGiro);
}

// ---------- hotspot ----------

// Mientras busca el hotspot, el ESP32 salta de canal y la red propia se vuelve inestable: si no lo
// encuentra en T_BUSCAR_RED, deja de buscar y queda solo la red propia (RED CONECTAR reintenta).
const unsigned long T_BUSCAR_RED = 20000;
bool buscandoRed = false;
unsigned long tBuscarRed = 0;

void conectarRed() {
  WiFi.disconnect(false, false);   // solo la conexión al hotspot: la red propia sigue
  buscandoRed = redSsid[0] != '\0';
  tBuscarRed = millis();
  if (buscandoRed) WiFi.begin(redSsid, redClave);
}

void atenderRed() {
  static bool enHotspot = false;
  if ((WiFi.status() == WL_CONNECTED) != enHotspot) {
    enHotspot = !enHotspot;
    if (enHotspot) {
      buscandoRed = false;
      logf("# conectado a %s: %s", redSsid, WiFi.localIP().toString().c_str());
    } else {
      buscandoRed = true;   // autoReconnect lo busca; si no vuelve, se deja de buscar
      tBuscarRed = millis();
      logf("# se perdió el hotspot %s: sigue la red propia", redSsid);
    }
  }
  if (buscandoRed && millis() - tBuscarRed > T_BUSCAR_RED) {
    buscandoRed = false;
    WiFi.disconnect(false, false);
    logf("# no encontré el hotspot %s: solo la red propia (RED CONECTAR para reintentar)", redSsid);
  }
}

void imprimirRed(Stream& s) {
  s.printf("OK red=%s", redSsid[0] ? redSsid : "(ninguna)");
  if (WiFi.status() == WL_CONNECTED) s.printf(" ip=%s", WiFi.localIP().toString().c_str());
  else s.print(redSsid[0] ? " desconectado" : "");
  s.printf(" propia=%s\n", WiFi.softAPIP().toString().c_str());
}

// El resto de la línea después del comando (el nombre del hotspot puede tener espacios).
const char* resto(const char* linea) {
  while (*linea == ' ') linea++;
  while (*linea && *linea != ' ') linea++;
  while (*linea == ' ') linea++;
  return linea;
}

// Calibra el giroscopio; si el carro se movió (alguien lo estaba acomodando), espera y lo intenta otra vez.
bool calibrarQuieto() {
  if (imu::calibrar()) return true;
  delay(300);
  return imu::calibrar();
}

// ---------- consola: el mismo protocolo por Serial y por TCP ----------

struct Consola {
  Stream* s = nullptr;
  char linea[96];
  int n = 0;
  bool recibiendoMapa = false;
  bool errorMapa = false;

  void procesar(char* l);
  void leer() {
    while (s && s->available()) {
      char c = s->read();
      if (c == '\n') {
        linea[n] = '\0';
        procesar(linea);
        n = 0;
      } else if (c != '\r' && n < (int)sizeof(linea) - 1) {
        linea[n++] = c;
      }
    }
  }
};

void Consola::procesar(char* l) {
  if (recibiendoMapa) {
    if (strcmp(l, "FIN") == 0) {
      recibiendoMapa = false;
      if (errorMapa) return;   // el error ya se informó
      planner::Mapa nuevo;
      if (!lector.terminar(nuevo)) {
        s->printf("ERR mapa: %s\n", lector.error());
        return;
      }
      mapa = nuevo;
      hayMapa = true;
      guardarMapa();
      s->printf("OK MAPA %dx%d inicio=%d,%d,%c meta=%d,%d\n", mapa.filas, mapa.cols, mapa.inicio.fila,
                mapa.inicio.col, planner::LETRA_ORI[mapa.inicio.ori], mapa.meta_fila, mapa.meta_col);
      planear(*s);
    } else if (!errorMapa && !lector.linea(l)) {
      errorMapa = true;   // se siguen leyendo líneas hasta FIN para no tomarlas como comandos
      s->printf("ERR mapa: %s\n", lector.error());
    }
    return;
  }

  char original[sizeof(linea)];
  strlcpy(original, l, sizeof(original));
  char* cmd = strtok(l, " ");
  if (!cmd) return;
  char* arg = strtok(nullptr, " ");
  for (char* p = cmd; *p; p++) *p = toupper(*p);

  if (strcmp(cmd, "RED_SSID") == 0 || strcmp(cmd, "RED_CLAVE") == 0) {
    bool esSsid = cmd[4] == 'S';
    strlcpy(esSsid ? redSsid : redClave, resto(original), esSsid ? sizeof(redSsid) : sizeof(redClave));
    prefs.putString(esSsid ? "red_ssid" : "red_clave", esSsid ? redSsid : redClave);
    s->printf("OK %s guardado (RED CONECTAR para conectarse)\n", esSsid ? "nombre" : "clave");
  } else if (strcmp(cmd, "RED") == 0) {
    if (arg && strcasecmp(arg, "CONECTAR") == 0) {
      conectarRed();
      s->println(redSsid[0] ? "OK conectando (RED para ver la IP)" : "ERR falta RED_SSID");
    } else if (arg && strcasecmp(arg, "BORRAR") == 0) {
      redSsid[0] = redClave[0] = '\0';
      prefs.remove("red_ssid");
      prefs.remove("red_clave");
      conectarRed();
      s->println("OK hotspot borrado: solo la red propia");
    } else {
      imprimirRed(*s);
    }
  } else if (strcmp(cmd, "MAPA") == 0) {
    if (ej.moviendo()) detener("# mapa nuevo: parado");
    lector.empezar();
    recibiendoMapa = true;
    errorMapa = false;
  } else if (strcmp(cmd, "PLAN") == 0) {
    planear(*s);
  } else if (strcmp(cmd, "RUTA") == 0) {
    imprimirRuta(*s);
  } else if (strcmp(cmd, "COSTO_GIRO") == 0 && arg) {
    costoGiro = atof(arg);
    prefs.putFloat("costo_giro", costoGiro);
    s->printf("OK costo_giro=%g\n", costoGiro);
    if (hayMapa) planear(*s);
  } else if (strcmp(cmd, "STOP") == 0) {
    detener(nullptr);
    s->println("OK stop");
  } else if (strcmp(cmd, "GO") == 0) {
    if (!hayRuta) {
      s->println("ERR no hay ruta (mandar el mapa primero)");
    } else if (ej.moviendo()) {
      s->println("ERR ya se está moviendo (STOP primero)");
    } else if (imu::ok() && !calibrarQuieto()) {
      // rumbo 0 = orientación de inicio: si alguien lo estaba tocando, el sesgo sale mal y el carro se tuerce
      s->printf("ERR el carro se movió al calibrar el giroscopio (ruido=%.0f): dejarlo quieto y GO otra vez\n",
                imu::ruidoCal());
    } else {
      s->printf("OK GO %d acciones%s\n", ruta.n, imu::ok() ? "" : " (sin giroscopio: giros por tiempo)");
      ej.rumboObj = 0;
      ej.enRuta = true;
      ej.idx = 0;
      ej.pose = mapa.inicio;
      ej.tGo = millis();
      siguienteAccion();
    }
  } else if (strcmp(cmd, "PASO") == 0 && arg) {
    char a = toupper(arg[0]);
    char* n = strtok(nullptr, " ");
    if (ej.moviendo()) {
      s->println("ERR ya se está moviendo (STOP primero)");
    } else if (a == 'F' || a == 'A') {
      int celdas = a == 'A' ? 1 : (n ? max(1, atoi(n)) : 1);
      s->printf("OK PASO %c %d\n", a, celdas);
      empezarAvance(celdas, a == 'A');
    } else if (a == 'L' || a == 'R') {
      s->printf("OK PASO %c\n", a);
      empezarGiro(a == 'R' ? 1 : -1);
    } else {
      s->println("ERR uso: PASO F [n] | PASO L | PASO R | PASO A");
    }
  } else if (strcmp(cmd, "SET") == 0 && arg) {
    char* v = strtok(nullptr, " ");
    if (!v || !cambiarParametro(arg, atoi(v))) {
      s->println("ERR uso: SET <parametro> <valor>. Parámetros: GET");
    } else {
      imprimirParametros(*s);
    }
  } else if (strcmp(cmd, "GET") == 0) {
    imprimirParametros(*s);
  } else if (strcmp(cmd, "SAVE") == 0) {
    guardarParametros();
    s->println("OK parámetros guardados");
  } else if (strcmp(cmd, "SENS") == 0) {
    if (arg) {
      sensContinuo = strcasecmp(arg, "ON") == 0;
      s->printf("OK sens %s\n", sensContinuo ? "on" : "off");
    } else {
      Lectura l = leerSensores();
      s->printf("OK izq=%d cen=%d der=%d  (umbral=%d)%s%s%s\n", l.izq, l.cen, l.der, par.umbral,
                negro(l.izq) ? " IZQ" : "", negro(l.cen) ? " CEN" : "", negro(l.der) ? " DER" : "");
    }
  } else if (strcmp(cmd, "IMU") == 0) {
    if (!imu::ok()) {
      s->printf("ERR el giroscopio no responde (SDA=%d SCL=%d, VCC, GND); los giros van por tiempo\n",
                PIN_IMU_SDA, PIN_IMU_SCL);
    } else if (arg && strcasecmp(arg, "CAL") == 0) {
      if (ej.moviendo()) {
        s->println("ERR se está moviendo (STOP primero)");
      } else {
        if (imu::calibrar()) s->printf("OK imu calibrado: sesgo=%.1f ruido=%.1f rumbo=0\n", imu::sesgo(), imu::ruidoCal());
        else s->printf("ERR el carro se movió al calibrar (ruido=%.0f): sigue el sesgo anterior\n", imu::ruidoCal());
      }
    } else if (arg) {
      imuContinuo = strcasecmp(arg, "ON") == 0;
      s->printf("OK imu %s\n", imuContinuo ? "on" : "off");
    } else {
      s->printf("OK rumbo=%.1f sesgo=%.1f whoami=0x%02X imu_signo=%d\n", imu::rumbo(), imu::sesgo(),
                imu::quienSoy(), par.imu_signo);
    }
  } else if (strcmp(cmd, "INFO") == 0) {
    if (hayMapa) s->printf("OK mapa=%dx%d", mapa.filas, mapa.cols);
    else s->print("OK mapa=ninguno");
    s->printf(" ruta=%d costo_giro=%g fase=%s imu=%s reset=%s uptime=%lus clientes_wifi=%d hotspot=%s\n",
              hayRuta ? ruta.n : 0, costoGiro, NOMBRE_FASE[ej.fase], imu::ok() ? "ok" : "no",
              nombreRazon(razonReset), millis() / 1000, WiFi.softAPgetStationNum(),
              WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "no");
  } else {
    s->printf("ERR comando desconocido: %s\n", cmd);
  }
}

Consola consolaSerial, consolaTcp;

// Una conexión nueva reemplaza a la anterior: si el PC se fue de la red sin cerrar, el carro no se entera
// (la conexión muerta puede durar minutos) y sin esto no se podría volver a conectar.
void atenderTcp() {
  if (cliente && !cliente.connected()) cliente.stop();
  WiFiClient nuevo = servidorTcp.accept();
  if (nuevo) {
    if (cliente) cliente.stop();
    cliente = nuevo;
    consolaTcp = Consola();
    consolaTcp.s = &cliente;
    cliente.printf("# agente-laberinto. reset=%s. Comandos: MAPA ... FIN, PLAN, RUTA, COSTO_GIRO <v>, GO, "
                   "PASO F|L|R|A, SET <p> <v>, GET, SAVE, SENS [ON|OFF], IMU [ON|OFF|CAL], "
                   "RED [CONECTAR|BORRAR], RED_SSID <nombre>, RED_CLAVE <clave>, INFO, STOP\n",
                   nombreRazon(razonReset));
  }
  if (cliente && cliente.connected()) consolaTcp.leer();
}

// ---------- setup / loop ----------

void setup() {
  uint8_t pines[] = {PIN_DER_EN, PIN_IZQ_EN, PIN_DER_AD, PIN_DER_AT, PIN_IZQ_AD, PIN_IZQ_AT};
  for (uint8_t p : pines) pinMode(p, OUTPUT);
  parar();
  Serial.begin(115200);
  razonReset = esp_reset_reason();

  analogReadResolution(12);
  prefs.begin("agente", false);
  cargarParametros();
  // Al prender, el giroscopio tarda en estabilizarse y la primera calibración suele salir ruidosa:
  // reintentar. Si igual no queda, el GO vuelve a calibrar.
  if (imu::iniciar(PIN_IMU_SDA, PIN_IMU_SCL, &par.imu_signo)) {
    for (int i = 0; i < 3 && !imu::calibrar(); i++) delay(500);
  }
  cargarMapa();
  if (hayMapa) hayRuta = planner::bfs(mapa, costoGiro, ruta);
  ledEstado();

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  WiFi.setAutoReconnect(true);
  strlcpy(redSsid, prefs.getString("red_ssid", "").c_str(), sizeof(redSsid));
  strlcpy(redClave, prefs.getString("red_clave", "").c_str(), sizeof(redClave));
  conectarRed();
  // Menos potencia = picos de corriente más bajos (evita brownouts con la batería de 9V).
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([]() { detener(nullptr); led(40, 0, 40); });
  ArduinoOTA.begin();  // también inicia mDNS con el hostname
  servidorTcp.begin();

  consolaSerial.s = &Serial;
  Serial.printf("# agente listo en %s:23  reset=%s  mapa=%s  imu=%s\n", WiFi.softAPIP().toString().c_str(),
                nombreRazon(razonReset), hayMapa ? "guardado" : "ninguno", imu::ok() ? "ok" : "no");
}

void loop() {
  ArduinoOTA.handle();
  atenderTcp();
  consolaSerial.leer();
  atenderEjecutor();
  if (imu::seCayo()) logf("# ERR el giroscopio dejó de responder (¿cable suelto?): giros por tiempo");
  atenderRed();

  if ((sensContinuo || imuContinuo) && millis() - ultimoSens >= 200) {
    ultimoSens = millis();
    if (sensContinuo) {
      Lectura l = leerSensores();
      logf("izq=%d cen=%d der=%d", l.izq, l.cen, l.der);
    }
    if (imuContinuo) logf("rumbo=%.1f", imu::rumbo());
  }
}
