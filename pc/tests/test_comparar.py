"""A*: la heurística es consistente en todos los mapas. comparar.py: una fila por mapa y algoritmo."""

import csv
from pathlib import Path

import pytest

from pc import comparar
from pc import mapa as mapas
from pc.modelo import ORIENTACIONES, Estado, sucesores
from pc.planners import REGISTRO
from pc.planners.astar import heuristica

RAIZ = Path(__file__).resolve().parents[2]
MAPAS = sorted((RAIZ / "maps").rglob("*.txt"))


@pytest.mark.parametrize("ruta", MAPAS, ids=lambda p: p.stem)
@pytest.mark.parametrize("costo_giro", [0.5, 1.0, 2.0])
def test_heuristica_consistente(ruta, costo_giro):
    """h(meta) = 0 y h(s) <= costo(s, s') + h(s'): con eso es admisible y A* es óptimo."""
    m = mapas.leer(ruta)
    for f in range(m.filas):
        for c in range(m.cols):
            for ori in ORIENTACIONES:
                e = Estado(f, c, ori)
                h = heuristica(e, m.meta, costo_giro)
                if e.celda == m.meta:
                    assert h == 0
                for _, sig, cs in sucesores(m, e, costo_giro):
                    assert h <= cs + heuristica(sig, m.meta, costo_giro) + 1e-9, (e, sig)


def test_heuristica_cuenta_el_giro():
    meta = (0, 3)
    assert heuristica(Estado(0, 0, "E"), meta, 0.5) == 3      # al frente
    assert heuristica(Estado(0, 0, "N"), meta, 0.5) == 3.5    # a un lado
    assert heuristica(Estado(0, 5, "E"), meta, 0.5) == 2.5    # detrás
    assert heuristica(Estado(2, 0, "E"), meta, 0.5) == 5.5    # otra fila y otra columna


def test_comparar_escribe_csv(tmp_path):
    salida = tmp_path / "comparacion.csv"
    assert comparar.main([*map(str, MAPAS), "--repeticiones", "1", "--salida", str(salida)]) == 0
    with salida.open(encoding="utf-8") as f:
        filas = list(csv.DictReader(f))
    assert len(filas) == len(MAPAS) * len(REGISTRO)
    assert set(filas[0]) == set(comparar.COLUMNAS)
    for fila in filas:
        assert fila["ruta"] != "sin ruta"
        assert int(fila["acciones"]) == len(fila["ruta"].split())
        assert int(fila["giros"]) == sum(mov != "F" for mov in fila["ruta"].split())
