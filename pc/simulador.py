"""Simulador: dibuja el mapa y anima la ruta del agente, paso a paso.

    python -m pc.simulador maps/simulados/ejemplo_5x5.txt                 # ruta de BFS, en una ventana
    python -m pc.simulador maps/simulados/ejemplo_5x5.txt --algo dfs
    python -m pc.simulador maps/simulados/ejemplo_5x5.txt --ruta "F F R F"  # una ruta escrita a mano
    python -m pc.simulador maps/simulados/ejemplo_5x5.txt --texto         # en la terminal, sin matplotlib

La ventana necesita matplotlib (pip install matplotlib); sin él, anima en la terminal.
Si la ruta atraviesa una pared, la animación para ahí y el programa termina con error.
"""

from __future__ import annotations

import argparse
import sys
import time

from pc import mapa as mapas
from pc.mapa import Mapa
from pc.modelo import ACCIONES, COSTO_GIRO, DELTA, Estado, aplicar
from pc.planners import REGISTRO, ejecutar

FLECHA = {"N": "^", "E": ">", "S": "v", "O": "<"}


class ErrorSimulacion(ValueError):
    def __init__(self, paso: int, accion: str, estado: Estado):
        super().__init__(f"el movimiento {paso} ({accion}) choca con una pared en {tuple(estado)}")
        self.paso, self.accion, self.estado = paso, accion, estado


def simular(m: Mapa, movimientos: list[str]) -> list[Estado]:
    """Estados desde el inicio, uno por movimiento. ErrorSimulacion si alguno atraviesa una pared."""
    estados = [m.inicio]
    for i, mov in enumerate(movimientos):
        if mov not in ACCIONES:
            raise ValueError(f"movimiento desconocido: {mov!r} (se esperaba F, L o R)")
        sig = aplicar(m, estados[-1], mov)
        if sig is None:
            raise ErrorSimulacion(i, mov, estados[-1])
        estados.append(sig)
    return estados


def cuadro(m: Mapa, estados: list[Estado]) -> str:
    """El mapa en texto con el recorrido hasta estados[-1]: S inicio, M meta, * celdas visitadas y el carro como flecha."""
    alto, ancho = 2 * m.filas + 1, 2 * m.cols + 1
    g = [[" "] * ancho for _ in range(alto)]
    for y in range(0, alto, 2):
        for x in range(0, ancho, 2):
            g[y][x] = "+"
    for f in range(m.filas):
        for c in range(m.cols):
            y, x = 2 * f + 1, 2 * c + 1
            if m.hay_pared(f, c, "N"):
                g[y - 1][x] = "-"
            if m.hay_pared(f, c, "S"):
                g[y + 1][x] = "-"
            if m.hay_pared(f, c, "O"):
                g[y][x - 1] = "|"
            if m.hay_pared(f, c, "E"):
                g[y][x + 1] = "|"
    for e in estados:
        g[2 * e.fila + 1][2 * e.col + 1] = "*"
    g[2 * m.inicio.fila + 1][2 * m.inicio.col + 1] = "S"
    g[2 * m.meta[0] + 1][2 * m.meta[1] + 1] = "M"
    actual = estados[-1]
    g[2 * actual.fila + 1][2 * actual.col + 1] = FLECHA[actual.ori]
    return "\n".join("".join(fila) for fila in g)


def animar_texto(m: Mapa, movimientos: list[str], estados: list[Estado], pausa: float, salida=None):
    salida = salida or sys.stdout
    for i in range(len(estados)):
        if salida.isatty():
            salida.write("\x1b[H\x1b[2J")  # limpiar la terminal
        paso = f"paso {i}/{len(movimientos)}" + (f": {movimientos[i - 1]}" if i else "")
        salida.write(f"{m.nombre}  {paso}\n{cuadro(m, estados[: i + 1])}\n\n")
        salida.flush()
        if i < len(estados) - 1:
            time.sleep(pausa)


