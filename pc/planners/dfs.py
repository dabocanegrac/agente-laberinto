"""DFS sobre estados (fila, col, orientación): encuentra UNA ruta, no necesariamente la mejor.

Explora primero F, después L y R (mismo orden que modelo.sucesores), así que tiende a seguir derecho
hasta chocar. Iterativo, con pila, para no pasarse del límite de recursión en mapas grandes.
"""

from pc.modelo import costo, sucesores
from pc.planners.base import Planner, Resultado, reconstruir, registrar


@registrar
class DFS(Planner):
    nombre = "dfs"
    optimiza = None

    def planear(self, mapa, inicio, meta):
        padres = {inicio: None}
        pila = [inicio]
        cerrados = set()
        expandidos = 0
        while pila:
            estado = pila.pop()
            if estado in cerrados:
                continue
            if estado.celda == meta:
                movimientos, estados = reconstruir(padres, estado)
                total = sum(costo(m, self.costo_giro) for m in movimientos)
                return Resultado(movimientos, estados, total, expandidos)
            cerrados.add(estado)
            expandidos += 1
            # Al revés, para que F salga primero de la pila.
            for accion, sig, _ in reversed(sucesores(mapa, estado, self.costo_giro)):
                if sig not in cerrados:
                    padres[sig] = (estado, accion)
                    pila.append(sig)
        return None
