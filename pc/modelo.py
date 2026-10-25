"""Estado del agente y acciones F/L/R con sus costos (ver docs/ARQUITECTURA.md, 2.2 y 2.3)."""

from __future__ import annotations

from typing import TYPE_CHECKING, NamedTuple

if TYPE_CHECKING:
    from pc.mapa import Mapa

# Orientaciones en sentido horario: girar a la derecha suma 1, a la izquierda resta 1.
ORIENTACIONES = "NESO"
# Bit de la pared en cada dirección (máscara de bits del mapa).
BIT = {"N": 1, "E": 2, "S": 4, "O": 8}
# Desplazamiento (fila, col) al avanzar. La fila 0 es la de arriba.
DELTA = {"N": (-1, 0), "E": (0, 1), "S": (1, 0), "O": (0, -1)}

ACCIONES = ("F", "L", "R")
COSTO_AVANCE = 1.0
# T_GIRO90_MS / T_CELDA_MS con los valores por defecto del firmware (400 / 800). Recalibrar con el carro.
COSTO_GIRO = 0.5

Celda = tuple[int, int]


class Estado(NamedTuple):
    fila: int
    col: int
    ori: str  # "N", "E", "S" u "O"

    @property
    def celda(self) -> Celda:
        return (self.fila, self.col)


def girar(ori: str, accion: str) -> str:
    paso = 1 if accion == "R" else -1
    return ORIENTACIONES[(ORIENTACIONES.index(ori) + paso) % 4]


def aplicar(mapa: Mapa, estado: Estado, accion: str) -> Estado | None:
    """Estado que resulta de la acción, o None si se choca con una pared."""
    if accion in ("L", "R"):
        return estado._replace(ori=girar(estado.ori, accion))
    if accion != "F":
        raise ValueError(f"acción desconocida: {accion!r}")
    if mapa.hay_pared(estado.fila, estado.col, estado.ori):
        return None
    df, dc = DELTA[estado.ori]
    return estado._replace(fila=estado.fila + df, col=estado.col + dc)


def costo(accion: str, costo_giro: float = COSTO_GIRO) -> float:
    return COSTO_AVANCE if accion == "F" else costo_giro


def sucesores(mapa: Mapa, estado: Estado, costo_giro: float = COSTO_GIRO) -> list[tuple[str, Estado, float]]:
    """(acción, estado siguiente, costo) de cada acción válida, en orden F, L, R."""
    res = []
    for accion in ACCIONES:
        sig = aplicar(mapa, estado, accion)
        if sig is not None:
            res.append((accion, sig, costo(accion, costo_giro)))
    return res
