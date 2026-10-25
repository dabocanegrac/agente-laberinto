# agente-laberinto

Agente que resuelve un laberinto impreso: conoce el mapa de antemano, planea la ruta con **BFS** (A* opcional) y la ejecuta en un carro 2WD con ESP32-S3 (Arduino UNO como plan B), usando sensores IR para detectar paredes y corregir la deriva.

Ciclo del agente: **percibir → planear → actuar**.

Proyecto de la clase de IA, Universidad Nacional.

## Estructura

```
agente-laberinto/
├── maps/
│   ├── fisicos/          # mapas que se imprimen para la demo (+ .esperado con la ruta de BFS)
│   └── simulados/        # mapas para probar y comparar algoritmos
├── pc/                   # Python: planificador, simulador, envío al carro
│   ├── mapa.py           # leer y validar el formato del mapa
│   ├── modelo.py         # estado (fila, col, orientación), acciones F/L/R y costos
│   ├── planners/         # un archivo por algoritmo (bfs.py, ...)
│   ├── planear.py        # CLI: python -m pc.planear <mapa> --algo <nombre|all>
│   └── tests/            # corren solos con todos los algoritmos y todos los mapas
├── resultados/           # tablas de comparación para el informe
├── arduino/
│   ├── agente/           # firmware final
│   ├── control_remoto/   # manejar y calibrar el carro por Wi-Fi
│   └── pruebas/          # sketches sueltos, uno por carpeta
└── docs/                 # arquitectura, informe, fotos
```

## Cómo agregar un algoritmo

1. Crear `pc/planners/<nombre>.py` con una clase `@registrar` que herede de `Planner` (ver el ejemplo en `pc/planners/bfs.py` y el contrato en `pc/planners/base.py`).
2. Poner `optimiza = "acciones"`, `"costo"` o `None` según lo que garantice el algoritmo. Los tests verifican esa promesa.
3. Correr `python -m pytest`. No hay que tocar nada más: `planear.py` y los tests lo encuentran solos.

## Cómo trabajamos con git

- `main` está protegida: todo entra por **pull request** con **1 aprobación** de otro integrante.
- Una rama por cosa: `algo/astar`, `firmware/movimiento`, `mapas/mapa1`, ...
- Ramas cortas y PRs pequeños; se mezclan con *squash* y la rama se borra sola.
- Antes de abrir el PR: `python -m pytest` en verde.

## Contratos

1. **Formato del mapa:** pendiente de confirmar con el grupo. Opciones:
   - Grilla `(2n+1) x (2n+1)` de 0/1, donde las paredes ocupan las posiciones impares.
   - Máscara de bits de paredes por celda: N=1, E=2, S=4, O=8.

   Python y Arduino leen exactamente el mismo archivo (`maps/mapa1.txt`).
2. **Salida del planificador:** lista de movimientos relativos, por ejemplo `F F L F R`.
   - `F`: avanzar una celda
   - `L` / `R`: girar 90° a la izquierda / derecha
3. **Interfaz de movimiento (Arduino):** `avanzar()`, `girar(IZQ|DER)`, `parar()`. Los pines están en `arduino/agente/src/config/pines*.h` y los valores por defecto en `parametros.h`.

## Hardware

| Pieza | Detalle |
|---|---|
| Placa | ESP32-S3 N16R8 de 44 pines + shield de expansión con jack DC (6.5-9V). Plan B: Arduino UNO R3 (CH340) |
| Chasis | Acrílico GPR V2.0 2WD |
| Motores | 2x motorreductor N20-10, 6V, 500 RPM, sin encoders (rápido y de poco torque) |
| Llantas | 43 mm de diámetro, 19 mm de ancho, eje D de 3 mm (≈1.1 m/s a 500 RPM) |
| Driver | L298N |
| Sensores | 3x TCRT5000, salida analógica (AO) |
| Energía motores | 4xAA en serie (6V) al borne 12V del L298N |
| Energía placa | Batería de 9V al jack DC del shield (o del UNO). **Nunca USB y batería al tiempo** |

