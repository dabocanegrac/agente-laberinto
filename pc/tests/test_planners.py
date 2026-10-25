"""Tests genéricos: corren con TODOS los planificadores registrados y TODOS los mapas de maps/.

    python -m pytest pc/tests

Un algoritmo nuevo queda probado sin escribir tests: basta con registrarlo.
"""

import heapq
from itertools import count
from pathlib import Path

import pytest

from pc import mapa as mapas
from pc.modelo import aplicar, costo, sucesores
from pc.planners import REGISTRO, ejecutar

RAIZ = Path(__file__).resolve().parents[2]
MAPAS = sorted((RAIZ / "maps").rglob("*.txt"))


def mejor_costo(m, costo_giro):
    """Dijkstra de referencia, independiente de los planificadores."""
    vistos, n = set(), count()
    cola = [(0.0, next(n), m.inicio)]
    while cola:
        c, _, e = heapq.heappop(cola)
        if e in vistos:
            continue
        if e.celda == m.meta:
            return c
        vistos.add(e)
        for _, sig, cs in sucesores(m, e, costo_giro):
            heapq.heappush(cola, (c + cs, next(n), sig))
    return None


def menos_acciones(m):
    return mejor_costo(m, costo_giro=1.0)  # con todo a costo 1, el costo es el número de acciones


@pytest.fixture(params=MAPAS, ids=lambda p: p.stem)
def mapa(request):
    return mapas.leer(request.param)


@pytest.mark.parametrize("nombre", sorted(REGISTRO))
def test_ruta_valida(nombre, mapa):
    planner = REGISTRO[nombre]()
    res = ejecutar(planner, mapa)
    assert res is not None, "no encontró ruta"
    estado = mapa.inicio
    assert res.estados[0] == estado
    assert len(res.estados) == len(res.movimientos) + 1
    for i, mov in enumerate(res.movimientos):
        estado = aplicar(mapa, estado, mov)
        assert estado is not None, f"el movimiento {i} ({mov}) atraviesa una pared"
        assert res.estados[i + 1] == estado
    assert estado.celda == mapa.meta
    assert res.costo == pytest.approx(sum(costo(m, planner.costo_giro) for m in res.movimientos))


@pytest.mark.parametrize("nombre", sorted(n for n, cls in REGISTRO.items() if cls.optimiza == "acciones"))
def test_optimo_en_acciones(nombre, mapa):
    res = ejecutar(REGISTRO[nombre](), mapa)
    assert len(res.movimientos) == menos_acciones(mapa)


@pytest.mark.parametrize("nombre", sorted(n for n, cls in REGISTRO.items() if cls.optimiza == "costo"))
def test_optimo_en_costo(nombre, mapa):
    planner = REGISTRO[nombre]()
    res = ejecutar(planner, mapa)
    assert res.costo == pytest.approx(mejor_costo(mapa, planner.costo_giro))


@pytest.mark.parametrize("ruta", MAPAS, ids=lambda p: p.stem)
def test_bfs_ruta_esperada(ruta):
    """Si existe maps/.../<mapa>.esperado, la ruta de BFS no puede cambiar sin querer."""
    esperado = ruta.with_suffix(".esperado")
    if not esperado.exists():
        pytest.skip("sin archivo .esperado")
    assert ejecutar(REGISTRO["bfs"](), mapas.leer(ruta)).ruta == esperado.read_text(encoding="utf-8").strip()
