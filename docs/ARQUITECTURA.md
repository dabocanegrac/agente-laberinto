# Arquitectura

Agente que resuelve un laberinto conocido: **percibir → planear → actuar**, todo dentro del carro.
El PC solo le manda el mapa; el carro calcula la ruta a bordo y la ejecuta.

Objetivos del diseño:

1. **El agente es autónomo:** recibe el mapa, planea y actúa sin depender del PC.
2. **Varios algoritmos comparables:** Python es la referencia para el simulador, la comparación y el informe; los que corren en el carro tienen que dar la misma ruta que su versión en Python (lo verifican los tests).

## 1. Capas

```
  PC                                   CARRO (ESP32-S3)
  maps/*.txt ──► enviar.py ──Wi-Fi──►  COMANDOS     (MAPA ... FIN, PLAN, GO, SET ...)
  (Python: referencia,                     │  mapa
   simulador, comparación)                 ▼
                                       PLANIFICADOR (BFS a bordo; misma ruta que Python)
                                           │  ruta: "F F L F R"
                                           ▼
                                       EJECUTOR     (sigue la ruta, corrige con sensores, log)
                                           │  avanzar_celda(), girar(), parar()
                                           ▼
                                       MOVIMIENTO   (movimientos de una celda con corrección)
                                           │
                                           ▼
                                       HAL          (motores, sensores, almacenamiento, red)
```

Reglas:
- Cada capa solo llama a la de abajo.
- El planificador no conoce el hardware y el ejecutor no sabe qué algoritmo produjo la ruta.
- El código de `src/planner/` no incluye `Arduino.h`: compila igual en el carro y en el PC, y así los tests lo comparan con Python.

## 2. Contratos compartidos

### 2.1 Mapa (DEC-01: grilla de 0/1)

Grilla `(2n+1) x (2m+1)` de 0/1. El PC (`pc/mapa.py`) y el carro (`src/planner/mapa.cpp`) leen exactamente el mismo archivo.

```
# maps/fisicos/mapa1.txt
inicio 0 0 E        # fila col orientación (N/E/S/O), en celdas
meta 1 1            # fila col, en celdas
11111
10001               # 1 = pared, 0 = libre; los espacios dentro de una fila se ignoran
11101
10001
11111
```

- La celda `(f, c)` está en la posición `(2f+1, 2c+1)` de la grilla; las paredes, entre celdas; las posiciones pares-pares son los postes.
- La fila 0 es la de arriba y la columna 0 la de la izquierda.
- Validaciones (iguales en los dos lectores): ancho y alto impares y de mínimo 3; todo el borde en 1; ninguna celda en 1; inicio y meta dentro del mapa.
- Tamaño máximo en el carro: 16x16 celdas (`MAX_FILAS`, `MAX_COLS` en `mapa.h`).

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
| `L`, `R` | `COSTO_GIRO` = 0.5 (≈ `T_GIRO90_MS / T_CELDA_MS`, medido en el carro) |

- **BFS** minimiza el número de acciones.
- **UCS y A\*** minimizan el costo, o sea el **tiempo real**, y prefieren rutas con menos giros.
- **DFS y Greedy** encuentran *una* ruta, no necesariamente la óptima.

Así la comparación del informe muestra diferencias reales que se pueden cronometrar con el carro.

Heurística para A\*: distancia Manhattan, más 1 giro si la meta no queda en la dirección actual. Es admisible.

## 3. Python (PC): referencia y herramientas

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
    optimiza: str | None        # "acciones", "costo" o None
    @abstractmethod
    def planear(self, mapa: Mapa, inicio: Estado, meta: Celda) -> Resultado | None: ...
