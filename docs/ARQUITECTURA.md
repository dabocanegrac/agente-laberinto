# Arquitectura

Agente que resuelve un laberinto conocido: **percibir → planear → actuar**.
El diseño tiene dos objetivos:

1. **Soportar varios algoritmos** de búsqueda sin tocar el código del carro.
2. **Correr en dos placas**: ESP32-S3 N16R8 (la de la demo: Wi-Fi, flasheo sin cable, más RAM) y Arduino UNO (plan B, ya probado).

## 1. Capas

```
  maps/*.txt ──► PLANIFICADOR  (BFS, DFS, UCS, A*, Greedy... intercambiables)
                      │  ruta: "F F L F R" + métricas
                      ▼
                 EJECUTOR      (sigue la ruta, corrige con sensores, log)
                      │  avanzar_celda(), girar(), parar()
                      ▼
                 MOVIMIENTO    (movimientos de una celda con corrección)
                      │
                      ▼
                 HAL           (motores, sensores, almacenamiento, transporte)
                      │
              ┌───────┴────────┐
          pines_uno.h     pines_esp32s3.h
```

Reglas:
- Cada capa solo llama a la de abajo.
- **Ninguna capa por encima de la HAL usa `#if` de plataforma.** Las diferencias entre placas viven solo en la HAL y en los archivos de pines.
- El planificador no conoce el hardware y el ejecutor no sabe qué algoritmo produjo la ruta.

## 2. Contratos compartidos

### 2.1 Mapa

Pendiente (Decisiones, DEC-01). Lo que proponemos es una **máscara de bits de paredes por celda**:

```
# maps/mapa1.txt
# filas columnas
5 5
# inicio: fila col orientación (N/E/S/O); meta: fila col
inicio 0 0 E
meta 4 4
# una fila por línea, un número por celda: N=1 E=2 S=4 O=8
9 5 3 9 3
...
```

- La fila 0 es la de arriba y la columna 0 la de la izquierda.
- Las paredes compartidas entre dos celdas se marcan **en las dos** celdas. `mapa.py` lo valida.

### 2.2 Ruta

Una lista de movimientos relativos separados por espacios:

| Movimiento | Significado |
|---|---|
| `F` | avanzar una celda |
| `L` | girar 90° a la izquierda |
| `R` | girar 90° a la derecha |

La media vuelta se representa como `R R`. El ejecutor puede optimizarla, pero el contrato no cambia.

### 2.3 Estado y costos (para que los algoritmos se diferencien)

El estado de búsqueda es **`(fila, col, orientación)`**, no solo la celda. Las acciones son `F`, `L` y `R`, y cada una tiene su costo:

| Acción | Costo por defecto |
|---|---|
| `F` | 1.0 |
| `L`, `R` | `COSTO_GIRO` (≈ `T_GIRO90_MS / T_CELDA_MS`, medido en el carro) |

- **BFS** minimiza el número de acciones.
- **UCS y A\*** minimizan el costo, o sea el **tiempo real**, y prefieren rutas con menos giros.
- **DFS y Greedy** encuentran *una* ruta, no necesariamente la óptima.

Así la comparación del informe muestra diferencias reales que se pueden cronometrar con el carro.

Heurística para A\*: distancia Manhattan, más 1 giro si la meta no queda en la dirección actual. Es admisible.

## 3. Planificador (PC, Python)

Todos los algoritmos se escriben en Python. Python es la referencia para el simulador, la comparación y el informe.

```python
# pc/planners/base.py
@dataclass
class Resultado:
    movimientos: list[str]      # ["F", "F", "L", ...]
    estados: list[Estado]       # (fila, col, orientación) por paso
    costo: float
    nodos_expandidos: int
    tiempo_ms: float

class Planner(ABC):
    nombre: str
    @abstractmethod
    def planear(self, mapa: Mapa, inicio: Estado, meta: Celda) -> Resultado | None: ...

REGISTRO: dict[str, type[Planner]] = {}
def registrar(cls): REGISTRO[cls.nombre] = cls; return cls
```

Agregar un algoritmo es crear un archivo en `pc/planners/` con un `@registrar class Algo(Planner)`. No hay que tocar nada más.

```
python -m pc.planear maps/mapa1.txt --algo astar        # imprime la ruta
python -m pc.planear maps/mapa1.txt --algo all          # tabla comparativa
python -m pc.simulador maps/mapa1.txt --algo ucs        # animación
python -m pc.enviar maps/mapa1.txt --algo astar --puerto COM5   # o --ip 192.168.4.1
```

