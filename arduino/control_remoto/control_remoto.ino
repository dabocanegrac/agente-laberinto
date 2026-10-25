// Control remoto del carro (ESP32-S3): página web servida por el propio carro.
// Conectarse a la red "agente-laberinto" y abrir http://192.168.4.1 (o http://agente-laberinto.local).
//
// HTTP:
//   GET /                 página de control (pagina.h)
//   GET /m?d=<v>&i=<v>    velocidad de cada rueda, -100..100. Hay que repetirlo: si no llega
//                         nada en DEADMAN_MS, el carro para solo (hombre muerto).
//   GET /stop             parar
//   GET /s                sensores y estado en JSON
// TCP 23: INFO, SENS, SENS ON|OFF, M <der> <izq> <ms>, STOP, y ajustes en vivo:
//         GET, PWMMIN <v>, PWMMAX <v>, GIRO <v>, FREQ <hz> (no se guardan: se pierden al reiniciar).
// OTA: espota / Arduino IDE, hostname "agente-laberinto".
// LED RGB: azul = esperando, verde = alguien manejando, rojo = último reinicio por brownout.

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <esp_system.h>
#include "pagina.h"

const char* WIFI_SSID = "agente-laberinto";
const char* WIFI_PASS = "laberinto2026";
const char* OTA_PASS  = "laberinto2026";
const char* HOSTNAME  = "agente-laberinto";

const uint8_t PIN_ENA = 7;    // motor A = llanta derecha
const uint8_t PIN_IN1 = 15;
const uint8_t PIN_IN2 = 16;
const uint8_t PIN_IN3 = 17;   // motor B = llanta izquierda
const uint8_t PIN_IN4 = 18;
const uint8_t PIN_ENB = 8;
const uint8_t PIN_SENSOR_IZQ = 4;
const uint8_t PIN_SENSOR_CEN = 5;
const uint8_t PIN_SENSOR_DER = 6;
const uint8_t PIN_RGB = 48;
const int ADC_MAX = 4095;

// Ajustables en vivo por TCP (PWMMIN, GIRO, FREQ). TODO: pasar los valores calibrados aquí.
int pwmMin = 90;            // por debajo de esto los TT no arrancan
int pwmGiroMin = 180;       // mínimo cuando las ruedas van en sentidos opuestos (giro en el eje)
int pwmMax = 255;           // con 6xAA (9V) bajar a ~217 para no pasar de ~6V promedio en los N20
uint32_t pwmFreq = 1000;    // Hz; más bajo suele dar más torque a baja velocidad con el L298N
// El arranque con golpe (para vencer la fricción estática) usa pwmMax.
const unsigned long GOLPE_MS = 80;
const unsigned long DEADMAN_MS = 500;
const unsigned long MAX_MOV_MS = 3000;

WebServer web(80);
WiFiServer servidorTcp(23);
WiFiClient cliente;
String linea;
bool sensContinuo = false;
unsigned long ultimoSens = 0;
unsigned long finMovimiento = 0;   // movimiento temporizado por TCP
unsigned long ultimoCmdWeb = 0;    // último /m recibido
bool manejoWeb = false;
int objetivoDer = 0, objetivoIzq = 0;   // PWM con signo que se aplica después del golpe
unsigned long finGolpe = 0;
esp_reset_reason_t razonReset;

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

void led(uint8_t r, uint8_t g, uint8_t b) { rgbLedWrite(PIN_RGB, r, g, b); }

void log(const String& s) {
  Serial.println(s);
  if (cliente && cliente.connected()) cliente.println(s);
}

// ---------- motores ----------

void parar() {
  analogWrite(PIN_ENA, 0);
  analogWrite(PIN_ENB, 0);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
  finMovimiento = 0;
  manejoWeb = false;
  objetivoDer = objetivoIzq = 0;
  finGolpe = 0;
}

void motor(uint8_t en, uint8_t inAd, uint8_t inAt, int pwm) {
  digitalWrite(inAd, pwm > 0);
  digitalWrite(inAt, pwm < 0);
  analogWrite(en, constrain(abs(pwm), 0, 255));
}

int signo(int v) { return (v > 0) - (v < 0); }

