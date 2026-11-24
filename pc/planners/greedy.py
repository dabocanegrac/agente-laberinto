"""Greedy: expande siempre el estado que la heurística ve más cerca de la meta.

Solo mira la heurística h(n), sin sumar el costo acumulado g(n). Es rápido pero no garantiza
la ruta óptima, por eso optimiza = None.
"""

import heapq
from itertools import count

from pc.modelo import DELTA, Estado, costo, sucesores
from pc.planners.base import Planner, Resultado, reconstruir, registrar


def heuristica(estado: Estado, meta: tuple[int, int], costo_giro: float) -> float:
    """Manhattan + 1 giro si la meta no queda en línea recta al frente (docs/ARQUITECTURA.md, 2.3)."""
    df, dc = meta[0] - estado.fila, meta[1] - estado.col
    h = abs(df) + abs(dc)
    if h == 0:
        return 0.0
    paso_f, paso_c = DELTA[estado.ori]
    al_frente = (df == 0 and dc * paso_c > 0) or (dc == 0 and df * paso_f > 0)
    return h if al_frente else h + costo_giro


@registrar
class Greedy(Planner):
    nombre = "greedy"
    optimiza = None

    def planear(self, mapa, inicio, meta):
        padres = {inicio: None}
        desempate = count()  # si dos estados tienen la misma h, sale primero el que entró antes
        frontera = [(heuristica(inicio, meta, self.costo_giro), next(desempate), inicio)]
        expandidos = 0
        while frontera:
            _, _, estado = heapq.heappop(frontera)
            if estado.celda == meta:
                movimientos, estados = reconstruir(padres, estado)
                total = sum(costo(m, self.costo_giro) for m in movimientos)
                return Resultado(movimientos, estados, total, expandidos)
            expandidos += 1
            for accion, sig, _ in sucesores(mapa, estado, self.costo_giro):
                if sig not in padres:
                    padres[sig] = (estado, accion)
                    heapq.heappush(frontera, (heuristica(sig, meta, self.costo_giro), next(desempate), sig))
        return None