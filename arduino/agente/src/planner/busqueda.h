#pragma once

// Lo que comparten los planificadores del carro (bfs.cpp, astar.cpp): la ruta que devuelven y el modelo
// de estados (fila, col, orientación) con las acciones F/L/R, igual que pc/modelo.py.

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

const int DF[] = {-1, 0, 1, 0};   // N E S O
const int DC[] = {0, 1, 0, -1};
const char ACCIONES[] = {'F', 'L', 'R'};   // mismo orden que pc/modelo.py: misma ruta en los empates

inline int indice(const Mapa& m, const Estado& e) { return (e.fila * m.cols + e.col) * 4 + e.ori; }

inline Estado estado_de(const Mapa& m, int i) {
  return {(int8_t)(i / 4 / m.cols), (int8_t)(i / 4 % m.cols), (uint8_t)(i % 4)};
}

// Devuelve false si la acción choca con una pared.
inline bool aplicar(const Mapa& m, const Estado& e, char a, Estado& sig) {
  sig = e;
  if (a == 'L') sig.ori = (e.ori + 3) % 4;
  else if (a == 'R') sig.ori = (e.ori + 1) % 4;
  else {
    if (m.hay_pared(e.fila, e.col, e.ori)) return false;
    sig.fila += DF[e.ori];
    sig.col += DC[e.ori];
  }
  return true;
}

}  // namespace planner
