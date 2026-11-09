"""Búsqueda de Costo Uniforme (UCS / Dijkstra): minimiza el costo acumulado."""

from __future__ import annotations

import heapq
from itertools import count

from pc.mapa import Mapa
from pc.modelo import Celda, Estado, sucesores
from pc.planners.base import Planner, Resultado, reconstruir, registrar


@registrar
class UCS(Planner):
    nombre = "ucs"
    optimiza = "costo"

    def planear(self, mapa: Mapa, inicio: Estado, meta: Celda) -> Resultado | None:
        padres: dict[Estado, tuple[Estado, str] | None] = {inicio: None}
        g = {inicio: 0.0}
        cerrados = set()
        expandidos = 0
        n = count()
        frontera = [(0.0, next(n), inicio)]

        while frontera:
            costo_actual, _, estado = heapq.heappop(frontera)
            if estado in cerrados:
                continue
            if estado.celda == meta:
                movimientos, estados = reconstruir(padres, estado)
                return Resultado(movimientos, estados, costo_actual, expandidos)
            cerrados.add(estado)
            expandidos += 1
            for accion, sig, c in sucesores(mapa, estado, self.costo_giro):
                if sig not in cerrados:
                    nuevo = costo_actual + c
                    if nuevo < g.get(sig, float("inf")):
                        g[sig] = nuevo
                        padres[sig] = (estado, accion)
                        heapq.heappush(frontera, (nuevo, next(n), sig))
        return None
