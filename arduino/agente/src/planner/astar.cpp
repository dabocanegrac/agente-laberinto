#include "astar.h"

namespace planner {

namespace {

// Cada estado entra a la frontera una vez por cada vez que mejora su g: como mucho una por arista.
const int MAX_FRONTERA = MAX_ESTADOS * 3 + 1;
const double INFINITO = 1e30;

// Los costos van en double, como en Python, para que los empates (y la ruta) sean los mismos.
struct Nodo {
  double f;
  uint16_t orden;   // desempate: a igual f, el que entró primero
  int16_t estado;
};

// Arreglos estáticos (~60 KB con 16x16, casi todo la frontera), para no usar el stack del loop.
int16_t padre[MAX_ESTADOS];
char accion[MAX_ESTADOS];
double g[MAX_ESTADOS];
bool cerrado[MAX_ESTADOS];
Nodo frontera[MAX_FRONTERA];   // montículo binario: el menor (f, orden) en frontera[0]
int n_frontera;

bool menor(const Nodo& a, const Nodo& b) { return a.f < b.f || (a.f == b.f && a.orden < b.orden); }

void meter(const Nodo& x) {
  int i = n_frontera++;
  while (i > 0 && menor(x, frontera[(i - 1) / 2])) {
    frontera[i] = frontera[(i - 1) / 2];
    i = (i - 1) / 2;
  }
  frontera[i] = x;
}

Nodo sacar() {
  Nodo top = frontera[0], x = frontera[--n_frontera];
  int i = 0;
  while (true) {
    int h = 2 * i + 1;
    if (h >= n_frontera) break;
    if (h + 1 < n_frontera && menor(frontera[h + 1], frontera[h])) h++;
    if (!menor(frontera[h], x)) break;
    frontera[i] = frontera[h];
    i = h;
  }
  frontera[i] = x;
  return top;
}

// Distancia Manhattan, más un giro si la meta no queda al frente (en línea recta adelante).
double heuristica(const Mapa& m, const Estado& e, double costo_giro) {
  int df = m.meta_fila - e.fila, dc = m.meta_col - e.col;
  if (df == 0 && dc == 0) return 0;
  double h = (df < 0 ? -df : df) + (dc < 0 ? -dc : dc);
  bool al_frente = DF[e.ori] == 0 ? (df == 0 && dc * DC[e.ori] > 0) : (dc == 0 && df * DF[e.ori] > 0);
  return al_frente ? h : h + costo_giro;
}

}  // namespace

bool astar(const Mapa& m, double costo_giro, Ruta& ruta) {
  int total = m.filas * m.cols * 4;
  for (int i = 0; i < total; i++) {
    g[i] = INFINITO;
    cerrado[i] = false;
  }

  int ini = indice(m, m.inicio);
  g[ini] = 0;
  padre[ini] = -1;
  uint16_t orden = 0;
  n_frontera = 0;
  meter({heuristica(m, m.inicio, costo_giro), orden++, (int16_t)ini});
  ruta.expandidos = 0;

  while (n_frontera > 0) {
    int actual = sacar().estado;
    if (cerrado[actual]) continue;   // entrada vieja: ya salió con un g menor
    Estado e = estado_de(m, actual);
    if (e.fila == m.meta_fila && e.col == m.meta_col) {
      int n = 0;
      for (int i = actual; padre[i] != -1; i = padre[i]) n++;
      ruta.n = n;
      for (int i = actual; padre[i] != -1; i = padre[i]) ruta.movimientos[--n] = accion[i];
      ruta.costo = (float)g[actual];
      return true;
    }
    cerrado[actual] = true;
    ruta.expandidos++;
    for (char a : ACCIONES) {
      Estado sig;
      if (!aplicar(m, e, a, sig)) continue;
      int j = indice(m, sig);
      double nuevo = g[actual] + (a == 'F' ? 1.0 : costo_giro);
      if (cerrado[j] || nuevo >= g[j]) continue;
      g[j] = nuevo;
      padre[j] = (int16_t)actual;
      accion[j] = a;
      meter({nuevo + heuristica(m, sig, costo_giro), orden++, (int16_t)j});
    }
  }
  return false;
}

}  // namespace planner
