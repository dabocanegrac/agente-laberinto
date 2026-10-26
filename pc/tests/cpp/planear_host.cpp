// Corre el planificador del carro en el PC, para compararlo con Python (pc/tests/test_planner_cpp.py).
//   planear_host <mapa.txt> [costo_giro]
// Imprime la ruta en la primera línea y "acciones costo expandidos" en la segunda.

#include <cstdio>
#include <cstdlib>

#include "../../../arduino/agente/src/planner/bfs.h"

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "uso: planear_host <mapa.txt> [costo_giro]\n");
    return 2;
  }
  float costo_giro = argc > 2 ? (float)std::atof(argv[2]) : 0.5f;
  FILE* f = std::fopen(argv[1], "r");
  if (!f) {
    std::fprintf(stderr, "no se pudo abrir %s\n", argv[1]);
    return 2;
  }
  planner::LectorMapa lector;
  lector.empezar();
  char linea[256];
  while (std::fgets(linea, sizeof(linea), f)) {
    for (char* p = linea; *p; p++)
      if (*p == '\n') *p = '\0';
    if (!lector.linea(linea)) {
      std::printf("ERR %s\n", lector.error());
      return 1;
    }
  }
  std::fclose(f);

  static planner::Mapa mapa;
  if (!lector.terminar(mapa)) {
    std::printf("ERR %s\n", lector.error());
    return 1;
  }
  static planner::Ruta ruta;
  if (!planner::bfs(mapa, costo_giro, ruta)) {
    std::printf("SIN RUTA\n");
    return 1;
  }
  for (int i = 0; i < ruta.n; i++) std::printf(i ? " %c" : "%c", ruta.movimientos[i]);
  std::printf("\n%d %g %d\n", ruta.n, ruta.costo, ruta.expandidos);
  return 0;
}
