# agente-laberinto

Agente que resuelve un laberinto impreso. Conoce el mapa de antemano: **el carro recibe el mapa, calcula la ruta a bordo con BFS** y la ejecuta en un carro 2WD con ESP32-S3, usando sensores IR para detectar las paredes y corregir la deriva.

Ciclo del agente: **percibir → planear → actuar**, todo dentro del carro.

Proyecto de la clase de IA, Universidad Nacional.

## Estructura

```
agente-laberinto/
├── maps/
│   ├── fisicos/          # mapas que se imprimen para la demo (+ .esperado con la ruta de BFS)
│   └── simulados/        # mapas para probar y comparar algoritmos
├── pc/                   # Python: referencia de los algoritmos, herramientas y tests
│   ├── mapa.py           # leer y validar el mapa (grilla de 0/1)
│   ├── modelo.py         # estado (fila, col, orientación), acciones F/L/R y costos
│   ├── planners/         # un archivo por algoritmo (bfs.py, ...)
│   ├── planear.py        # python -m pc.planear <mapa> --algo <nombre|all>
│   ├── enviar.py         # python -m pc.enviar <mapa>: manda el mapa al carro
│   ├── imprimir.py       # python -m pc.imprimir <mapa>: PDF para el plotter o para armarlo
│   └── tests/            # corren solos con todos los algoritmos y todos los mapas
├── resultados/           # tablas de comparación para el informe
├── arduino/
│   ├── agente/           # firmware del carro: recibe el mapa, planea a bordo y recorre la ruta
│   │   ├── src/planner/  #   lector del mapa y BFS en C++ (misma ruta que Python, lo verifican los tests)
│   │   └── src/imu/      #   giroscopio MPU6050: giros por ángulo y rumbo al avanzar
│   ├── control_remoto/   # manejar el carro y ver los sensores desde el celular
│   └── pruebas/          # sketches sueltos, uno por carpeta
└── docs/                 # arquitectura, informe, fotos
```

## Contratos

1. **Mapa** (`maps/**/*.txt`): grilla `(2n+1) x (2n+1)` de 0/1. La celda `(f, c)` está en la posición `(2f+1, 2c+1)` y las paredes quedan entre las celdas. El PC y el carro leen exactamente el mismo archivo.

   ```
   # comentario
   inicio 0 0 E        fila col orientación (N/E/S/O), en celdas
   meta 1 1            fila col, en celdas
   11111
   10001               1 = pared, 0 = libre (los espacios se ignoran)
   11101
   10001
   11111
   ```
2. **Ruta:** lista de movimientos relativos, por ejemplo `F F L F R`. `F` avanza una celda; `L` / `R` giran 90° a la izquierda / derecha.
3. **Dónde se planea:** en el carro. El PC manda el mapa y el carro responde con la ruta. Python es la referencia: el BFS en C++ del carro tiene que dar exactamente la misma ruta (`pc/tests/test_planner_cpp.py`).

## Cómo agregar un algoritmo

1. Crear `pc/planners/<nombre>.py` con una clase `@registrar` que herede de `Planner` (ver el ejemplo en `pc/planners/bfs.py` y el contrato en `pc/planners/base.py`).
2. Poner `optimiza = "acciones"`, `"costo"` o `None` según lo que garantice el algoritmo. Los tests verifican esa promesa.
3. Correr `python -m pytest`. No hay que tocar nada más: `planear.py` y los tests lo encuentran solos.
4. Para correrlo también en el carro: portarlo a `arduino/agente/src/planner/` y agregarlo a `test_planner_cpp.py`.

## Cómo trabajamos con git

- `main` está protegida: todo entra por **pull request** con **1 aprobación** de otro integrante.
- Una rama por cosa: `algo/astar`, `firmware/movimiento`, `mapas/mapa1`, ...
- Ramas cortas y PRs pequeños; se mezclan con *squash* y la rama se borra sola.
- Antes de abrir el PR: `python -m pytest` en verde.

## Cómo correr

### PC (Python 3.10 o más reciente)

Desde la raíz del repo:

