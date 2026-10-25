"""BFS sobre estados (fila, col, orientación): minimiza el número de acciones F/L/R."""

from collections import deque

from pc.modelo import costo, sucesores
from pc.planners.base import Planner, Resultado, reconstruir, registrar


@registrar
class BFS(Planner):
    nombre = "bfs"
    optimiza = "acciones"

    def planear(self, mapa, inicio, meta):
        padres = {inicio: None}
        frontera = deque([inicio])
        expandidos = 0
        while frontera:
            estado = frontera.popleft()
            if estado.celda == meta:
                movimientos, estados = reconstruir(padres, estado)
                total = sum(costo(m, self.costo_giro) for m in movimientos)
                return Resultado(movimientos, estados, total, expandidos)
            expandidos += 1
            for accion, sig, _ in sucesores(mapa, estado, self.costo_giro):
                if sig not in padres:
                    padres[sig] = (estado, accion)
                    frontera.append(sig)
        return None
