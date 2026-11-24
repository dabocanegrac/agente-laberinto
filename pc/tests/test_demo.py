import socket

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


def test_crear_servidor_busca_puerto_libre_si_defecto_esta_ocupado(monkeypatch):
    ocupado = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ocupado.bind(("127.0.0.1", 0))
    ocupado.listen(1)
    puerto_ocupado = ocupado.getsockname()[1]
    monkeypatch.setattr(demo, "PUERTO_DEFECTO", puerto_ocupado)
    monkeypatch.setattr(demo, "MAX_INTENTOS_PUERTO", 10)
    try:
        srv = demo.crear_servidor("127.0.0.1", None, demo.manejador(demo.Carro()))
        try:
            assert srv.server_port != puerto_ocupado
            assert srv.server_port > 0
        finally:
            srv.server_close()
    finally:
        ocupado.close()


def test_crear_servidor_puerto_explicito_ocupado_falla():
    ocupado = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    ocupado.bind(("127.0.0.1", 0))
    ocupado.listen(1)
    puerto_ocupado = ocupado.getsockname()[1]
    try:
        with pytest.raises(OSError):
            demo.crear_servidor("127.0.0.1", puerto_ocupado, demo.manejador(demo.Carro()))
    finally:
        ocupado.close()