def animar_ventana(m: Mapa, movimientos: list[str], estados: list[Estado], pausa: float, titulo: str):
    import matplotlib.pyplot as plt
    from matplotlib.animation import FuncAnimation

    fig, ax = plt.subplots(figsize=(max(4, m.cols * 0.8), max(4, m.filas * 0.8)))
    ax.set_xlim(-0.1, m.cols + 0.1)
    ax.set_ylim(m.filas + 0.1, -0.1)  # la fila 0 arriba, como en el archivo
    ax.set_aspect("equal")
    ax.axis("off")
    ax.set_title(titulo)
    # Paredes: en coordenadas de celda, la celda (f, c) ocupa [c, c+1] x [f, f+1].
    for f in range(m.filas):
        for c in range(m.cols):
            for ori, (x0, y0, x1, y1) in {"N": (c, f, c + 1, f), "S": (c, f + 1, c + 1, f + 1),
                                          "O": (c, f, c, f + 1), "E": (c + 1, f, c + 1, f + 1)}.items():
                if m.hay_pared(f, c, ori):
                    ax.plot([x0, x1], [y0, y1], color="black", lw=3, solid_capstyle="round")
    ax.add_patch(plt.Rectangle((m.inicio.col + 0.1, m.inicio.fila + 0.1), 0.8, 0.8, color="#cde8cd"))
    ax.add_patch(plt.Rectangle((m.meta[1] + 0.1, m.meta[0] + 0.1), 0.8, 0.8, color="#f5d0c5"))
    ax.text(m.inicio.col + 0.5, m.inicio.fila + 0.85, "S", ha="center", va="center", fontsize=8)
    ax.text(m.meta[1] + 0.5, m.meta[0] + 0.85, "M", ha="center", va="center", fontsize=8)

    (rastro,) = ax.plot([], [], color="#3b7dd8", lw=2, alpha=0.6)
    carro = ax.annotate("", xy=(0, 0), xytext=(0, 0), arrowprops=dict(arrowstyle="-|>", color="#c0392b", lw=3))
    texto = ax.text(0, m.filas + 0.05, "", va="top", fontsize=9)

    def dibujar(i):
        e = estados[i]
        rastro.set_data([s.col + 0.5 for s in estados[: i + 1]], [s.fila + 0.5 for s in estados[: i + 1]])
        df, dc = DELTA[e.ori]
        carro.xy = (e.col + 0.5 + 0.3 * dc, e.fila + 0.5 + 0.3 * df)
        carro.set_position((e.col + 0.5 - 0.3 * dc, e.fila + 0.5 - 0.3 * df))
        texto.set_text(f"paso {i}/{len(movimientos)}" + (f": {movimientos[i - 1]}" if i else ""))
        return rastro, carro, texto

    anim = FuncAnimation(fig, dibujar, frames=len(estados), interval=pausa * 1000, repeat=False)
    plt.show()
    return anim


def main(argv=None):
    p = argparse.ArgumentParser(description="Dibuja el mapa y anima la ruta del agente.")
    p.add_argument("mapa")
    p.add_argument("--algo", default="bfs", help=f"planificador que da la ruta: {', '.join(sorted(REGISTRO))}")
    p.add_argument("--ruta", help='ruta escrita a mano, por ejemplo "F F R F" (en vez de --algo)')
    p.add_argument("--costo-giro", type=float, default=COSTO_GIRO, help="costo de L/R relativo a F")
    p.add_argument("--pausa", type=float, default=0.4, help="segundos entre pasos")
    p.add_argument("--texto", action="store_true", help="animar en la terminal en vez de una ventana")
    args = p.parse_args(argv)

    m = mapas.leer(args.mapa)
    if args.ruta is not None:
        movimientos, titulo = args.ruta.split(), f"{m.nombre}: ruta a mano"
    elif args.algo in REGISTRO:
        res = ejecutar(REGISTRO[args.algo](args.costo_giro), m)
        if res is None:
            print("sin ruta", file=sys.stderr)
            return 1
        movimientos = res.movimientos
        titulo = (f"{m.nombre}: {args.algo}, {len(movimientos)} acciones, costo {res.costo:g}, "
                  f"{res.nodos_expandidos} expandidos")
    else:
        p.error(f"algoritmo desconocido {args.algo!r}; hay: {', '.join(sorted(REGISTRO))}")

    error = None
    try:
        estados = simular(m, movimientos)
    except ErrorSimulacion as e:
        error = e
        estados = simular(m, movimientos[: e.paso])  # animar hasta el choque
        movimientos = movimientos[: e.paso]
    except ValueError as e:
        p.error(str(e))

    usar_texto = args.texto
    if not usar_texto:
        try:
            import matplotlib  # noqa: F401
        except ImportError:
            print("# sin matplotlib (pip install matplotlib): animando en la terminal", file=sys.stderr)
            usar_texto = True
    if usar_texto:
        animar_texto(m, movimientos, estados, args.pausa)
    else:
        animar_ventana(m, movimientos, estados, args.pausa, titulo)

    if error is not None:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    if estados[-1].celda != m.meta:
        print(f"ERROR: la ruta termina en {estados[-1].celda}, no en la meta {m.meta}", file=sys.stderr)
        return 1
    print(f"# llegó a la meta en {len(movimientos)} movimientos", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
