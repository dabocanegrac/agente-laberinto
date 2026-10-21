# agente-laberinto

Agente que resuelve un laberinto impreso: conoce el mapa de antemano, planea la ruta con **BFS** (A* opcional) y la ejecuta en un carro 2WD con Arduino UNO, usando sensores IR para detectar paredes y corregir la deriva.

Ciclo del agente: **percibir → planear → actuar**.

Proyecto de la clase de IA, Universidad Nacional.

## Estructura

```
agente-laberinto/
├── maps/                 # mapas en el formato compartido (PC y Arduino)
├── pc/
│   ├── bfs.py            # BFS: mapa -> lista de movimientos
│   ├── astar.py          # opcional, con métricas
│   └── simulador.py      # dibuja el mapa y ejecuta la lista de movimientos
├── arduino/
│   ├── pruebas/          # sketches sueltos: motores, sensores, calibración
│   └── agente/           # sketch final: PLANEAR -> EJECUTAR
└── docs/                 # informe PDF, fotos y videos de pruebas
```

## Contratos

1. **Formato del mapa:** pendiente de confirmar con el grupo. Opciones:
   - Grilla `(2n+1) x (2n+1)` de 0/1, donde las paredes ocupan las posiciones impares.
   - Máscara de bits de paredes por celda: N=1, E=2, S=4, O=8.

   Python y Arduino leen exactamente el mismo archivo (`maps/mapa1.txt`).
2. **Salida del planificador:** lista de movimientos relativos, por ejemplo `F F L F R`.
   - `F`: avanzar una celda
   - `L` / `R`: girar 90° a la izquierda / derecha
3. **Interfaz de movimiento (Arduino):** `avanzar()`, `girar(IZQ|DER)`, `parar()`. Todos los pines y constantes están en `arduino/agente/pines.h`.

## Hardware

| Pieza | Detalle |
|---|---|
| Placa | Arduino UNO R3 compatible (ATmega328P, CH340). En el IDE: "Arduino Uno" |
| Chasis | Acrílico GPR V2.0 2WD |
| Motores | 2x motorreductor TT 6V 1:48, sin encoders |
| Llantas | 6.5 cm de diámetro |
| Driver | L298N |
| Sensores | 3x TCRT5000, salida analógica (AO) |
| Energía motores | 4xAA en serie (6V) al borne 12V del L298N |
| Energía Arduino | Batería de 9V al jack |

## Cableado

| Función | Pin Arduino |
|---|---|
| ENA (PWM motor A) | D5 |
| IN1 | D7 |
| IN2 | D8 |
| IN3 | D9 |
| IN4 | D10 |
| ENB (PWM motor B) | D6 |
| Sensor izquierdo (AO) | A0 |
| Sensor centro (AO) | A1 |
| Sensor derecho (AO) | A2 |

- Jumpers ENA, ENB y 5V del L298N quitados.
- El borne 5V del L298N se alimenta desde el 5V del Arduino.
- Tierra común entre L298N, Arduino y sensores.
- Motor A en OUT1-OUT2, motor B en OUT3-OUT4.

## Cómo correr

### PC (Python 3)

```
python pc/bfs.py maps/mapa1.txt
python pc/simulador.py maps/mapa1.txt
```

### Arduino

Abrir el sketch en el Arduino IDE, seleccionar la placa "Arduino Uno" y el puerto COM del CH340, y subir. Monitor Serial a 115200 baudios.

Orden sugerido de pruebas:
1. `arduino/pruebas/prueba_motores`: identificar motor izquierdo/derecho y sentido de giro.
2. `arduino/pruebas/prueba_sensores`: calibrar umbral y polaridad (Serial Plotter).
3. `arduino/pruebas/calibracion_movimiento`: encontrar `T_CELDA_MS` y `T_GIRO90_MS`.
4. `arduino/agente`: recorrido completo.

## Calibración en el sitio

Los tiempos de movimiento y los umbrales de los sensores dependen de las pilas, la tinta y la luz. Calibrar siempre en el sitio de la demo y con pilas nuevas.