```
pip install pytest ziglang reportlab
python -m pc.planear maps/simulados/ejemplo_5x5.txt              # ruta de BFS
python -m pc.planear maps/simulados/ejemplo_5x5.txt --algo all   # tabla con todos los algoritmos
python -m pytest                                                 # tests
```

`ziglang` trae un compilador de C++ para probar el planificador del carro en el PC; si ya tienen `g++` o `clang++` no hace falta.

### Mandar el mapa al carro

Conectarse a la red Wi-Fi `agente-laberinto` (clave `laberinto2026`) con el carro prendido y el firmware `arduino/agente`:

```
python -m pc.enviar maps/simulados/ejemplo_5x5.txt
```

El carro valida el mapa, lo guarda (sobrevive a un reinicio), calcula la ruta y la devuelve. `enviar.py` la compara con la de Python. Por cable: `--puerto COM6` (o `/dev/cu.wchusbserial...` en el Mac; `pip install pyserial`).

Para calibrar y correr la ruta: `python -m pc.consola` (por Wi-Fi) o `python -m pc.consola --puerto <puerto>` (por cable). Una línea vacía manda `STOP`. Los comandos están en `docs/ARQUITECTURA.md` (4.4) y el paso a paso en `docs/HANDOFF.md`.

**Hotspot del celular (recomendado):** así el PC queda en la misma red que el carro sin perder internet. Se configura una vez con `RED_SSID <nombre>`, `RED_CLAVE <clave>` y `RED CONECTAR` (quedan en la memoria del carro, no en el código). El hotspot tiene que dar 2.4 GHz. El carro mantiene también su red propia de respaldo. `pc.enviar` y `pc.consola` encuentran el carro solos: la última IP conocida (`~/.agente-laberinto-ip`), la red propia o barriendo la subred del hotspot, porque el hotspot de Android no deja pasar mDNS ni broadcast.

En el Mac, la terminal necesita el permiso de **Red local** (Ajustes → Privacidad y seguridad). Sin él, Python da `No route to host` aunque `ping` funcione.

### Imprimir o armar el laberinto

```
python -m pc.imprimir maps/fisicos/mapa1.txt --celda 24 --linea 2 --margen 5
```

Genera en `build/impresion/`:
- `<mapa>_plotter.pdf`: escala 1:1 en una sola página. Imprimir **al 100 %** y comprobar la regla de 10 cm del borde.
- `<mapa>_armado.pdf`: plano con medidas, lista de piezas y materiales, para armarlo con **cinta aislante** o con **tiras de cartulina negra** sobre una base blanca.

La cinta o la cartulina tienen que ser **mate** y absorber IR: probarlas antes con los sensores (deben dar > 400).

**La tinta del plotter no sirve:** es invisible al IR de los TCRT5000 (lee igual que el papel blanco, 26/10/2026). El impreso se usa como guía y las líneas se forran con cinta aislante (~1.9 cm, centrada sobre la línea de 2 cm), sin arrugas y pasándola ~1 cm en las esquinas.

### ESP32-S3

Core `esp32:esp32` 3.3.x. Se programa por el USB-C marcado "COM" (chip CH343). Monitor Serial a 115200 baudios.

```
FQBN=esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=default,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn $FQBN --output-dir build/<sketch> arduino/<ruta>/<sketch>
arduino-cli upload -p <puerto> --fqbn $FQBN arduino/<ruta>/<sketch>
```

En el IDE: placa "ESP32S3 Dev Module", Flash Size 16MB, PSRAM "OPI PSRAM", Partition Scheme "16M Flash (3MB APP/9.9MB FATFS)". Después de la primera carga se puede flashear por Wi-Fi (OTA), ver `docs/ARQUITECTURA.md`.

En el Mac con Apple Silicon, el `ctags` del core es x86: instalar Rosetta (`softwareupdate --install-rosetta`). Para el cable, el driver de WCH (`brew install --cask wch-ch34x-usb-serial-driver`) y aprobar la extensión en Ajustes.

### Sketches