**Tests:** `pc/tests/` comprueba que todos los algoritmos óptimos den el mismo costo en cada mapa de `maps/`, y que las rutas guardadas en `maps/*.esperado` no cambien sin querer.

## 4. Firmware (Arduino IDE, un solo sketch para las dos placas)

El Arduino IDE solo compila lo que está en la carpeta del sketch y en `src/` (de forma recursiva). Por eso todo el código vive dentro de `agente/src/`.

```
arduino/agente/
├── agente.ino                  # setup/loop: transporte -> comandos -> ejecutor
└── src/
    ├── config/
    │   ├── pines.h             # elige el archivo según la placa
    │   ├── pines_uno.h
    │   └── pines_esp32s3.h
    ├── hal/
    │   ├── motores.h/.cpp      # mover(pwm_izq, pwm_der), parar()
    │   ├── sensores.h/.cpp     # leer() normalizado 0..1000, es_negro()
    │   ├── almacen.h/.cpp      # parámetros y ruta: EEPROM (UNO) / Preferences (ESP32)
    │   └── red_esp32.cpp       # Wi-Fi AP, WebSocket, OTA (solo se compila en ESP32)
    ├── movimiento.h/.cpp       # avanzar_celda(), girar(), con corrección por sensores
    ├── ejecutor.h/.cpp         # recorre la ruta, maneja estados y log
    ├── comandos.h/.cpp         # protocolo de texto sobre cualquier Stream
    └── planner/                # opcional: BFS/A* en C++ portable (sin Arduino.h)
```

### 4.1 Selección de placa

```cpp
// src/config/pines.h
#pragma once
#if defined(ARDUINO_ARCH_ESP32)
  #include "pines_esp32s3.h"
#elif defined(ARDUINO_ARCH_AVR)
  #include "pines_uno.h"
#else
  #error "Placa no soportada"
#endif
```

| | `pines_uno.h` | `pines_esp32s3.h` |
|---|---|---|
| ENA / IN1 / IN2 (motor A = llanta derecha) | 5 / 7 / 8 | 7 / 15 / 16 |
| ENB / IN3 / IN4 (motor B = llanta izquierda) | 6 / 9 / 10 | 8 / 17 / 18 |
| Sensor izq / centro / der | A0 / A1 / A2 | 4 / 5 / 6 (ADC1) |
| Borne 5V del L298N | 5V del Arduino | 5V del shield |
| `ADC_MAX` | 1023 | 4095 |
| LED de estado | 13 | 48 (RGB) |
| Alimentación de los sensores | 5V | **3V3** (filas G/V/S del shield, medido 3.3V) |

### 4.1.1 Compilar y subir a la ESP32-S3

```
FQBN = esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=default,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn $FQBN --output-dir build <sketch>
arduino-cli upload  -p COM6 --fqbn $FQBN <sketch>                       # por cable (puerto "COM", chip CH343)
espota -i 192.168.4.1 -I <ip del PC> -p 3232 -a <clave OTA> -f build/<sketch>.ino.bin -r   # por Wi-Fi
```

- `CDCOnBoot=default` porque se programa por el USB-C "COM" (UART). Con el USB nativo sería `cdc`.
- `app3M_fat9M_16MB` aprovecha los 16 MB: 3 MB por app y dos slots para OTA. La tabla por defecto es de 4 MB y deja solo 1.3 MB.
- `espota` está en `Arduino15/packages/esp32/hardware/esp32/<versión>/tools/`.
- **Firewall de Windows:** en el OTA, el ESP32 abre una conexión de vuelta hacia el PC. Windows la bloquea si `espota.exe` no tiene una regla de entrada **para el perfil de la red del carro** (pública o privada). Al primer intento aparece una ventana para permitirlo.
- La red del carro es `agente-laberinto` (192.168.4.1). Comandos por TCP en el puerto 23.

Verificado en las dos placas: adelante es IN1=H/IN2=L (derecha) e IN3=H/IN4=L (izquierda), sin invertir motores. En la S3 (23/10/2026), los sensores leen ~35-57 en blanco y 1000 en el aire (escala 0-1000).

### 4.2 Sensores normalizados

`sensores.leer()` devuelve valores de **0 a 1000** en las dos placas: `crudo * 1000 / ADC_MAX`, con la polaridad aplicada para que 1000 sea negro. Los umbrales se configuran en esa escala, así que pasar de 10 a 12 bits no cambia nada por encima de la HAL.