```

Agregar un algoritmo es crear un archivo en `pc/planners/` con un `@registrar class Algo(Planner)`. No hay que tocar nada más.

```
python -m pc.planear maps/fisicos/mapa1.txt --algo all     # tabla comparativa
python -m pc.enviar maps/fisicos/mapa1.txt                 # manda el mapa al carro (o --puerto COM6)
python -m pc.imprimir maps/fisicos/mapa1.txt               # PDF para el plotter y plano para armarlo
```

**Tests** (`python -m pytest`):
- todos los algoritmos dan rutas válidas, y los que prometen ser óptimos lo son (contra un Dijkstra de referencia);
- `test_planner_cpp.py` compila el planificador del carro en el PC y exige **la misma ruta, costo y nodos expandidos** que `bfs.py` en todos los mapas, y los mismos errores con mapas malos;
- `test_enviar.py` prueba el protocolo contra un carro simulado;
- `test_imprimir.py` verifica que las piezas del PDF cubren exactamente las paredes, sin superponerse.

## 4. Firmware (ESP32-S3)

El Arduino IDE solo compila lo que está en la carpeta del sketch y en `src/` (de forma recursiva). Por eso todo el código vive dentro de `agente/src/`.

```
arduino/agente/
├── agente.ino                  # Wi-Fi, OTA, consola (Serial y TCP) y comandos
└── src/
    ├── config/
    │   ├── pines.h / pines_esp32s3.h
    │   └── parametros.h        # valores por defecto (tiempos, PWM, umbral)
    ├── planner/                # sin Arduino.h: compila también en el PC
    │   ├── mapa.h/.cpp         # LectorMapa: valida la grilla línea por línea
    │   └── bfs.h/.cpp          # BFS sobre (fila, col, orientación), arreglos estáticos
    ├── hal/                    # (por hacer) motores, sensores normalizados 0..1000
    ├── movimiento.h/.cpp       # (por hacer) avanzar_celda(), girar(), con corrección
    └── ejecutor.h/.cpp         # (por hacer) recorre la ruta, maneja estados y log
```

### 4.1 Pines

| Función | GPIO |
|---|---|
| ENA / IN1 / IN2 (motor A = llanta derecha) | 7 / 15 / 16 |
| ENB / IN3 / IN4 (motor B = llanta izquierda) | 8 / 17 / 18 |
| Sensor izq / centro / der | 4 / 5 / 6 (ADC1), alimentados a 3.3V |
| LED de estado | 48 (RGB) |

Adelante es IN1=H/IN2=L (derecha) e IN3=H/IN4=L (izquierda), sin invertir motores. No usar GPIO 35-37 (PSRAM), 0/3/45/46 (arranque), 19/20 (USB nativo) ni 43/44 (UART).

### 4.2 Compilar y subir

```
FQBN = esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=default,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn $FQBN --output-dir build/agente arduino/agente
arduino-cli upload  -p COM6 --fqbn $FQBN arduino/agente                       # por cable (puerto "COM", chip CH343)
espota -i 192.168.4.1 -I <ip del PC> -p 3232 -a <clave OTA> -f build/agente/agente.ino.bin -r   # por Wi-Fi
```

- `CDCOnBoot=default` porque se programa por el USB-C "COM" (UART). Con el USB nativo sería `cdc`.
- `app3M_fat9M_16MB` aprovecha los 16 MB: 3 MB por app y dos slots para OTA.
- `espota` está en `Arduino15/packages/esp32/hardware/esp32/<versión>/tools/`.
- **Firewall de Windows:** en el OTA, el ESP32 abre una conexión de vuelta hacia el PC. Windows la bloquea si `espota.exe` no tiene una regla de entrada **para el perfil de la red del carro**. Al primer intento aparece una ventana para permitirlo.
- `control_remoto` y `agente` usan la misma red y el mismo puerto: en el carro va uno a la vez.

### 4.3 Sensores normalizados

`sensores.leer()` devuelve valores de **0 a 1000**: `crudo * 1000 / 4095`, con la polaridad aplicada para que 1000 sea negro. Los umbrales se configuran en esa escala. Referencia (23/10/2026): blanco ~35-57, cinta aislante ~250-860, aire 1000.

### 4.4 Protocolo de comandos

Texto, una línea por comando, por `Serial` (115200) o por TCP en `192.168.4.1:23` (red `agente-laberinto`). Responde `OK ...`, `ERR ...` o `RUTA ...`; el log empieza con `#`.

| Comando | Qué hace | Estado |
|---|---|---|
| `MAPA` + líneas + `FIN` | recibe el mapa, lo valida, lo guarda (sobrevive a un reinicio) y responde con la ruta | ✅ |
| `PLAN` | vuelve a calcular la ruta con el mapa guardado | ✅ |
| `RUTA` | imprime la última ruta | ✅ |
| `COSTO_GIRO <v>` | costo de L/R y vuelve a planear | ✅ |
| `INFO` / `STOP` | estado / parar motores | ✅ |
| `GO` | ejecuta la ruta | por hacer |
| `PASO F` | ejecuta un solo movimiento (para calibrar) | por hacer |
| `SET <param> <valor>` / `GET` / `SAVE` | `T_CELDA`, `T_GIRO`, `PWM`, `COMP_IZQ`, `COMP_DER`, `UMBRAL`, `KP` | por hacer |
| `SENS` | lectura de los 3 sensores | por hacer |

Ejemplo:
```
> MAPA
> inicio 0 0 E
> meta 1 1
> 11111
> 10001
> 11101
> 10001
> 11111
> FIN
< OK MAPA 2x2 inicio=0,0,E meta=1,1
< RUTA F R F
< # acciones=3 costo=2.5 expandidos=8 tiempo=... us
```

