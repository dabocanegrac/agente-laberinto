#pragma once

// Mapa del laberinto en formato de grilla (2n+1)x(2n+1) de 0/1, el mismo que lee pc/mapa.py:
//
//   inicio 0 0 E        fila col orientación (N/E/S/O), en celdas
//   meta 2 2            fila col, en celdas
//   1111111             una línea por fila de la grilla: 1 = pared, 0 = libre
//   1000101             la celda (f, c) está en la posición (2f+1, 2c+1);
//   ...                 las paredes, entre celdas
//
// Los espacios dentro de una fila se ignoran y todo lo que va después de '#' es comentario.
// No usa Arduino.h: compila igual en el carro y en el PC (pc/tests/test_planner_cpp.py).

#include <stdint.h>

namespace planner {

const int MAX_FILAS = 16;   // en celdas
const int MAX_COLS = 16;
const int MAX_LINEA = 2 * MAX_COLS + 1;

// Orientaciones en sentido horario: girar a la derecha suma 1.
enum Ori : uint8_t { N = 0, E = 1, S = 2, O = 3 };
const char LETRA_ORI[] = "NESO";

struct Estado {
  int8_t fila;
  int8_t col;
  uint8_t ori;
};

struct Mapa {
  int filas;
  int cols;
  uint8_t paredes[MAX_FILAS][MAX_COLS];   // bits N=1 E=2 S=4 O=8
  Estado inicio;
  int8_t meta_fila;
  int8_t meta_col;

  bool hay_pared(int fila, int col, uint8_t ori) const { return paredes[fila][col] & (1 << ori); }
};

// Lee el mapa línea por línea, para recibirlo por Serial o TCP sin guardar todo el texto.
//   lector.empezar(); for (cada línea) lector.linea(txt); lector.terminar(mapa);
// Si algo falla, devuelve false y error() dice por qué.
class LectorMapa {
 public:
  void empezar();
  bool linea(const char* texto);
  bool terminar(Mapa& mapa);
  const char* error() const { return error_; }

 private:
  bool fallar(const char* msg);

  char grilla_[2 * MAX_FILAS + 1][MAX_LINEA + 1];
  int n_filas_grilla_;
  int ancho_;
  Estado inicio_;
  int meta_fila_, meta_col_;
  bool hay_inicio_, hay_meta_;
  int n_linea_;
  char error_[72];
};

}  // namespace planner
