#include "bfs.h"

namespace planner {

namespace {

// Arreglos estáticos: con 16x16 son ~4 KB, y así no se usa el stack del loop.
int16_t padre[MAX_ESTADOS];
char accion[MAX_ESTADOS];
bool visto[MAX_ESTADOS];
int16_t cola[MAX_ESTADOS];

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