## Cableado

Los pines están en `arduino/agente/src/config/` (`pines.h` elige el archivo según la placa).

| Función | ESP32-S3 (GPIO) | UNO |
|---|---|---|
| ENA (PWM motor A = llanta derecha) | 7 | D5 |
| IN1 / IN2 | 15 / 16 | D7 / D8 |
| ENB (PWM motor B = llanta izquierda) | 8 | D6 |
| IN3 / IN4 | 17 / 18 | D9 / D10 |
| Sensor izquierdo / centro / derecho (AO) | 4 / 5 / 6 | A0 / A1 / A2 |
| Alimentación de los sensores | 3.3V (filas G/V/S del shield) | 5V |

- Jumpers ENA, ENB y 5V del L298N quitados.
- El borne 5V del L298N se alimenta desde el 5V de la placa (shield o UNO).
- Tierra común entre L298N, placa y sensores.
- Adelante = IN1=H/IN2=L (derecha) e IN3=H/IN4=L (izquierda), sin invertir motores.
- En la ESP32-S3 no usar GPIO 35-37 (PSRAM), 0/3/45/46 (arranque), 19/20 (USB nativo) ni 43/44 (UART).
- Motor A en OUT1-OUT2, motor B en OUT3-OUT4.

## Cómo correr

### PC (Python 3)

Python 3.10 o más reciente. Desde la raíz del repo:

```
pip install pytest
python -m pc.planear maps/simulados/ejemplo_5x5.txt              # ruta de BFS
python -m pc.planear maps/simulados/ejemplo_5x5.txt --algo all   # tabla con todos los algoritmos
python -m pytest                                                 # tests
```

### ESP32-S3

Core `esp32:esp32` 3.3.x. Se programa por el USB-C marcado "COM" (chip CH343). Monitor Serial a 115200 baudios.

```
FQBN=esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=default,PartitionScheme=app3M_fat9M_16MB
arduino-cli compile --fqbn $FQBN --output-dir build/<sketch> arduino/<ruta>/<sketch>
arduino-cli upload -p <puerto> --fqbn $FQBN arduino/<ruta>/<sketch>
```

En el IDE: placa "ESP32S3 Dev Module", Flash Size 16MB, PSRAM "OPI PSRAM", Partition Scheme "16M Flash (3MB APP/9.9MB FATFS)". Después de la primera carga se puede flashear por Wi-Fi (OTA), ver `docs/ARQUITECTURA.md`.

### Arduino UNO (plan B)

Placa "Arduino Uno" y el puerto COM del CH340.

### Sketches

| Sketch | Placa | Para qué |
|---|---|---|
| `arduino/control_remoto` | S3 | Manejar el carro desde el celular y ver los sensores. Red `agente-laberinto`, página en http://192.168.4.1 |
| `arduino/pruebas/diagnostico_esp32s3` | S3 | Lectura de los sensores por Serial |
| `arduino/pruebas/prueba_motores_esp32s3` | S3 | Identificar motor izquierdo/derecho y sentido de giro |
| `arduino/pruebas/prueba_alimentacion_esp32s3` | S3 | Arranques de motores para buscar reinicios por brownout |
| `arduino/pruebas/wifi_ota_esp32s3` | S3 | Wi-Fi AP, comandos por TCP y OTA |
| `arduino/pruebas/diagnostico` | UNO | Vcc y sensores por Serial |
| `arduino/pruebas/prueba_motores` | UNO | Igual que la versión S3 |
| `arduino/agente` | las dos | Firmware final (en construcción) |

## Calibración en el sitio

Los tiempos de movimiento y los umbrales de los sensores dependen de las pilas, la tinta y la luz. Calibrar siempre en el sitio de la demo y con pilas nuevas.
