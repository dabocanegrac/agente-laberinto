"""A* sobre estados (fila, col, orientación): minimiza el costo (el tiempo real), como UCS pero
expandiendo menos nodos gracias a la heurística (ver docs/ARQUITECTURA.md, 2.3)."""

import heapq
from itertools import count

from pc.modelo import COSTO_AVANCE, DELTA, sucesores
from pc.planners.base import Planner, Resultado, reconstruir, registrar


def heuristica(estado, meta, costo_giro):
    """Distancia Manhattan, más 1 giro si la meta no queda al frente (en línea recta adelante).

    Es admisible y consistente: si la meta no está al frente, está detrás, a un lado o en otra fila
    y otra columna, y en todos esos casos hace falta al menos un giro. Avanzar no puede dejar la
    meta al frente si antes no lo estaba, así que h nunca baja más que el costo de la acción.
    """
    df, dc = meta[0] - estado.fila, meta[1] - estado.col
    if df == 0 and dc == 0:
        return 0.0
    h = (abs(df) + abs(dc)) * COSTO_AVANCE
    uf, uc = DELTA[estado.ori]
    al_frente = (df == 0 and dc * uc > 0) if uf == 0 else (dc == 0 and df * uf > 0)
    return h if al_frente else h + costo_giro


@registrar
class AStar(Planner):
    nombre = "astar"
    optimiza = "costo"

    def planear(self, mapa, inicio, meta):
        padres = {inicio: None}
        g = {inicio: 0.0}
        cerrados = set()
        n = count()  # desempate: a igual f, el que entró primero (resultado determinista)
        frontera = [(heuristica(inicio, meta, self.costo_giro), next(n), inicio)]
        expandidos = 0
        while frontera:
            _, _, estado = heapq.heappop(frontera)
            if estado in cerrados:
                continue  # entrada vieja: ya salió con un g menor
            if estado.celda == meta:
                movimientos, estados = reconstruir(padres, estado)
                return Resultado(movimientos, estados, g[estado], expandidos)
            cerrados.add(estado)
            expandidos += 1
            for accion, sig, c in sucesores(mapa, estado, self.costo_giro):
                nuevo = g[estado] + c
                if sig not in cerrados and nuevo < g.get(sig, float("inf")):
                    g[sig] = nuevo
                    padres[sig] = (estado, accion)
                    heapq.heappush(frontera, (nuevo + heuristica(sig, meta, self.costo_giro), next(n), sig))
        return None