// -100..100 -> PWM con zona muerta: cualquier valor distinto de cero arranca en pwmMin.
int aPwm(int v, int minimo) {
  v = constrain(v, -100, 100);
  if (abs(v) < 3) return 0;
  minimo = min(minimo, pwmMax);
  int p = minimo + (long)(pwmMax - minimo) * abs(v) / 100;
  return v > 0 ? p : -p;
}

void aplicar(int pDer, int pIzq) {
  motor(PIN_ENA, PIN_IN1, PIN_IN2, pDer);
  motor(PIN_ENB, PIN_IN3, PIN_IN4, pIzq);
}

void ruedas(int der, int izq) {
  bool enEje = signo(der) * signo(izq) < 0;
  int minimo = enEje ? max(pwmMin, pwmGiroMin) : pwmMin;
  int pDer = aPwm(der, minimo);
  int pIzq = aPwm(izq, minimo);

  // Golpe de arranque si alguna rueda arranca o cambia de sentido.
  bool arranca = (pDer && signo(pDer) != signo(objetivoDer)) ||
                 (pIzq && signo(pIzq) != signo(objetivoIzq));
  objetivoDer = pDer;
  objetivoIzq = pIzq;
  if (arranca) {
    aplicar(signo(pDer) * pwmMax, signo(pIzq) * pwmMax);
    finGolpe = millis() + GOLPE_MS;
  } else if (!finGolpe) {
    aplicar(pDer, pIzq);
  }
}

void fijarFrecuencia(uint32_t hz) {
  pwmFreq = constrain(hz, 50, 20000);
  analogWriteFrequency(PIN_ENA, pwmFreq);
  analogWriteFrequency(PIN_ENB, pwmFreq);
}

// ---------- sensores ----------

int normalizar(int crudo) { return (long)crudo * 1000 / ADC_MAX; }

void leerSensores(int& izq, int& cen, int& der) {
  izq = normalizar(analogRead(PIN_SENSOR_IZQ));
  cen = normalizar(analogRead(PIN_SENSOR_CEN));
  der = normalizar(analogRead(PIN_SENSOR_DER));
}

String lecturaSensores() {
  int izq, cen, der;
  leerSensores(izq, cen, der);
  char buf[48];
  snprintf(buf, sizeof(buf), "izq:%d,cen:%d,der:%d", izq, cen, der);
  return String(buf);
}

// ---------- web ----------

void webPagina() { web.send(200, "text/html; charset=utf-8", PAGINA); }

void webMover() {
  int d = web.arg("d").toInt();
  int i = web.arg("i").toInt();
  finMovimiento = 0;
  ruedas(d, i);
  ultimoCmdWeb = millis();
  manejoWeb = (d != 0 || i != 0);
  web.send(200, "text/plain", "ok");
}

void webParar() {
  parar();
  web.send(200, "text/plain", "ok");
}

void webEstado() {
  int izq, cen, der;
  leerSensores(izq, cen, der);
  char buf[160];
  snprintf(buf, sizeof(buf),
           "{\"izq\":%d,\"cen\":%d,\"der\":%d,\"reset\":\"%s\",\"uptime\":%lu,\"clientes\":%d}",
           izq, cen, der, nombreRazon(razonReset), millis() / 1000, WiFi.softAPgetStationNum());
  web.sendHeader("Cache-Control", "no-store");
  web.send(200, "application/json", buf);
}

// ---------- TCP (depuración) ----------