| Sketch | Para qué |
|---|---|
| `arduino/agente` | Firmware del carro: recibe el mapa, calcula la ruta y la recorre (`GO`). Comandos por TCP en 192.168.4.1:23 |
| `arduino/control_remoto` | Manejar el carro desde el celular y ver los sensores. Página en http://192.168.4.1 |
| `arduino/pruebas/diagnostico_esp32s3` | Lectura de los sensores por Serial |
| `arduino/pruebas/prueba_motores_esp32s3` | Identificar motor izquierdo/derecho y sentido de giro |
| `arduino/pruebas/prueba_alimentacion_esp32s3` | Arranques de motores para buscar reinicios por brownout |
| `arduino/pruebas/wifi_ota_esp32s3` | Wi-Fi AP, comandos por TCP y OTA |

## Hardware

| Pieza | Detalle |
|---|---|
| Placa | ESP32-S3 N16R8 de 44 pines + shield de expansión con jack DC (6.5-9V) |
| Chasis | 18 x 10 cm; 15 cm de ancho con ruedas; 20 cm de largo con el sensor frontal |
| Motores | 2x motorreductor N20-10, 6V, 500 RPM, sin encoders (rápido y de poco torque) |
| Llantas | 43 mm de diámetro, 19 mm de ancho, eje D de 3 mm (≈1.1 m/s a 500 RPM) |
| Driver | L298N |
| Sensores | 3x TCRT5000, salida analógica (AO). 16 cm entre las puntas de los laterales; el frontal a 16 cm del eje de las ruedas |
| Giroscopio | MPU6050 (módulo GY-521) por I2C, plano con el chip hacia arriba, pegado con cinta doble faz de espuma |
| Energía motores | 4xAA en serie (6V) al borne 12V del L298N |
| Energía placa | Batería de 9V al jack DC del shield. **Nunca USB y batería al tiempo** |

## Cableado

Los pines están en `arduino/agente/src/config/pines_esp32s3.h`.

| Función | GPIO |
|---|---|
| ENA (PWM motor A = llanta derecha) | 7 |
| IN1 / IN2 | 15 / 16 |
| ENB (PWM motor B = llanta izquierda) | 8 |
| IN3 / IN4 | 17 / 18 |
| Sensor izquierdo / centro / derecho (AO) | 4 / 5 / 6 |
| Alimentación de los sensores | 3.3V (filas G/V/S del shield) |
| Giroscopio SDA / SCL (VCC a 3.3V; AD0, XDA, XCL e INT sin conectar) | 9 / 10 |
| LED RGB (WS2812) | 48 |

- Jumpers ENA, ENB y 5V del L298N quitados.
- El borne 5V del L298N se alimenta desde el 5V del shield. **Si ese cable se suelta, los motores no responden** (el L298N se queda sin lógica): con 4xAA su regulador interno no alcanza a dar 5V.
- El LED RGB de esta placa clon no prende hasta cerrar con estaño el puente de soldadura marcado `RGB`. El LED rojo fijo es el de power y el verde que titila es el TX del CH343 (si titila sin parar, el ESP32 está en un bucle de brownout).
- Tierra común entre L298N, placa y sensores.
- Adelante = IN1=H/IN2=L (derecha) e IN3=H/IN4=L (izquierda), sin invertir motores.
- No usar GPIO 35-37 (PSRAM), 0/3/45/46 (arranque), 19/20 (USB nativo) ni 43/44 (UART).
- Motor A en OUT1-OUT2, motor B en OUT3-OUT4.

## Calibración en el sitio

Los tiempos de movimiento y los umbrales de los sensores dependen de las pilas, la tinta y la luz. Calibrar siempre en el sitio de la demo y con pilas nuevas.

Los valores por defecto de `arduino/agente/src/config/parametros.h` son los que funcionaron el 26/10/2026 en el impreso de prueba (5 de 5 corridas en la meta, ~14 s). Los giros dependen del giroscopio y no de los tiempos, así que en otro sitio normalmente solo hay que recalibrar `umbral`, `t_celda` y `t_alinear` (paso a paso en `docs/HANDOFF.md`).
