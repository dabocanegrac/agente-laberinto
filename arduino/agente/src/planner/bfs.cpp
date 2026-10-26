#include "bfs.h"

namespace planner {

namespace {

const int DF[] = {-1, 0, 1, 0};   // N E S O
const int DC[] = {0, 1, 0, -1};
const char ACCIONES[] = {'F', 'L', 'R'};

// Arreglos estáticos: con 16x16 son ~4 KB, y así no se usa el stack del loop.
int16_t padre[MAX_ESTADOS];
char accion[MAX_ESTADOS];
bool visto[MAX_ESTADOS];
int16_t cola[MAX_ESTADOS];

int indice(const Mapa& m, const Estado& e) { return (e.fila * m.cols + e.col) * 4 + e.ori; }

Estado estado_de(const Mapa& m, int i) {
  return {(int8_t)(i / 4 / m.cols), (int8_t)(i / 4 % m.cols), (uint8_t)(i % 4)};
}

bool aplicar(const Mapa& m, const Estado& e, char a, Estado& sig) {
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

}  // namespace

bool bfs(const Mapa& m, float costo_giro, Ruta& ruta) {
  int total = m.filas * m.cols * 4;
  for (int i = 0; i < total; i++) visto[i] = false;

  int ini = indice(m, m.inicio);
  visto[ini] = true;
  padre[ini] = -1;
  int cabeza = 0, fin = 0;
  cola[fin++] = ini;
  ruta.expandidos = 0;

  while (cabeza < fin) {
    int actual = cola[cabeza++];
    Estado e = estado_de(m, actual);
    if (e.fila == m.meta_fila && e.col == m.meta_col) {
      int n = 0;
      for (int i = actual; padre[i] != -1; i = padre[i]) n++;
      ruta.n = n;
      ruta.costo = 0;
      for (int i = actual; padre[i] != -1; i = padre[i]) {
        ruta.movimientos[--n] = accion[i];
        ruta.costo += accion[i] == 'F' ? 1.0f : costo_giro;
      }
      return true;
    }
    ruta.expandidos++;
    for (char a : ACCIONES) {
      Estado sig;
      if (!aplicar(m, e, a, sig)) continue;
      int j = indice(m, sig);
      if (visto[j]) continue;
      visto[j] = true;
      padre[j] = (int16_t)actual;
      accion[j] = a;
      cola[fin++] = (int16_t)j;
    }
  }
  return false;
}

}  // namespace planner
