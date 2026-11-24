import json
import threading
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer
from pathlib import Path

import pytest

from pc import demo
from pc.planners import REGISTRO

RAIZ = Path(__file__).resolve().parents[2]


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


def test_describe_mapa_con_inicio_y_meta_personalizados():
    ruta = "maps/simulados/ejemplo_5x5.txt"
    original = (RAIZ / ruta).read_text(encoding="utf-8")

    d = demo.describir_mapa(ruta, 0.5, inicio="3,1,N", meta="0,4")
    assert d["inicio"] == [3, 1, "N"]
    assert d["meta"] == [0, 4]
    assert d["inicio_defecto"] == [0, 0, "E"]
    assert d["meta_defecto"] == [4, 4]
    assert "inicio 3 1 N" in d["texto"]
    assert "meta 0 4" in d["texto"]
    for a in d["algoritmos"]:
        assert "error" not in a
        assert a["estados"][0] == [3, 1, "N"]
        assert a["estados"][-1][:2] == [0, 4]

    assert (RAIZ / ruta).read_text(encoding="utf-8") == original


def test_inicio_igual_a_meta_da_ruta_vacia():
    d = demo.describir_mapa("maps/simulados/ejemplo_5x5.txt", 0.5, inicio=[2, 2, "O"], meta=[2, 2])
    bfs = next(a for a in d["algoritmos"] if a["nombre"] == "bfs")
    assert bfs["acciones"] == 0
    assert bfs["ruta"] == ""
    assert bfs["estados"] == [[2, 2, "O"]]


@pytest.mark.parametrize(
    "kwargs",
    [
        {"inicio": "5,0,N"},
        {"inicio": "-1,0,N"},
        {"inicio": "0,0,X"},
        {"inicio": "0,0"},
        {"inicio": [0.5, 0, "N"]},
        {"meta": "0,5"},
        {"meta": "0,-1"},
        {"meta": "0"},
    ],
)
def test_rechaza_puntos_invalidos(kwargs):
    with pytest.raises(ValueError):
        demo.describir_mapa("maps/simulados/ejemplo_5x5.txt", 0.5, **kwargs)


@pytest.mark.parametrize("ruta", ["../pyproject.toml", "pc/demo.py", "maps/../pc/demo.py", "maps/simulados/ejemplo_5x5.esperado"])
def test_no_sirve_archivos_fuera_de_maps(ruta):
    with pytest.raises(ValueError):
        demo.ruta_mapa(ruta)


class _CarroFalso:
    def __init__(self):
        self.enviadas = []

    def estado(self):
        return {"tipo": "estado", "conectado": True, "ip": "127.0.0.1"}

    def enviar(self, linea, auto=False):
        self.enviadas.append((linea, auto))
        return True


def test_http_api_mapa_y_enviar_con_puntos_personalizados():
    carro = _CarroFalso()
    srv = ThreadingHTTPServer(("127.0.0.1", 0), demo.manejador(carro))
    puerto = srv.server_address[1]
    hilo = threading.Thread(target=srv.serve_forever, daemon=True)
    hilo.start()
    try:
        base = f"http://127.0.0.1:{puerto}"
        with urllib.request.urlopen(f"{base}/api/mapa?ruta=maps/fisicos/mapa1.txt&inicio=2,1,O&meta=0,2") as r:
            assert r.status == 200
            d = json.loads(r.read())
        assert d["inicio"] == [2, 1, "O"]
        assert d["meta"] == [0, 2]
        assert d["inicio_defecto"] == [0, 0, "E"]
        assert d["meta_defecto"] == [3, 2]

        req_bad = urllib.request.Request(f"{base}/api/mapa?ruta=maps/fisicos/mapa1.txt&inicio=9,9,N")
        with pytest.raises(urllib.error.HTTPError) as exc:
            urllib.request.urlopen(req_bad)
        assert exc.value.code == 400

        cuerpo = json.dumps({
            "ruta": "maps/fisicos/mapa1.txt",
            "costo_giro": 0.75,
            "inicio": [2, 1, "O"],
            "meta": [0, 2],
        }).encode()
        req_post = urllib.request.Request(f"{base}/api/mapa/enviar", data=cuerpo, method="POST")
        with urllib.request.urlopen(req_post) as r:
            assert r.status == 200
            assert json.loads(r.read()) == {"ok": True}

        lineas = [l for l, _ in carro.enviadas]
        assert lineas[0] == "COSTO_GIRO 0.75"
        assert lineas[1] == "MAPA"
        assert lineas[2] == "inicio 2 1 O"
        assert lineas[3] == "meta 0 2"
        assert lineas[-1] == "FIN"
    finally:
        srv.shutdown()
        srv.server_close()
        hilo.join(2)
