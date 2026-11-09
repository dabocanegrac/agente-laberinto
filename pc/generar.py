"""Generador de mapas aleatorios válidos para el laberinto.

Garantiza siempre camino entre inicio y meta mediante un árbol generador (DFS)
y permite abrir ciclos adicionales para crear múltiples caminos y bifurcaciones.
"""

from __future__ import annotations

import argparse
import random
import sys
from pathlib import Path

from pc import mapa as mapas
from pc.mapa import Mapa
from pc.modelo import DELTA, ORIENTACIONES, Celda, Estado

MAX_FILAS = 16
MAX_COLS = 16
CARPETA_SIMULADOS = Path(__file__).resolve().parents[1] / "maps" / "simulados"


def generar_mapa(
    filas: int = 8,
    cols: int = 8,
    *,
    ciclos: float = 0.15,
    semilla: int | None = None,
    inicio: Estado | None = None,
    meta: Celda | None = None,
    nombre: str = "",
) -> Mapa:
    """Genera un mapa aleatorio válido de filas x cols celdas.

    Usa DFS iterativo para construir un árbol generador (sin ciclos) y luego
    abre paredes internas adicionales con probabilidad `ciclos` (0.0..1.0).
    """
    if not (1 <= filas <= MAX_FILAS and 1 <= cols <= MAX_COLS):
        raise ValueError(f"el tamaño debe estar entre 1x1 y {MAX_FILAS}x{MAX_COLS} celdas")
    if filas * cols < 2:
        raise ValueError("el mapa debe tener al menos 2 celdas")
    if not (0.0 <= ciclos <= 1.0):
        raise ValueError("ciclos debe estar entre 0 y 1")

    if inicio is None:
        inicio = Estado(0, 0, "E")
    elif inicio.ori not in ORIENTACIONES:
        raise ValueError(f"orientación inválida: {inicio.ori!r}")

    if meta is None:
        meta = (filas - 1, cols - 1)
        if inicio.celda == meta:
            meta = (0, 0)

    if not (0 <= inicio.fila < filas and 0 <= inicio.col < cols):
        raise ValueError(f"inicio fuera del mapa: {inicio}")
    if not (0 <= meta[0] < filas and 0 <= meta[1] < cols):
        raise ValueError(f"meta fuera del mapa: {meta}")
    if inicio.celda == meta:
        raise ValueError("inicio y meta no pueden ser la misma celda")

    rng = random.Random(semilla)
    alto, ancho = 2 * filas + 1, 2 * cols + 1
    g = [["1"] * ancho for _ in range(alto)]

    for f in range(filas):
        for c in range(cols):
            g[2 * f + 1][2 * c + 1] = "0"

    # Árbol generador aleatorio mediante DFS iterativo
    visitadas = {(0, 0)}
    pila = [(0, 0)]
    while pila:
        f, c = pila[-1]
        vecinos = []
        for ori in ORIENTACIONES:
            df, dc = DELTA[ori]
            nf, nc = f + df, c + dc
            if 0 <= nf < filas and 0 <= nc < cols and (nf, nc) not in visitadas:
                vecinos.append((df, dc, nf, nc))
        if vecinos:
            df, dc, nf, nc = rng.choice(vecinos)
            g[2 * f + 1 + df][2 * c + 1 + dc] = "0"
            visitadas.add((nf, nc))
            pila.append((nf, nc))
        else:
            pila.pop()

    # Apertura de ciclos sobre paredes internas restantes
    if ciclos > 0.0:
        for y in range(1, alto - 1):
            for x in range(1, ancho - 1):
                if (y + x) % 2 == 1 and g[y][x] == "1" and rng.random() < ciclos:
                    g[y][x] = "0"

    return mapas.desde_grilla(["".join(fila) for fila in g], inicio, meta, nombre=nombre)


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="Genera mapas aleatorios válidos del laberinto.")
    p.add_argument("--filas", type=int, default=8, help=f"filas en celdas (1..{MAX_FILAS})")
    p.add_argument("--cols", type=int, default=8, help=f"columnas en celdas (1..{MAX_COLS})")
    p.add_argument("--ciclos", type=float, default=0.15, help="probabilidad de abrir cada pared interna (0..1)")
    p.add_argument("--semilla", type=int, default=None, help="semilla entera para reproducibilidad")
    p.add_argument("-n", "--cantidad", type=int, default=1, help="cantidad de mapas a generar (>= 1)")
    p.add_argument("-o", "--salida", type=Path, default=CARPETA_SIMULADOS, help="archivo .txt de destino o carpeta")
    p.add_argument("--prefijo", default="simulado", help="prefijo cuando la salida es una carpeta")
    p.add_argument("--esperado", action="store_true", help="guarda la ruta óptima BFS en .esperado")
    args = p.parse_args(argv)

    if args.cantidad < 1:
        p.error("la cantidad debe ser al menos 1")
    if args.cantidad > 1 and args.salida.suffix == ".txt":
        p.error("si --cantidad > 1, --salida debe ser un directorio")

    from pc.planners.bfs import BFS

    for i in range(1, args.cantidad + 1):
        semilla_i = (args.semilla + (i - 1)) if args.semilla is not None else None

        if args.cantidad == 1 and args.salida.suffix == ".txt":
            ruta_txt = args.salida
        elif args.cantidad == 1:
            ruta_txt = args.salida / f"{args.prefijo}_{args.filas}x{args.cols}.txt"
        else:
            ruta_txt = args.salida / f"{args.prefijo}_{args.filas}x{args.cols}_{i:02d}.txt"

        try:
            m = generar_mapa(
                args.filas,
                args.cols,
                ciclos=args.ciclos,
                semilla=semilla_i,
                nombre=ruta_txt.stem,
            )
        except ValueError as e:
            p.error(str(e))

        ruta_txt.parent.mkdir(parents=True, exist_ok=True)
        header = (
            f"# Mapa simulado {m.filas}x{m.cols} "
            f"(ciclos={args.ciclos:g}{f', semilla={semilla_i}' if semilla_i is not None else ''})\n"
        )
        ruta_txt.write_text(header + mapas.a_texto(m), encoding="utf-8")
        print(f"escrito {ruta_txt}")

        if args.esperado:
            res = BFS().planear(m, m.inicio, m.meta)
            if res is not None:
                ruta_esp = ruta_txt.with_suffix(".esperado")
                ruta_esp.write_text(res.ruta + "\n", encoding="utf-8")
                print(f"escrito {ruta_esp}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