### 4.3 Transporte: cualquier `Stream`

`comandos` recibe un `Stream&`, la clase base de Arduino que tienen `Serial`, `SoftwareSerial` y `WiFiClient`. El mismo protocolo funciona entonces:

| Placa | Transporte |
|---|---|
| UNO | `Serial` por USB (y `SoftwareSerial` en D2/D3 si se agrega un HC-05) |
| ESP32-S3 | `Serial` por USB y Telnet/WebSocket por Wi-Fi, en modo AP |

### 4.4 Protocolo de comandos

Es de texto, una línea por comando. Responde `OK ...` o `ERR ...`, y el log empieza con `#`.

| Comando | Qué hace |
|---|---|
| `RUTA F F L F R` | guarda la ruta en memoria y en el almacén |
| `GO` / `STOP` | ejecuta o detiene la ruta |
| `PASO F` | ejecuta un solo movimiento (para calibrar) |
| `SET <param> <valor>` | `T_CELDA`, `T_GIRO`, `PWM`, `COMP_IZQ`, `COMP_DER`, `UMBRAL`, `KP` |
| `GET` | imprime todos los parámetros |
| `SAVE` | guarda los parámetros en el almacén |
| `SENS` | lectura de los 3 sensores (también hay `SENS ON` para mandarlas en continuo) |

Los parámetros se guardan en EEPROM o Preferences, así que **calibrar no implica recompilar**. Se flashea poco y todo lo demás se ajusta por comandos.

### 4.5 Restricciones

| | UNO | ESP32-S3 N16R8 |
|---|---|---|
| RAM | 2 KB: arreglos fijos, nada de `String` ni STL, `F("...")` en los prints | 512 KB + 8 MB de PSRAM: sin restricciones prácticas |
| Planificar a bordo | Solo BFS (opcional) | Se pueden portar BFS, UCS y A* si sobra tiempo |
| Flasheo | USB | USB la primera vez, después **OTA por Wi-Fi** |

El código de `src/planner/` no puede incluir `Arduino.h`, para que compile con `g++` en el PC y se compare contra Python.

## 5. Plataforma de la demo: ESP32-S3

**Decidido el 25/10/2026 (DEC-06): la demo va con la ESP32-S3.** Ya está montada en el carro, con Wi-Fi, OTA y control remoto funcionando. El UNO queda como **plan B**: el mismo firmware compila para las dos placas, así que se puede volver a montar si la S3 falla.

### 5.1 Fases del ESP32-S3

| Fase | Qué | Hecho cuando |
|---|---|---|
| E0 | Compilar el mismo sketch para "ESP32S3 Dev Module" (Flash 16MB, OPI PSRAM) | compila sin `#if` fuera de la HAL |
| E1 | Bring-up en banco: Blink con el RGB, Serial | sube y responde por el puerto COM |
| E2 | Sensores a 3.3V | contraste blanco/cinta medido; normalizado >5x |
| E3 | Motores con el L298N a 3.3V lógico | `prueba_motores` pasa igual que en el UNO |
| E4 | Alimentación: batería de 9V al jack DC del shield, GND común con el L298N | ~5V medidos en el shield sin la placa; sin reinicios (brownout) al arrancar los motores; regulador tibio, no hirviendo |
| E5 | Wi-Fi AP + comandos por Telnet/WebSocket + OTA | flashear y calibrar sin cable |
| E6 | Recalibrar y recorrer el mapa 1 | 3 de 3 recorridos completos, sin reinicios |

E2 va de primero a propósito: es el riesgo más grande, porque con el LED IR más débil se puede perder el contraste.

### 5.2 Plan B: volver al UNO

Para que el UNO siga siendo una opción real el día de la demo:

1. El firmware de `arduino/agente` tiene que seguir compilando para las dos placas (sin `#if` fuera de la HAL).
2. Llevar el UNO y los cables marcados; montarlo debe tomar menos de 15 minutos (mismo chasis).
3. Sin Wi-Fi, la calibración y la ruta van por Serial desde el laptop.

### 5.3 Compras y montaje

Comprado (20/10/2026): **kit ESP32-S3 N16R8 de 44 pines** (headers soldados, dos USB-C) **+ shield de expansión para ESP32-S3 de 44 pines** con jack DC (entrada de 6.5 a 9V) y filas G/V/S por GPIO.

