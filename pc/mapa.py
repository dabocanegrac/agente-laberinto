"""Lectura y validación del mapa en formato de grilla (2n+1)x(2n+1) de 0/1 (DEC-01).

    # comentario
    inicio 0 0 E        fila col orientación (N/E/S/O), en celdas
    meta 2 2            fila col, en celdas
    1111111             una línea por fila de la grilla: 1 = pared, 0 = libre
    1000101             la celda (f, c) está en la posición (2f+1, 2c+1)
    ...                 y las paredes quedan entre celdas

Los espacios dentro de una fila se ignoran. El carro lee el mismo archivo
(arduino/agente/src/planner/mapa.cpp), así que los dos lectores tienen que aceptar lo mismo.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from pc.modelo import BIT, ORIENTACIONES, Celda, Estado


class ErrorMapa(ValueError):
    pass


@dataclass
class Mapa:
    filas: int
    cols: int
    paredes: list[list[int]]  # máscara por celda: N=1 E=2 S=4 O=8
    inicio: Estado
    meta: Celda
    nombre: str = ""

    def dentro(self, fila: int, col: int) -> bool:
        return 0 <= fila < self.filas and 0 <= col < self.cols

    def hay_pared(self, fila: int, col: int, ori: str) -> bool:
        return bool(self.paredes[fila][col] & BIT[ori])


def desde_grilla(grilla: list[str], inicio: Estado, meta: Celda, nombre: str = "") -> Mapa:
    alto, ancho = len(grilla), len(grilla[0]) if grilla else 0
    if any(len(g) != ancho for g in grilla):
        raise ErrorMapa("todas las filas de la grilla deben tener el mismo ancho")
    if alto < 3 or ancho < 3 or alto % 2 == 0 or ancho % 2 == 0:
        raise ErrorMapa(f"la grilla es de {alto}x{ancho}: debe ser de (2n+1)x(2m+1), mínimo 3x3")
    for y, g in enumerate(grilla):
        for x, v in enumerate(g):
            if (y in (0, alto - 1) or x in (0, ancho - 1)) and v != "1":
                raise ErrorMapa(f"falta pared en el borde, posición ({y}, {x}) de la grilla")
            if y % 2 == 1 and x % 2 == 1 and v != "0":
                raise ErrorMapa(f"la celda ({y // 2}, {x // 2}) está marcada como pared")

    filas, cols = alto // 2, ancho // 2
    paredes = []
    for f in range(filas):
        fila = []
        for c in range(cols):
            y, x = 2 * f + 1, 2 * c + 1
            fila.append(
                (grilla[y - 1][x] == "1") * BIT["N"] + (grilla[y][x + 1] == "1") * BIT["E"]
                + (grilla[y + 1][x] == "1") * BIT["S"] + (grilla[y][x - 1] == "1") * BIT["O"]
            )
        paredes.append(fila)

    m = Mapa(filas, cols, paredes, inicio, meta, nombre)
    if not m.dentro(*inicio.celda):
        raise ErrorMapa(f"inicio fuera del mapa: {inicio}")
    if not m.dentro(*meta):
        raise ErrorMapa(f"meta fuera del mapa: {meta}")
    return m


def leer(ruta: str | Path) -> Mapa:
    ruta = Path(ruta)
    inicio = meta = None
    grilla = []
    for n, linea in enumerate(ruta.read_text(encoding="utf-8").splitlines(), start=1):
        linea = linea.split("#", 1)[0].strip()
        if not linea:
            continue
        partes = linea.split()
        if partes[0] == "inicio":
            if len(partes) != 4 or partes[3] not in ORIENTACIONES:
                raise ErrorMapa(f"{ruta.name}:{n}: se esperaba 'inicio fila col N|E|S|O'")
            inicio = Estado(int(partes[1]), int(partes[2]), partes[3])
        elif partes[0] == "meta":
            if len(partes) != 3:
                raise ErrorMapa(f"{ruta.name}:{n}: se esperaba 'meta fila col'")
            meta = (int(partes[1]), int(partes[2]))
        else:
            fila = "".join(partes)
            if set(fila) - {"0", "1"}:
                raise ErrorMapa(f"{ruta.name}:{n}: la grilla solo puede tener 0 y 1")
            grilla.append(fila)

    if inicio is None or meta is None:
        raise ErrorMapa(f"{ruta.name}: faltan las líneas 'inicio' y/o 'meta'")
    try:
        return desde_grilla(grilla, inicio, meta, nombre=ruta.stem)
    except ErrorMapa as e:
        raise ErrorMapa(f"{ruta.name}: {e}") from None


def a_texto(m: Mapa) -> str:
    """El mapa en el formato del archivo, sin comentarios (lo que se le manda al carro)."""
    alto, ancho = 2 * m.filas + 1, 2 * m.cols + 1
    g = [["0"] * ancho for _ in range(alto)]
    for y in range(0, alto, 2):
        for x in range(0, ancho, 2):
            g[y][x] = "1"  # postes
    for f in range(m.filas):
        for c in range(m.cols):
            y, x = 2 * f + 1, 2 * c + 1
            if m.hay_pared(f, c, "N"):
                g[y - 1][x] = "1"
            if m.hay_pared(f, c, "E"):
                g[y][x + 1] = "1"
            if m.hay_pared(f, c, "S"):
                g[y + 1][x] = "1"
            if m.hay_pared(f, c, "O"):
                g[y][x - 1] = "1"
    lineas = [f"inicio {m.inicio.fila} {m.inicio.col} {m.inicio.ori}", f"meta {m.meta[0]} {m.meta[1]}"]
    return "\n".join(lineas + ["".join(fila) for fila in g]) + "\n"