Los parámetros se guardan en Preferences, así que **calibrar no implica recompilar**.

### 4.5 Alinear con las paredes

Con el carro centrado en una celda (eje de las ruedas en el centro), el sensor frontal queda 16 cm adelante: 4 cm más allá del centro de la línea de enfrente (celda de 24 cm, media celda = 12 cm). Al avanzar hacia una pared, el sensor frontal pisa la línea cuando al eje le faltan ~4 cm para el centro: **al ver la línea, avanzar ~4 cm y parar** (el valor exacto se calibra). Eso borra el error acumulado de avanzar por tiempo. El sensor frontal se ignora durante los giros.

## 5. Laberinto físico

- **DEC-02: celda de 24 cm** entre centros de líneas, línea de 2 cm, margen blanco de 5 cm. Quedan 22 cm de blanco entre paredes: con 16 cm entre los sensores laterales hay **3 cm de tolerancia a cada lado** antes de pisar una línea, y 3.5 cm para las ruedas.
- Tamaño: un laberinto de `c` columnas mide `24c + 2` cm de ancho. En un rollo de plotter de 91 cm caben 3 columnas (74 cm + margen); el largo no tiene límite. Con cinta o cartulina no hay límite.
- `pc/imprimir.py` genera el PDF 1:1 para el plotter y un plano con medidas y lista de piezas para armarlo con cinta aislante o tiras de cartulina negra (DEC-03).
- La tinta, la cinta o la cartulina tienen que ser **mate** y absorber IR: probarlas con los sensores (> 400) adentro y al sol.

## 6. Cronograma

| Fecha | Qué |
|---|---|
| Mar 20/10 | Circuito verificado ✅, repo ✅ |
| Vie 23/10 - Sáb 24/10 | ESP32-S3: sensores, motores, alimentación, Wi-Fi, OTA, control remoto ✅ |
| Dom 25/10 | Estructura de `pc/`, BFS; mapa en grilla; el carro recibe el mapa y planea a bordo ✅ |
| Lun 26/10 | Decisiones pendientes cerradas, fecha confirmada con el profe |
| Mar 27/10 | Mapa 1 diseñado, prueba de tinta / cartulina |
| Mié 28/10 | **Corte:** simulador y calibración por Wi-Fi |
| Jue 29/10 | Laberinto impreso o armado, ejecutor en el firmware, A* |
| Sáb 31/10 | El carro recorre el mapa 1 (3 de 3) |
| Dom 1/11 | Ensayo en exterior |
| Lun 2/11 | Comparación de algoritmos en el carro, diapositivas, video |
| Mar 3/11 | Informe en PDF, README final, ensayo general |
| Mié 4/11 | **Presentación** (por confirmar) |

## 7. Riesgos

| Riesgo | Mitigación |
|---|---|
| La tinta del plotter no absorbe IR | Probarla ya (LAB-03); plan B con cinta aislante o cartulina negra mate (`pc/imprimir.py` da el plano) |
| El sol satura los sensores | Bajar los sensores a 5-8 mm (hoy están a ~1.5 cm) y ponerles faldita; recalibrar el umbral en el sitio |
| Los tiempos cambian con el voltaje de las pilas | Pilas nuevas; `SET T_CELDA` / `T_GIRO` en el sitio; alinear con las paredes (4.5) |
| La ruta del carro no coincide con la de Python | `test_planner_cpp.py` lo revisa en cada PR; `pc.enviar` lo vuelve a comparar con el carro real |
| El ESP32 se reinicia por los picos de los motores | Alimentación separada (9V al shield para la lógica, AA para los motores) + condensador |
| La batería de 9V se agota rápido | **Pasó el 23/10:** con 7.67V en reposo, el ESP32 entró en bucle de brownout al prender el Wi-Fi. Medir antes de cada sesión (nueva ≈ 9.5V; cambiarla por debajo de ~8.5V). Potencia del Wi-Fi bajada a 8.5 dBm. Batería de repuesto en la demo |
| Motores sin torque (pitan al girar en el eje) | Son N20 6V 500 RPM y el L298N se come ~2V: con 4xAA les llegan ~4V. Siguiente paso: 6xAA (9V) + `PWMMAX` ~85 %. A futuro: N20 de 100-200 RPM **con encoder** |
| Brownout al conectar la batería | Es el rebote del plug en el jack: un solo reinicio al conectar no es problema. Meter el plug firme, de una vez |
| El puerto serial cambia en el Mac (`/dev/cu.usbserial-*`) | `pc.enviar --puerto`; por Wi-Fi no hace falta |