| Qué | Nota |
|---|---|
| Kit ESP32-S3 N16R8 + shield de 44 pines | Comprado |
| Broche de batería de 9V con plug DC de 5.5 mm | Ya lo tenemos. Confirmar que el plug encaje en el jack del shield |
| Cable USB-C de datos | Ya lo tenemos |
| Batería de 9V de repuesto | Ya la tenemos |

Montaje:
```
Batería 9V ──► jack DC del shield ──► ESP32-S3 (regulador del shield)
4xAA (6V)  ──► L298N ──► motores
GND del shield ──► GND del L298N (tierra común)
Jumper de los buses del shield en 3.3V ──► sensores (filas G/V/S)
```

Al recibirlo:
1. Identificar el regulador del shield (normalmente es un AMS1117-5.0).
2. Con la batería conectada y **sin la placa**, medir ~5V en el pin 5V/VIN del shield.
3. Verificar que el jumper de los buses esté en **3.3V** antes de conectar los sensores.
4. Montar la placa y dejarla unos minutos con el Wi-Fi prendido; el regulador puede estar tibio, pero no hirviendo.

Reglas:
- Los **GPIO 35, 36 y 37** salen en el shield, pero los usa la PSRAM de la N16R8: **no se usan**.
- La batería de 9V va solo al jack DC, **nunca directo a los pines** de la placa.
- Una batería de 9V nueva mide ~9.5V, un poco por encima de lo que dice el shield. Bajo carga baja a ~9V; si el regulador se calienta mucho, revisar.
- Programar por el USB-C "COM". La primera carga es por cable; después se programa por OTA.

No hace falta: HC-05, step-down, power bank ni convertidor de niveles.

## 6. Cronograma

| Fecha | Qué |
|---|---|
| Mar 20/10 | Circuito verificado con el UNO ✅, repo ✅ |
| Mié 21/10 | **Corte 1:** BFS en PC, formato del mapa |
| Jue 22/10 | Simulador, prueba de tinta |
| Vie 23/10 - Sáb 24/10 | ESP32-S3: E1-E5 (sensores, motores, alimentación, Wi-Fi, OTA, control remoto) ✅ |
| Dom 25/10 | DEC-06: la demo va con la S3 ✅. Estructura de `pc/` y BFS |
| Lun 26/10 | Firmware del agente, calibración por Wi-Fi, E6: recorrer el mapa 1 |
| Mar 27/10 | Comparación de algoritmos en el carro, ensayo en exterior, README e informe |
| Mié 28/10 | **Presentación** |

## 7. Riesgos

| Riesgo | Mitigación |
|---|---|
| La tinta del plotter no absorbe IR | Probarla ya (LAB-03); plan B con cinta aislante |
| El sol satura los sensores | Faldita de cartón; recalibrar el umbral con `SET UMBRAL` en el sitio |
| Los tiempos cambian con el voltaje de las pilas | Pilas nuevas; `SET T_CELDA` / `T_GIRO` en el sitio |
| El ESP32 pierde contraste a 3.3V | Medido en E2: blanco ~35-57 y cinta ~250-860, contraste suficiente. Recalibrar el umbral en el sitio |
| El ESP32 se reinicia por los picos de los motores | Alimentación separada (9V al shield para la lógica, AA para los motores) + condensador |
| La batería de 9V se agota rápido | **Pasó el 23/10:** con 7.67V en reposo, el ESP32 entró en bucle de brownout al prender el Wi-Fi. Medir antes de cada sesión (nueva ≈ 9.5V; cambiarla por debajo de ~8.5V). Potencia del Wi-Fi bajada a 8.5 dBm. Batería de repuesto en la demo |
| Motores sin torque (pitan al girar en el eje) | Son N20 6V 500 RPM (reducción baja) y el L298N se come ~2V: con 4xAA les llegan ~4V. Ya se agregó arranque con golpe y PWM mínimo para giros. Siguiente paso: 6xAA (9V) + `PWMMAX` ~85%. A futuro: N20 6V de 100-200 RPM **con encoder** |
| Brownout al conectar la batería | Es el rebote del plug en el jack: un solo reinicio al conectar no es problema. Meter el plug firme, de una vez |
| El puerto serial cambia en el Mac (`/dev/cu.usbserial-*`) | Los scripts reciben `--puerto`; probar el Mac con el UNO y el ESP32 antes de la demo |
| La S3 falla el día de la demo | Plan B con el UNO (sección 5.2) |
