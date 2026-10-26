// Firmware del agente (ESP32-S3): recibe el mapa, calcula la ruta a bordo con BFS y la guarda.
// Ejecutar la ruta con los motores es el siguiente paso (HW-05).
//
// Comandos, una línea cada uno, por Serial (115200) o por TCP en 192.168.4.1:23 (red agente-laberinto):
//   MAPA              empieza a recibir un mapa (formato de grilla, ver src/planner/mapa.h)
//   ...líneas...      inicio, meta y filas de la grilla
//   FIN               termina el mapa: lo valida, lo guarda y calcula la ruta
//   PLAN              vuelve a calcular la ruta con el mapa guardado
//   RUTA              imprime la última ruta
//   COSTO_GIRO <v>    costo de L/R relativo a F (por defecto 0.5) y vuelve a planear
//   INFO              estado
//   STOP              para los motores
// Respuestas: "OK ...", "ERR ...", "RUTA F F L ..."; el log empieza con '#'.
// Lo más fácil es mandarlo desde el PC: python -m pc.enviar <mapa.txt>
//
// OTA: espota / Arduino IDE, hostname "agente-laberinto".
// LED RGB: azul = sin mapa, verde = ruta lista, rojo = último reinicio por brownout.

#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <esp_system.h>

#include "src/config/pines.h"
#include "src/config/parametros.h"
#include "src/planner/bfs.h"

const char* WIFI_SSID = "agente-laberinto";
const char* WIFI_PASS = "laberinto2026";
const char* OTA_PASS  = "laberinto2026";
const char* HOSTNAME  = "agente-laberinto";

WiFiServer servidorTcp(23);
WiFiClient cliente;
Preferences prefs;
esp_reset_reason_t razonReset;

planner::LectorMapa lector;
planner::Mapa mapa;
planner::Ruta ruta;
bool hayMapa = false;
bool hayRuta = false;
float costoGiro = 0.5f;

void led(uint8_t r, uint8_t g, uint8_t b) { rgbLedWrite(PIN_LED, r, g, b); }

void ledEstado() {
  if (razonReset == ESP_RST_BROWNOUT) led(40, 0, 0);
  else if (hayRuta) led(0, 40, 0);
  else led(0, 0, 40);
}

void parar() {
  analogWrite(PIN_DER_EN, 0);
  analogWrite(PIN_IZQ_EN, 0);
  digitalWrite(PIN_DER_AD, LOW);
  digitalWrite(PIN_DER_AT, LOW);
  digitalWrite(PIN_IZQ_AD, LOW);
  digitalWrite(PIN_IZQ_AT, LOW);
}

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

  char* cmd = strtok(l, " ");
  if (!cmd) return;
  char* arg = strtok(nullptr, " ");
  for (char* p = cmd; *p; p++) *p = toupper(*p);

  if (strcmp(cmd, "MAPA") == 0) {
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
    parar();
    s->println("OK stop");
  } else if (strcmp(cmd, "INFO") == 0) {
    if (hayMapa) s->printf("OK mapa=%dx%d", mapa.filas, mapa.cols);
    else s->print("OK mapa=ninguno");
    s->printf(" ruta=%d costo_giro=%g reset=%s uptime=%lus clientes_wifi=%d\n", hayRuta ? ruta.n : 0, costoGiro,
              nombreRazon(razonReset), millis() / 1000, WiFi.softAPgetStationNum());
  } else {
    s->printf("ERR comando desconocido: %s\n", cmd);
  }
}

Consola consolaSerial, consolaTcp;

void atenderTcp() {
  if (!cliente || !cliente.connected()) {
    if (cliente) cliente.stop();
    WiFiClient nuevo = servidorTcp.accept();
    if (nuevo) {
      cliente = nuevo;
      consolaTcp = Consola();
      consolaTcp.s = &cliente;
      cliente.printf("# agente-laberinto. reset=%s. Comandos: MAPA ... FIN, PLAN, RUTA, COSTO_GIRO <v>, INFO, STOP\n",
                     nombreRazon(razonReset));
    }
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

  prefs.begin("agente", false);
  cargarMapa();
  if (hayMapa) hayRuta = planner::bfs(mapa, costoGiro, ruta);
  ledEstado();

  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  // Menos potencia = picos de corriente más bajos (evita brownouts con la batería de 9V).
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([]() { parar(); led(40, 0, 40); });
  ArduinoOTA.begin();  // también inicia mDNS con el hostname
  servidorTcp.begin();

  consolaSerial.s = &Serial;
  Serial.printf("# agente listo en %s:23  reset=%s  mapa=%s\n", WiFi.softAPIP().toString().c_str(),
                nombreRazon(razonReset), hayMapa ? "guardado" : "ninguno");
}

void loop() {
  ArduinoOTA.handle();
  atenderTcp();
  consolaSerial.leer();
}
