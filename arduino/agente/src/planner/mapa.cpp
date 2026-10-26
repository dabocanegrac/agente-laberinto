#include "mapa.h"

#include <stdio.h>
#include <string.h>

namespace planner {

namespace {

// Copia la línea sin el comentario y sin espacios al inicio y al final.
void limpiar(const char* texto, char* salida, int max) {
  int n = 0;
  for (const char* p = texto; *p && *p != '#' && n < max - 1; p++) salida[n++] = *p;
  while (n > 0 && (salida[n - 1] == ' ' || salida[n - 1] == '\t' || salida[n - 1] == '\r')) n--;
  salida[n] = '\0';
  int i = 0;
  while (salida[i] == ' ' || salida[i] == '\t') i++;
  if (i) memmove(salida, salida + i, n - i + 1);
}

int indice_ori(char letra) {
  const char* p = strchr(LETRA_ORI, letra);
  return (letra && p) ? (int)(p - LETRA_ORI) : -1;
}

}  // namespace

void LectorMapa::empezar() {
  n_filas_grilla_ = 0;
  ancho_ = 0;
  hay_inicio_ = hay_meta_ = false;
  n_linea_ = 0;
  error_[0] = '\0';
}

bool LectorMapa::fallar(const char* msg) {
  snprintf(error_, sizeof(error_), "linea %d: %s", n_linea_, msg);
  return false;
}

bool LectorMapa::linea(const char* texto) {
  n_linea_++;
  char l[96];
  limpiar(texto, l, sizeof(l));
  if (!l[0]) return true;

  if (strncmp(l, "inicio", 6) == 0) {
    int f, c;
    char o;
    if (sscanf(l + 6, "%d %d %c", &f, &c, &o) != 3 || indice_ori(o) < 0)
      return fallar("se esperaba 'inicio fila col N|E|S|O'");
    inicio_ = {(int8_t)f, (int8_t)c, (uint8_t)indice_ori(o)};
    hay_inicio_ = true;
    return true;
  }
  if (strncmp(l, "meta", 4) == 0) {
    if (sscanf(l + 4, "%d %d", &meta_fila_, &meta_col_) != 2) return fallar("se esperaba 'meta fila col'");
    hay_meta_ = true;
    return true;
  }

  // Fila de la grilla
  if (n_filas_grilla_ >= 2 * MAX_FILAS + 1) return fallar("demasiadas filas");
  char* fila = grilla_[n_filas_grilla_];
  int n = 0;
  for (const char* p = l; *p; p++) {
    if (*p == ' ' || *p == '\t') continue;
    if (*p != '0' && *p != '1') return fallar("la grilla solo puede tener 0 y 1");
    if (n >= MAX_LINEA) return fallar("fila demasiado larga");
    fila[n++] = *p;
  }
  fila[n] = '\0';
  if (n_filas_grilla_ == 0) ancho_ = n;
  else if (n != ancho_) return fallar("todas las filas de la grilla deben tener el mismo ancho");
  n_filas_grilla_++;
  return true;
}

bool LectorMapa::terminar(Mapa& m) {
  n_linea_ = 0;
  if (!hay_inicio_ || !hay_meta_) return fallar("faltan las lineas 'inicio' y/o 'meta'");
  int alto = n_filas_grilla_;
  if (alto < 3 || ancho_ < 3 || alto % 2 == 0 || ancho_ % 2 == 0)
    return fallar("la grilla debe ser de (2n+1)x(2m+1), minimo 3x3");

  m.filas = alto / 2;
  m.cols = ancho_ / 2;
  for (int y = 0; y < alto; y++) {
    for (int x = 0; x < ancho_; x++) {
      bool borde = y == 0 || x == 0 || y == alto - 1 || x == ancho_ - 1;
      if (borde && grilla_[y][x] != '1') return fallar("falta pared en el borde");
      if (y % 2 == 1 && x % 2 == 1 && grilla_[y][x] != '0') return fallar("una celda no puede ser pared");
    }
  }
  for (int f = 0; f < m.filas; f++) {
    for (int c = 0; c < m.cols; c++) {
      int y = 2 * f + 1, x = 2 * c + 1;
      m.paredes[f][c] = (grilla_[y - 1][x] == '1') * 1 + (grilla_[y][x + 1] == '1') * 2 +
                        (grilla_[y + 1][x] == '1') * 4 + (grilla_[y][x - 1] == '1') * 8;
    }
  }
  if (inicio_.fila < 0 || inicio_.fila >= m.filas || inicio_.col < 0 || inicio_.col >= m.cols)
    return fallar("inicio fuera del mapa");
  if (meta_fila_ < 0 || meta_fila_ >= m.filas || meta_col_ < 0 || meta_col_ >= m.cols)
    return fallar("meta fuera del mapa");
  m.inicio = inicio_;
  m.meta_fila = (int8_t)meta_fila_;
  m.meta_col = (int8_t)meta_col_;
  return true;
}

}  // namespace planner
