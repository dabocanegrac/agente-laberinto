# Handoff: primera prueba en el laberinto impreso (26/10/2026)

Esto es para seguir en otra máquina (con o sin Claude Code) sin perder el contexto.
Si usas Claude Code allá, pídele que lea este archivo, el `README.md` y `docs/ARQUITECTURA.md`.

## Dónde estamos

- **El laberinto impreso** es `maps/simulados/prueba_4x4.txt` en la versión para rollo de 91 cm:
  **celda de 22 cm**, línea de 2 cm y margen de 0.5 cm (91 x 91 cm).
  - No es el mapa 1 de la demo, que se arma con celda de 24 cm (DEC-02).
  - Arriba del papel es el norte, porque la fila 0 está arriba. El texto gris del borde queda abajo a la izquierda.
- **Firmware `arduino/agente` (rama `firmware/ejecutor`):**
  - Recibe el mapa, planea con BFS a bordo y **recorre la ruta** con `GO`.
  - Ya compila, pero **nunca se ha corrido en el carro**: hoy es la primera vez.
  - Todos los parámetros vienen sin calibrar.
- **Lo que tiene el carro ahora:** `arduino/control_remoto`, el del joystick web. Ese sketch también tiene OTA, así que el firmware nuevo se puede subir por Wi-Fi.
- **El hardware:** pines, alimentación y sensores están en el README.
  - Batería de 9 V al jack y 4xAA a los motores.
  - **Nunca conectar USB y batería al tiempo.**
  - Si la batería está por debajo de ~8.5 V, el ESP32 se reinicia por brownout y el LED queda rojo.

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
3. Instalar el Arduino IDE 2 (o `arduino-cli`) y agregarle el core **esp32 de Espressif**. Aquí se usó la versión 3.3.12.
   - Si es con el IDE: en Preferencias, agregar la URL `https://espressif.github.io/arduino-esp32/package_esp32_index.json` y luego instalar "esp32" desde el gestor de placas.
4. Configurar la placa como **ESP32S3 Dev Module**:
   - Flash Size: 16MB
   - PSRAM: OPI PSRAM
   - Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
   - USB CDC On Boot: Disabled (el default)
   - Con `arduino-cli`, el FQBN es:
     ```
     esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,CDCOnBoot=default,PartitionScheme=app3M_fat9M_16MB
     ```

## Subir el firmware

**Por Wi-Fi, que es lo recomendado:**
1. Prender el carro con la batería y conectar la máquina a la red `agente-laberinto` (clave `laberinto2026`).
2. Abrir `arduino/agente/agente.ino` y escoger el puerto de red `agente-laberinto`. La clave de OTA también es `laberinto2026`.
3. Subir. Si el sistema pregunta si deja entrar conexiones, hay que permitirlo: el ESP32 se conecta de vuelta a la máquina.
4. Con `arduino-cli`:
   ```
   arduino-cli compile --fqbn <FQBN> arduino/agente
   arduino-cli upload --fqbn <FQBN> -p 192.168.4.1 --protocol network --upload-field password=laberinto2026 arduino/agente
   ```
5. Si falla, la otra opción es `espota`. Está explicado en `docs/ARQUITECTURA.md`, sección 4.2.

**Por cable:** el chip USB es un CH343.
- En Windows salía como COM6.
- En Mac puede necesitar el driver de WCH y sale como `/dev/cu.wchusbserial...`.
- Mientras esté el cable, la batería va desconectada.

## La prueba, paso a paso

Todo se hace conectado a la red `agente-laberinto`, desde la raíz del repo.

1. **Mandar el mapa:**
   ```
   python -m pc.enviar maps/simulados/prueba_4x4.txt
   ```
   Tiene que decir `OK: el carro calculó la misma ruta que Python` y la ruta debe ser
   `F F R F F R F R F L F L F F L F F F`. El mapa queda guardado y sobrevive a un reinicio.
2. **Abrir la consola:**
   ```
   python -m pc.consola
   ```
   - Enter con la línea vacía manda `STOP`. **Tener el dedo listo.**
   - Ctrl+C también manda `STOP` y cierra.
3. **Calibrar en este orden.** Cada `SET` se aplica en el acto; al final hay que hacer `SAVE`.

| # | Qué | Cómo | Ajuste |
|---|---|---|---|
| 1 | Sensores | `SENS` con los 3 sensores en blanco y luego con cada uno encima de una línea. `SENS ON` muestra la lectura continua y `SENS OFF` la apaga | `SET umbral <v>` a mitad de camino entre el blanco y la línea. **La tinta del plotter no está probada:** si sobre la línea no sube de ~150, el problema es la tinta o la altura de los sensores, no el código |
| 2 | Giro de 90° | Hacer `PASO R` cuatro veces: el carro tiene que quedar mirando igual que al principio. Repetir con `PASO L` | Se pasa: bajar `t_giro`. No llega: subirlo. Si pita y no gira: subir `pwm_giro` (máximo 255) |
| 3 | Avance por celda | Con el eje de las ruedas en el centro de una celda, hacer `PASO F 3`: tiene que terminar en el centro de la tercera celda | `t_celda` nuevo = `t_celda` × 66 / (cm que avanzó realmente). Si se tuerce sin pisar líneas: `SET comp <v>` (positivo le da más a la rueda derecha) |
| 4 | Alinear con la pared | Con el carro centrado y una pared dos celdas adelante, hacer `PASO A`: avanza hasta que el frontal ve la línea y sigue `t_alinear` ms. Tiene que quedar con el eje en el centro de la celda | En teoría, el frontal (16 cm delante del eje) ve la línea cuando al eje le faltan ~6 cm para el centro, así que `t_alinear` ≈ 0.27 × `t_celda`. Se pasa: bajarlo |
| 5 | Corrección lateral | Ver en `PASO F 3` si se endereza cuando un lateral pisa una línea | Poca corrección: subir `kp`. Si zigzaguea: bajarlo |
| 6 | Guardar | `GET` para revisar y `SAVE` para guardar | |

4. **Recorrer:**
   - Poner el carro en la celda de **arriba a la izquierda**, mirando a la **derecha** (este), con el eje de las ruedas en el centro de la celda.
   - Escribir `GO`.
   - El log muestra cada grupo, por ejemplo `# F x2 -> (0,2,E) alinear con la pared`, y al final `OK META en N s`.
   - La meta es la celda de abajo a la derecha.
5. **Si algo sale mal, anotar** la pose que dijo el log antes de fallar y hacer `GET`. Con eso se corrige el parámetro que toque.

Los parámetros y sus valores por defecto están en `arduino/agente/src/config/parametros.h`.

## Qué falta después de hoy

- Mezclar el PR de `firmware/ejecutor` cuando el carro haga el recorrido. Necesita la aprobación de otro integrante.
- Pasar los valores calibrados a `parametros.h` como nuevos valores por defecto.
- Medir el giro y el avance reales para fijar `COSTO_GIRO` ≈ `t_giro / t_celda` (en `pc/modelo.py` y en el carro).
- El mapa 1 de la demo va con celda de 24 cm: los tiempos cambian, pero `t_giro` y el umbral no.
- Los pendientes del grupo están en el Excel (`Organizacion_Agente_Laberinto.xlsx`, en el Drive).

## Reglas del grupo (para quien siga con Claude Code)

- No asignar responsables ni roles a los integrantes: eso lo decide el grupo.
- No hacer push, PR ni merge sin que Daniel lo pida.
- Responder en español.
