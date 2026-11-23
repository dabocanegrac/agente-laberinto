#pragma once

// BFS sobre estados (fila, col, orientación), igual que pc/planners/bfs.py:
// mismo orden de acciones (F, L, R) y misma prueba de meta, así que da exactamente la misma ruta.

#include "busqueda.h"

namespace planner {

// Devuelve false si no hay ruta a la meta.
bool bfs(const Mapa& mapa, float costo_giro, Ruta& ruta);

}  // namespace planner
