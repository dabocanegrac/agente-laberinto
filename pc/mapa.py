"""Lectura y validación del mapa (formato de máscara de bits, DEC-01).

    # comentario
    5 5                 filas columnas
    inicio 0 0 E        fila col orientación (N/E/S/O)
    meta 4 4            fila col
    9 5 3 9 3           una fila por línea, un número por celda: N=1 E=2 S=4 O=8
    ...

Las paredes compartidas entre dos celdas se marcan en las dos.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from pc.modelo import BIT, DELTA, ORIENTACIONES, Celda, Estado

OPUESTA = {"N": "S", "E": "O", "S": "N", "O": "E"}


class ErrorMapa(ValueError):
    pass


@dataclass
class Mapa:
    filas: int
    cols: int
    paredes: list[list[int]]
    inicio: Estado
    meta: Celda
    nombre: str = ""

    def dentro(self, fila: int, col: int) -> bool:
        return 0 <= fila < self.filas and 0 <= col < self.cols

    def hay_pared(self, fila: int, col: int, ori: str) -> bool:
        return bool(self.paredes[fila][col] & BIT[ori])

    def validar(self) -> None:
        if not self.dentro(*self.inicio.celda):
            raise ErrorMapa(f"inicio fuera del mapa: {self.inicio}")
        if not self.dentro(*self.meta):
            raise ErrorMapa(f"meta fuera del mapa: {self.meta}")
        for f in range(self.filas):
            for c in range(self.cols):
                v = self.paredes[f][c]
                if not 0 <= v <= 15:
                    raise ErrorMapa(f"celda ({f}, {c}) = {v}: debe estar entre 0 y 15")
                for ori, (df, dc) in DELTA.items():
                    vf, vc = f + df, c + dc
                    if not self.dentro(vf, vc):
                        if not self.hay_pared(f, c, ori):
                            raise ErrorMapa(f"celda ({f}, {c}): falta la pared {ori} del borde")
                    elif self.hay_pared(f, c, ori) != self.hay_pared(vf, vc, OPUESTA[ori]):
                        raise ErrorMapa(
                            f"pared {ori} de ({f}, {c}) no coincide con la {OPUESTA[ori]} de ({vf}, {vc})"
                        )


def leer(ruta: str | Path) -> Mapa:
    ruta = Path(ruta)
    lineas = []
    for n, linea in enumerate(ruta.read_text(encoding="utf-8").splitlines(), start=1):
        linea = linea.split("#", 1)[0].strip()
        if linea:
            lineas.append((n, linea.split()))

    def error(n: int, msg: str) -> ErrorMapa:
        return ErrorMapa(f"{ruta.name}:{n}: {msg}")

    if not lineas:
        raise ErrorMapa(f"{ruta.name}: archivo vacío")
    n, tam = lineas[0]
    try:
        filas, cols = map(int, tam)
    except ValueError:
        raise error(n, "se esperaba 'filas columnas'") from None

    inicio = meta = None
    paredes = []
    for n, partes in lineas[1:]:
        if partes[0] == "inicio":
            if len(partes) != 4 or partes[3] not in ORIENTACIONES:
                raise error(n, "se esperaba 'inicio fila col N|E|S|O'")
            inicio = Estado(int(partes[1]), int(partes[2]), partes[3])
        elif partes[0] == "meta":
            if len(partes) != 3:
                raise error(n, "se esperaba 'meta fila col'")
            meta = (int(partes[1]), int(partes[2]))
        else:
            if len(partes) != cols:
                raise error(n, f"la fila tiene {len(partes)} celdas y deberían ser {cols}")
            paredes.append([int(x) for x in partes])

    if inicio is None or meta is None:
        raise ErrorMapa(f"{ruta.name}: faltan las líneas 'inicio' y/o 'meta'")
    if len(paredes) != filas:
        raise ErrorMapa(f"{ruta.name}: hay {len(paredes)} filas y deberían ser {filas}")

    mapa = Mapa(filas, cols, paredes, inicio, meta, nombre=ruta.stem)
    mapa.validar()
    return mapa