void procesar(String cmd) {
  cmd.trim();
  if (cmd.isEmpty()) return;
  String may = cmd;
  may.toUpperCase();

  if (may == "INFO") {
    log("OK reset=" + String(nombreRazon(razonReset)) +
        " uptime_s=" + String(millis() / 1000) +
        " heap=" + String(ESP.getFreeHeap()) +
        " clientes_wifi=" + String(WiFi.softAPgetStationNum()));
  } else if (may == "SENS") {
    log("OK " + lecturaSensores());
  } else if (may == "SENS ON") {
    sensContinuo = true;
    log("OK sens on");
  } else if (may == "SENS OFF") {
    sensContinuo = false;
    log("OK sens off");
  } else if (may == "STOP") {
    parar();
    log("OK stop");
  } else if (may == "GET") {
    log("OK PWMMIN=" + String(pwmMin) + " PWMMAX=" + String(pwmMax) +
        " GIRO=" + String(pwmGiroMin) + " FREQ=" + String(pwmFreq));
  } else if (may.startsWith("PWMMAX ")) {
    pwmMax = constrain(cmd.substring(7).toInt(), 0, 255);
    log("OK PWMMAX=" + String(pwmMax));
  } else if (may.startsWith("PWMMIN ")) {
    pwmMin = constrain(cmd.substring(7).toInt(), 0, 255);
    log("OK PWMMIN=" + String(pwmMin));
  } else if (may.startsWith("GIRO ")) {
    pwmGiroMin = constrain(cmd.substring(5).toInt(), 0, 255);
    log("OK GIRO=" + String(pwmGiroMin));
  } else if (may.startsWith("FREQ ")) {
    fijarFrecuencia(cmd.substring(5).toInt());
    log("OK FREQ=" + String(pwmFreq));
  } else if (may.startsWith("M ")) {
    int der, izq;
    unsigned long ms;
    if (sscanf(cmd.c_str() + 2, "%d %d %lu", &der, &izq, &ms) != 3) {
      log("ERR uso: M <der> <izq> <ms>   (PWM -255..255)");
      return;
    }
    manejoWeb = false;
    finGolpe = 0;
    objetivoDer = der;
    objetivoIzq = izq;
    aplicar(der, izq);
    finMovimiento = millis() + min(ms, MAX_MOV_MS);
    log("OK mover der=" + String(der) + " izq=" + String(izq));
  } else {
    log("ERR comando desconocido: " + cmd);
  }
}

void atenderTcp() {
  if (!cliente || !cliente.connected()) {
    if (cliente) {
      sensContinuo = false;
      cliente.stop();
    }
    WiFiClient nuevo = servidorTcp.accept();
    if (nuevo) {
      cliente = nuevo;
      linea = "";
      log("# conectado. reset=" + String(nombreRazon(razonReset)) +
          ". Comandos: INFO, SENS, SENS ON|OFF, M <der> <izq> <ms>, STOP, GET, PWMMIN <v>, PWMMAX <v>, GIRO <v>, FREQ <hz>");
    }
  }
  if (cliente && cliente.connected()) {
    while (cliente.available()) {
      char c = cliente.read();
      if (c == '\n') { procesar(linea); linea = ""; }
      else if (c != '\r' && linea.length() < 80) linea += c;
    }
  }
}

// ---------- setup / loop ----------

void setup() {
  uint8_t pines[] = {PIN_ENA, PIN_ENB, PIN_IN1, PIN_IN2, PIN_IN3, PIN_IN4};
  for (uint8_t p : pines) pinMode(p, OUTPUT);
  parar();
  fijarFrecuencia(pwmFreq);
  analogReadResolution(12);
  Serial.begin(115200);

  razonReset = esp_reset_reason();
  led(0, 0, 40);

  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  // Menos potencia = picos de corriente más bajos (evita brownouts con la batería de 9V).
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([]() { parar(); led(40, 0, 40); });
  ArduinoOTA.begin();  // también inicia mDNS con el hostname
  MDNS.addService("http", "tcp", 80);

  web.on("/", webPagina);
  web.on("/m", webMover);
  web.on("/stop", webParar);
  web.on("/s", webEstado);
  web.onNotFound([]() { web.send(404, "text/plain", "no existe"); });
  web.begin();
  servidorTcp.begin();

  Serial.printf("# control_remoto listo: http://%s  reset=%s\n",
                WiFi.softAPIP().toString().c_str(), nombreRazon(razonReset));
}

void loop() {
  ArduinoOTA.handle();
  web.handleClient();
  atenderTcp();

  if (finGolpe && (long)(millis() - finGolpe) >= 0) {
    finGolpe = 0;
    aplicar(objetivoDer, objetivoIzq);
  }

  // Hombre muerto: si la página deja de mandar órdenes, parar.
  if (manejoWeb && millis() - ultimoCmdWeb > DEADMAN_MS) {
    parar();
    log("# hombre muerto: sin órdenes de la página, parado");
  }

  if (finMovimiento && (long)(millis() - finMovimiento) >= 0) {
    parar();
    log("# fin movimiento");
  }

  if (sensContinuo && millis() - ultimoSens >= 250) {
    ultimoSens = millis();
    log(lecturaSensores());
  }

  if (razonReset == ESP_RST_BROWNOUT) led(40, 0, 0);
  else if (manejoWeb) led(0, 40, 0);
  else led(0, 0, 40);
}
