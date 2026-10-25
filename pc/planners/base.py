"""Contrato de los planificadores. Agregar un algoritmo = un archivo en pc/planners/ con:

    @registrar
    class MiAlgo(Planner):
        nombre = "mialgo"
        optimiza = None      # "acciones", "costo" o None si no garantiza la ruta óptima

        def planear(self, mapa, inicio, meta):
            ...
            return Resultado(movimientos, estados, costo, nodos_expandidos)

No hay que tocar nada más: planear.py y los tests lo encuentran solos.
"""

from __future__ import annotations

import time
from abc import ABC, abstractmethod
from dataclasses import dataclass

from pc.mapa import Mapa
from pc.modelo import COSTO_GIRO, Celda, Estado


@dataclass
class Resultado:
    movimientos: list[str]  # ["F", "F", "L", ...]
    estados: list[Estado]   # desde el inicio hasta la meta, uno más que movimientos
    costo: float
    nodos_expandidos: int
    tiempo_ms: float = 0.0  # lo llena ejecutar()

    @property
    def ruta(self) -> str:
        return " ".join(self.movimientos)


class Planner(ABC):
    nombre: str
    optimiza: str | None = None

    def __init__(self, costo_giro: float = COSTO_GIRO):
        self.costo_giro = costo_giro

    @abstractmethod
    def planear(self, mapa: Mapa, inicio: Estado, meta: Celda) -> Resultado | None:
        """Ruta desde inicio hasta cualquier orientación en la celda meta, o None si no hay."""


REGISTRO: dict[str, type[Planner]] = {}


def registrar(cls: type[Planner]) -> type[Planner]:
    if cls.nombre in REGISTRO:
        raise ValueError(f"ya hay un planificador llamado {cls.nombre!r}")
    REGISTRO[cls.nombre] = cls
    return cls


def ejecutar(planner: Planner, mapa: Mapa) -> Resultado | None:
    t0 = time.perf_counter()
    res = planner.planear(mapa, mapa.inicio, mapa.meta)
    if res is not None:
        res.tiempo_ms = (time.perf_counter() - t0) * 1000
    return res


def reconstruir(padres: dict[Estado, tuple[Estado, str] | None], final: Estado) -> tuple[list[str], list[Estado]]:
    """Recorre los padres desde el estado final hasta el inicio (el que tiene padre None)."""
    movimientos, estados = [], [final]
    while padres[estados[-1]] is not None:
        anterior, accion = padres[estados[-1]]
        movimientos.append(accion)
        estados.append(anterior)
    return movimientos[::-1], estados[::-1]
