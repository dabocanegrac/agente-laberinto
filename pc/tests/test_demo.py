import pytest

from pc import demo
from pc.planners import REGISTRO


def test_lista_los_mapas_de_maps():
    rutas = [m["ruta"] for m in demo.listar_mapas()]
    assert "maps/simulados/ejemplo_5x5.txt" in rutas
    assert all(r.startswith("maps/") and r.endswith(".txt") for r in rutas)


def test_describe_el_mapa_con_todos_los_algoritmos():
    d = demo.describir_mapa("maps/simulados/ejemplo_5x5.txt", 0.5)
    assert (d["filas"], d["cols"]) == (5, 5)
    assert {a["nombre"] for a in d["algoritmos"]} == set(REGISTRO)
    bfs = next(a for a in d["algoritmos"] if a["nombre"] == "bfs")
    assert bfs["acciones"] == len(bfs["ruta"].split())
    assert len(bfs["estados"]) == bfs["acciones"] + 1
    assert bfs["estados"][0] == d["inicio"] and tuple(bfs["estados"][-1][:2]) == tuple(d["meta"])


@pytest.mark.parametrize("ruta", ["../pyproject.toml", "pc/demo.py", "maps/../pc/demo.py", "maps/simulados/ejemplo_5x5.esperado"])
def test_no_sirve_archivos_fuera_de_maps(ruta):
    with pytest.raises(ValueError):
        demo.ruta_mapa(ruta)
