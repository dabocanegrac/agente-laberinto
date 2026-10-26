#pragma once

// BFS sobre estados (fila, col, orientación), igual que pc/planners/bfs.py:
// mismo orden de acciones (F, L, R) y misma prueba de meta, así que da exactamente la misma ruta.

#include "mapa.h"

namespace planner {

const int MAX_ESTADOS = MAX_FILAS * MAX_COLS * 4;
const int MAX_MOVIMIENTOS = MAX_ESTADOS;

struct Ruta {
  char movimientos[MAX_MOVIMIENTOS];   // 'F', 'L' o 'R'
  int n;
  float costo;
  int expandidos;
};

// Devuelve false si no hay ruta a la meta.
bool bfs(const Mapa& mapa, float costo_giro, Ruta& ruta);

}  // namespace planner
