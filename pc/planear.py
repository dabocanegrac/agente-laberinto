"""Planea la ruta de un mapa.

    python -m pc.planear maps/simulados/ejemplo_5x5.txt                 # BFS
    python -m pc.planear maps/simulados/ejemplo_5x5.txt --algo all      # tabla comparativa
    python -m pc.planear maps/simulados/ejemplo_5x5.txt --costo-giro 0.8
"""

import argparse
import sys

from pc import mapa as mapas
from pc.modelo import COSTO_GIRO
from pc.planners import REGISTRO, ejecutar


def main(argv=None):
    p = argparse.ArgumentParser(description="Planea la ruta F/L/R de un mapa.")
    p.add_argument("mapa")
    p.add_argument("--algo", default="bfs", help=f"{', '.join(sorted(REGISTRO))} o all")
    p.add_argument("--costo-giro", type=float, default=COSTO_GIRO, help="costo de L/R relativo a F")
    args = p.parse_args(argv)

    m = mapas.leer(args.mapa)
    if args.algo == "all":
        nombres = sorted(REGISTRO)
    elif args.algo in REGISTRO:
        nombres = [args.algo]
    else:
        p.error(f"algoritmo desconocido {args.algo!r}; hay: {', '.join(sorted(REGISTRO))}")

    if args.algo != "all":
        res = ejecutar(REGISTRO[nombres[0]](args.costo_giro), m)
        if res is None:
            print("sin ruta", file=sys.stderr)
            return 1
        print(res.ruta)
        print(f"# acciones={len(res.movimientos)} costo={res.costo:g} "
              f"expandidos={res.nodos_expandidos} tiempo={res.tiempo_ms:.2f} ms", file=sys.stderr)
        return 0

    print(f"{'algoritmo':<10} {'acciones':>8} {'costo':>7} {'expandidos':>10} {'ms':>8}  ruta")
    for nombre in nombres:
        res = ejecutar(REGISTRO[nombre](args.costo_giro), m)
        if res is None:
            print(f"{nombre:<10} sin ruta")
        else:
            print(f"{nombre:<10} {len(res.movimientos):>8} {res.costo:>7g} {res.nodos_expandidos:>10} "
                  f"{res.tiempo_ms:>8.2f}  {res.ruta}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
