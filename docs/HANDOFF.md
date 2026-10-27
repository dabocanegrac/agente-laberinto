# Handoff: el carro recorre el laberinto impreso (26/10/2026)

Esto es para seguir en otra máquina (con o sin Claude Code) sin perder el contexto.
Si usas Claude Code allá, pídele que lea este archivo, el `README.md` y `docs/ARQUITECTURA.md`.

## Dónde estamos

- **El 26/10 el carro recorrió el laberinto de prueba:** 6 de 6 corridas en la meta, en ~14 s, con giros a ±4°.
- **El laberinto impreso** es `maps/simulados/prueba_4x4.txt` en la versión para rollo de 91 cm:
  **celda de 22 cm**, línea de 2 cm y margen de 0.5 cm (91 x 91 cm).
  - No es el mapa 1 de la demo, que se arma con celda de 24 cm (DEC-02).
  - Arriba del papel es el norte, porque la fila 0 está arriba. El texto gris del borde queda abajo a la izquierda.
  - **La tinta del plotter es invisible al IR:** las líneas se forraron con cinta aislante.
- **Firmware `arduino/agente` (rama `firmware/ejecutor`, PR #5):**
  - Recibe el mapa, planea con BFS a bordo y recorre la ruta con `GO`.
  - Giros por **giroscopio** (MPU6050) con corrección por pulsos, rumbo mantenido al avanzar, freno con contramarcha y aproximación lenta a las paredes (ver `docs/ARQUITECTURA.md`, 4.5 y 4.6).
  - Los valores por defecto de `src/config/parametros.h` son los calibrados ese día.
- **El hardware:** pines, alimentación, sensores y giroscopio están en el README.
  - Batería de 9 V al jack y 4xAA a los motores. **Nunca conectar USB y batería al tiempo.**
  - La 9 V se agota rápido: si el carro se desconecta y el LED verde (TX) titila sin parar, está en bucle de brownout. `INFO` dice `reset=BROWNOUT`. Cambiarla.
  - Si los motores no responden, revisar el cable de 5 V del shield al L298N.

## Montar la otra máquina

1. Clonar el repo y pasarse a la rama:
   ```
   git clone https://github.com/dabocanegrac/agente-laberinto.git
   cd agente-laberinto
   git switch firmware/ejecutor
   ```
2. Instalar Python 3.10 o más nuevo y las dependencias:
   ```
   pip install pytest ziglang reportlab pyserial
   python -m pytest
   ```
   Tiene que dar 30 pasan y 2 saltados.
3. Instalar el Arduino IDE 2 (o `arduino-cli`) y agregarle el core **esp32 de Espressif**, versión 3.3.12.
   - Si es con el IDE: en Preferencias, agregar la URL `https://espressif.github.io/arduino-esp32/package_esp32_index.json` y luego instalar "esp32" desde el gestor de placas.
   - En un Mac con Apple Silicon hace falta Rosetta (`softwareupdate --install-rosetta`): el `ctags` del core es x86.
4. Configurar la placa como **ESP32S3 Dev Module**:
   - Flash Size: 16MB
   - PSRAM: OPI PSRAM
   - Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
   - USB CDC On Boot: Disabled (el default)
   - Con `arduino-cli`, el FQBN es:
     ```
     esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=default,PartitionScheme=app3M_fat9M_16MB
     ```
5. En el Mac: darle a la terminal el permiso de **Red local** (Ajustes → Privacidad y seguridad). Si Python da `No route to host` con el carro y `ping` sí funciona, es eso: apagar y prender el permiso y reabrir la terminal con Cmd+Q.

## Subir el firmware

**Por Wi-Fi:**
1. Prender el carro con la batería y conectar la máquina a la red `agente-laberinto` (clave `laberinto2026`).
2. Compilar y subir con `espota` (está en `Arduino15/packages/esp32/hardware/esp32/3.3.12/tools/`):
   ```
   arduino-cli compile --fqbn <FQBN> --output-dir build/agente arduino/agente
   python3 espota.py -i 192.168.4.1 -I <ip de esta máquina, 192.168.4.x> -p 3232 -a laberinto2026 -f build/agente/agente.ino.bin -r
   ```
   `arduino-cli upload --protocol network` muchas veces no encuentra el carro (`port not found`) porque depende de mDNS.
3. Si el sistema pregunta si deja entrar conexiones, hay que permitirlo: el ESP32 se conecta de vuelta a la máquina.
4. Al reiniciar, la red del carro desaparece unos segundos y la máquina puede pasarse sola a otra red. Volver a conectarse a `agente-laberinto`.

**Por cable:** el chip USB es un CH343, por el USB-C marcado "COM".
- En Windows salía como COM6. En Mac hace falta el driver de WCH (`brew install --cask wch-ch34x-usb-serial-driver` y aprobar la extensión en Ajustes) y sale como `/dev/cu.wchusbserial...`.
- Mientras esté el cable, la 9 V va desconectada. Las 4xAA pueden quedar puestas (solo alimentan los motores).
- `arduino-cli upload -p /dev/cu.wchusbserial... --fqbn <FQBN> --input-dir build/agente arduino/agente`

## Correr la ruta

1. Mandar el mapa (queda guardado y sobrevive a un reinicio):
   ```
   python -m pc.enviar maps/simulados/prueba_4x4.txt
   ```
   Tiene que decir `OK: el carro calculó la misma ruta que Python`: `F F R F F R F R F L F L F F L F F F`.
2. Abrir la consola: `python -m pc.consola` (por Wi-Fi) o `python -m pc.consola --puerto <puerto>` (por cable). Enter con la línea vacía manda `STOP`; Ctrl+C también.
3. Poner el carro en la celda de **arriba a la izquierda**, mirando a la **derecha** (este):
   - el eje de las ruedas (centro de la rueda) a 10 cm del borde de la cinta de la izquierda;
   - el centro del carro a 10 cm del borde de la cinta de arriba.
4. `GO`. **No tocar el carro el primer medio segundo:** calibra el giroscopio.
5. El log muestra cada tramo (`# F x2 -> (0,2,E) alinear con la pared`), el rumbo al terminar cada movimiento (`# rumbo=90.2 objetivo=90 pulsos=1`) y al final `OK META en N s`.

## Calibrar en otro piso o con otro laberinto

Lo que depende del sitio y lo que no:

| Qué | ¿Cambia con el sitio? | Por qué |
|---|---|---|
| Giros | Casi no | El giroscopio mide el ángulo real. Solo revisar con `PASO R`/`PASO L` |
| Rumbo al avanzar | No | El giroscopio lo corrige (`kr`) |
| `umbral` | Sí | Cambia con la cinta, la luz y el papel |
| `t_celda` | Sí | Cambia con el tamaño de la celda, las pilas y la pendiente |
| `t_alinear` / `pwm_aprox` | Sí | Dependen de cuánto impulso trae el carro al llegar a la pared (pendiente, piso) |

**Orden (unos 10 minutos)**, con `GET` al principio y `SAVE` cada vez que un valor quede bien:

1. **Umbral:** `SENS` con los 3 sensores sobre blanco y con cada uno sobre la cinta. `SET umbral` a mitad de camino entre el blanco más alto y la cinta más baja.
2. **Giros:** carro en el centro de una celda de la ruta (sobre el papel, con las ruedas lejos de la cinta). `IMU CAL`, después `PASO R` ×4 y `PASO L` ×4. El log de cada uno dice `# rumbo=… objetivo=…`. Con los pulsos debería quedar a ±4°. Si muchos giros necesitan 5-8 pulsos, ajustar la anticipación: con `SET giro_tol 180` (sin pulsos) se ve el error de cada lado; `giro_antic_r`/`giro_antic_l` nuevo = actual + (lo que se pasa en promedio) o − (lo que le falta). Volver a `SET giro_tol 4`.
3. **Avance:** carro centrado en una celda con 2 celdas libres adelante. `IMU CAL` y `PASO F 2`. Medir cuánto avanzó el centro de la rueda: `t_celda` nuevo = `t_celda` × (2 × tamaño de la celda) / cm. Confirmar con otro `PASO F 2`. El avance resultó proporcional al tiempo con el freno de contramarcha.
4. **Pared:** carro centrado, mirando hacia una celda libre que tiene pared al fondo. `PASO A`. El centro de la rueda debe quedar a media celda menos 1 cm del borde de la cinta (10 cm con celda de 22, 11 cm con celda de 24).
   - Si le falta: subir `t_alinear` (cada 10 ms ≈ 0.4 cm).
   - Si se pasa con `t_alinear` en 0: bajar `pwm_aprox` de a 10. No bajar de ~130: con 100 los motores no arrancan.
   - **Si el piso tiene pendiente,** hacerlo en la dirección de bajada: es donde más se pasa. En las otras direcciones quedará un poco corto, y eso no afecta.
5. **`GO`** varias veces. Si en algún tramo se tuerce hacia la cinta del lado, subir `kr` (de a 2).

Con la celda de 24 cm de la demo, como punto de partida: `t_celda` ≈ 536 × 24 / 22 ≈ 585.

## Qué falta

- Mezclar el PR de `firmware/ejecutor`. Necesita la aprobación de otro integrante.
- Fijar `COSTO_GIRO` con los tiempos reales: un giro tarda ~1-1.5 s y una celda ~0.6-0.8 s, así que el costo de un giro es ~2, no 0.5 (en `pc/modelo.py` y en el carro).
- Que la rueda no roce la cinta en los giros (arreglo mecánico): es la mayor fuente de pulsos.
- Calibrar en el sitio de la demo, con pilas nuevas y la 9 V de repuesto.
- Los pendientes del grupo están en el Excel (`Organizacion_Agente_Laberinto.xlsx`, en el Drive).

## Reglas del grupo (para quien siga con Claude Code)

- No asignar responsables ni roles a los integrantes: eso lo decide el grupo.
- No hacer push, PR ni merge sin que Daniel lo pida.
- Responder en español.
