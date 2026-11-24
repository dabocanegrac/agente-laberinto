"""Corre todos los algoritmos en todos los mapas y deja la tabla en resultados/ para el informe.

    python -m pc.comparar                                    # todos los mapas de maps/
    python -m pc.comparar maps/fisicos/mapa1.txt             # solo esos mapas
    python -m pc.comparar --costo-giro 0.8 --repeticiones 20

El tiempo es la mediana de --repeticiones corridas (una sola es muy ruidosa en mapas pequeños).
"""

import argparse
import csv
import statistics
import sys
from pathlib import Path

from pc import mapa as mapas
from pc.modelo import COSTO_GIRO
from pc.planners import REGISTRO, ejecutar

RAIZ = Path(__file__).resolve().parents[1]
COLUMNAS = ["mapa", "algoritmo", "optimiza", "acciones", "giros", "costo", "nodos_expandidos", "tiempo_ms", "ruta"]


def comparar(rutas, costo_giro=COSTO_GIRO, repeticiones=5):
    """Una fila (dict con COLUMNAS) por cada mapa y algoritmo, en orden."""
    filas = []
    for ruta in rutas:
        m = mapas.leer(ruta)
        for nombre in sorted(REGISTRO):
            cls = REGISTRO[nombre]
            corridas = [ejecutar(cls(costo_giro), m) for _ in range(repeticiones)]
            res = corridas[0]
            fila = {"mapa": m.nombre, "algoritmo": nombre, "optimiza": cls.optimiza or "-"}
            if res is None:
                fila.update(acciones="", giros="", costo="", nodos_expandidos="", tiempo_ms="", ruta="sin ruta")
            else:
                fila.update(
                    acciones=len(res.movimientos),
                    giros=sum(mov != "F" for mov in res.movimientos),
                    costo=f"{res.costo:g}",
                    nodos_expandidos=res.nodos_expandidos,
                    tiempo_ms=f"{statistics.median(r.tiempo_ms for r in corridas):.3f}",
                    ruta=res.ruta,
                )
            filas.append(fila)
    return filas


def guardar(filas, salida):
    salida = Path(salida)
    salida.parent.mkdir(parents=True, exist_ok=True)
    with salida.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=COLUMNAS, lineterminator="\n")
        w.writeheader()
        w.writerows(filas)


def main(argv=None):
    p = argparse.ArgumentParser(description="Compara todos los planificadores en todos los mapas.")
    p.add_argument("mapas", nargs="*", help="por defecto, todos los maps/**/*.txt")
    p.add_argument("--costo-giro", type=float, default=COSTO_GIRO, help="costo de L/R relativo a F")
    p.add_argument("--repeticiones", type=int, default=5, help="corridas por algoritmo para medir el tiempo")
    p.add_argument("--salida", default=str(RAIZ / "resultados" / "comparacion.csv"))
    args = p.parse_args(argv)
    if args.repeticiones < 1:
        p.error("--repeticiones debe ser al menos 1")

    rutas = args.mapas or sorted((RAIZ / "maps").rglob("*.txt"))
    filas = comparar(rutas, args.costo_giro, args.repeticiones)
    guardar(filas, args.salida)

    print(f"{'mapa':<14} {'algoritmo':<10} {'acciones':>8} {'giros':>5} {'costo':>7} {'expandidos':>10} {'ms':>8}")
    for f in filas:
        print(f"{f['mapa']:<14} {f['algoritmo']:<10} {f['acciones']:>8} {f['giros']:>5} {f['costo']:>7} "
              f"{f['nodos_expandidos']:>10} {f['tiempo_ms']:>8}")
    print(f"# costo_giro={args.costo_giro:g}, {len(filas)} filas en {args.salida}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
