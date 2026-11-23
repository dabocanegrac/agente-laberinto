#pragma once

// A* sobre estados (fila, col, orientación), igual que pc/planners/astar.py: misma heurística (Manhattan
// + 1 giro si la meta no queda al frente), mismo orden de acciones y mismo desempate (a igual f, el que
// entró primero a la frontera), así que da exactamente la misma ruta y los mismos nodos expandidos.
// Minimiza el costo (F = 1, L/R = costo_giro).
//
// costo_giro es double, como en Python: en float, 0.8 queda 0.800000012 y eso basta para que dos rutas
// que en Python empatan dejen de empatar (otra ruta). Lo encontró test_misma_ruta_en_mapas_aleatorios.

#include "busqueda.h"

namespace planner {

// Devuelve false si no hay ruta a la meta.
bool astar(const Mapa& mapa, double costo_giro, Ruta& ruta);

}  // namespace planner
