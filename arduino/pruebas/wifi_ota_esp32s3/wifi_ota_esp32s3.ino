// Wi-Fi + OTA + comandos por TCP (ESP32-S3, fase E5; con batería también cubre E4).
// Crea la red "agente-laberinto". Comandos por TCP en 192.168.4.1:23, una línea por comando:
//   INFO              razón del último reinicio, uptime, memoria libre
//   SENS              una lectura de los sensores (normalizada 0-1000)
//   SENS ON | OFF     lecturas continuas a 4 Hz
//   M <der> <izq> <ms>  PWM -255..255 por motor durante ms (máx 3000), luego para
//   STOP              detiene los motores
// LED RGB: azul = esperando conexión, verde = cliente conectado, rojo = último reinicio por brownout.

#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_system.h>

const char* WIFI_SSID = "agente-laberinto";
const char* WIFI_PASS = "laberinto2026";
const char* OTA_PASS  = "laberinto2026";

const uint8_t PIN_ENA = 7;
const uint8_t PIN_IN1 = 15;
const uint8_t PIN_IN2 = 16;
const uint8_t PIN_IN3 = 17;
const uint8_t PIN_IN4 = 18;
const uint8_t PIN_ENB = 8;
const uint8_t PIN_SENSOR_IZQ = 4;
const uint8_t PIN_SENSOR_CEN = 5;
const uint8_t PIN_SENSOR_DER = 6;
const uint8_t PIN_RGB = 48;
const int ADC_MAX = 4095;
const unsigned long MAX_MOV_MS = 3000;

WiFiServer servidor(23);
WiFiClient cliente;
String linea;
bool sensContinuo = false;
unsigned long ultimoSens = 0;
unsigned long finMovimiento = 0;
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

void parar() {
  analogWrite(PIN_ENA, 0);
  analogWrite(PIN_ENB, 0);
  digitalWrite(PIN_IN1, LOW);
  digitalWrite(PIN_IN2, LOW);
  digitalWrite(PIN_IN3, LOW);
  digitalWrite(PIN_IN4, LOW);
  finMovimiento = 0;
}

void motor(uint8_t en, uint8_t inAd, uint8_t inAt, int pwm) {
  digitalWrite(inAd, pwm > 0);
  digitalWrite(inAt, pwm < 0);
  analogWrite(en, constrain(abs(pwm), 0, 255));
}

int normalizar(int crudo) { return (long)crudo * 1000 / ADC_MAX; }

String lecturaSensores() {
  char buf[64];
  snprintf(buf, sizeof(buf), "izq:%d,cen:%d,der:%d",
           normalizar(analogRead(PIN_SENSOR_IZQ)),
           normalizar(analogRead(PIN_SENSOR_CEN)),
           normalizar(analogRead(PIN_SENSOR_DER)));
  return String(buf);
}

void procesar(String cmd) {
  cmd.trim();
  if (cmd.isEmpty()) return;
  String may = cmd;
  may.toUpperCase();

  if (may == "INFO") {
    log("OK reset=" + String(nombreRazon(razonReset)) +
        " uptime_s=" + String(millis() / 1000) +
        " heap=" + String(ESP.getFreeHeap()) +
        " psram=" + String(ESP.getPsramSize()));
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
  } else if (may.startsWith("M ")) {
    int der, izq;
    unsigned long ms;
    if (sscanf(cmd.c_str() + 2, "%d %d %lu", &der, &izq, &ms) != 3) {
      log("ERR uso: M <der> <izq> <ms>");
      return;
    }
    ms = min(ms, MAX_MOV_MS);
    motor(PIN_ENA, PIN_IN1, PIN_IN2, der);
    motor(PIN_ENB, PIN_IN3, PIN_IN4, izq);
    finMovimiento = millis() + ms;
    log("OK mover der=" + String(der) + " izq=" + String(izq) + " ms=" + String(ms));
  } else {
    log("ERR comando desconocido: " + cmd);
  }
}

void setup() {
  uint8_t pines[] = {PIN_ENA, PIN_ENB, PIN_IN1, PIN_IN2, PIN_IN3, PIN_IN4};
  for (uint8_t p : pines) pinMode(p, OUTPUT);
  parar();
  analogReadResolution(12);
  Serial.begin(115200);

  razonReset = esp_reset_reason();
  led(0, 0, 40);

  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  // Menos potencia = picos de corriente más bajos (evita brownouts con la batería de 9V).
  // Alcanza de sobra para unos metros.
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  ArduinoOTA.setHostname("agente-laberinto");
  ArduinoOTA.setPassword(OTA_PASS);
  ArduinoOTA.onStart([]() { parar(); led(40, 0, 40); });
  ArduinoOTA.begin();
  servidor.begin();

  Serial.printf("# AP %s listo en %s, reset=%s\n", WIFI_SSID,
                WiFi.softAPIP().toString().c_str(), nombreRazon(razonReset));
}

void loop() {
  ArduinoOTA.handle();

  if (!cliente || !cliente.connected()) {
    if (cliente) {          // se cayó la conexión: por seguridad, parar
      parar();
      sensContinuo = false;
      cliente.stop();
    }
    WiFiClient nuevo = servidor.accept();
    if (nuevo) {
      cliente = nuevo;
      linea = "";
      log("# conectado. reset=" + String(nombreRazon(razonReset)) + ". Comandos: INFO, SENS, SENS ON|OFF, M <der> <izq> <ms>, STOP");
    }
  }

  if (cliente && cliente.connected()) {
    while (cliente.available()) {
      char c = cliente.read();
      if (c == '\n') { procesar(linea); linea = ""; }
      else if (c != '\r' && linea.length() < 80) linea += c;
    }
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
  else if (cliente && cliente.connected()) led(0, 40, 0);
  else led(0, 0, 40);
}
