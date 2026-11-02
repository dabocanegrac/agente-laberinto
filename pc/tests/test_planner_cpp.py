"""Los planificadores del carro (C++) tienen que dar la MISMA ruta que su versión en pc/planners/ en todos los mapas.

Compila arduino/agente/src/planner/ para el PC. Busca el compilador en este orden:
$CXX, g++, clang++ y, si no hay ninguno, `python -m ziglang c++` (pip install ziglang).
Si no encuentra ninguno, el test se salta.
"""

import importlib.util
import os
import random
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

from pc import mapa as mapas
from pc.planners import REGISTRO, ejecutar

RAIZ = Path(__file__).resolve().parents[2]
MAPAS = sorted((RAIZ / "maps").rglob("*.txt"))
FUENTES = [
    RAIZ / "pc/tests/cpp/planear_host.cpp",
    RAIZ / "arduino/agente/src/planner/mapa.cpp",
    RAIZ / "arduino/agente/src/planner/bfs.cpp",
    RAIZ / "arduino/agente/src/planner/astar.cpp",
]
# Los que están portados al carro (nombre en REGISTRO = argumento de planear_host).
A_BORDO = ["astar", "bfs"]


def compilador():
    if os.environ.get("CXX"):
        return [os.environ["CXX"]]
    for c in ("g++", "clang++"):
        if shutil.which(c):
            return [c]
    if importlib.util.find_spec("ziglang"):
        return [sys.executable, "-m", "ziglang", "c++"]
    return None


@pytest.fixture(scope="module")
def planear_host(tmp_path_factory):
    cxx = compilador()
    if cxx is None:
        pytest.skip("no hay compilador de C++ (instalar g++/clang++ o pip install ziglang)")
    exe = tmp_path_factory.mktemp("cpp") / ("planear_host.exe" if os.name == "nt" else "planear_host")
    r = subprocess.run([*cxx, "-std=c++17", "-O2", "-w", *map(str, FUENTES), "-o", str(exe)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return exe


@pytest.mark.parametrize("costo_giro", [0.5, 0.8, 1.0, 2.0])
@pytest.mark.parametrize("ruta", MAPAS, ids=lambda p: p.stem)
@pytest.mark.parametrize("algo", A_BORDO)
def test_misma_ruta_que_python(planear_host, algo, ruta, costo_giro):
    r = subprocess.run([str(planear_host), str(ruta), str(costo_giro), algo], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    ruta_cpp, metricas = r.stdout.splitlines()
    acciones, costo, expandidos = metricas.split()

    py = ejecutar(REGISTRO[algo](costo_giro), mapas.leer(ruta))
    assert ruta_cpp == py.ruta
    assert int(acciones) == len(py.movimientos)
    assert float(costo) == pytest.approx(py.costo)
    assert int(expandidos) == py.nodos_expandidos


def mapa_aleatorio(rnd, filas, cols, p_pared):
    """Grilla con paredes internas al azar: tiene ciclos y muchas rutas empatadas (a veces ninguna)."""
    alto, ancho = 2 * filas + 1, 2 * cols + 1
    g = [["1"] * ancho for _ in range(alto)]
    for y in range(1, alto - 1):
        for x in range(1, ancho - 1):
            if y % 2 == 1 and x % 2 == 1:
                g[y][x] = "0"
            elif (y + x) % 2 == 1:   # entre dos celdas
                g[y][x] = "1" if rnd.random() < p_pared else "0"
    inicio = mapas.Estado(rnd.randrange(filas), rnd.randrange(cols), rnd.choice("NESO"))
    meta = (rnd.randrange(filas), rnd.randrange(cols))
    return mapas.desde_grilla(["".join(f) for f in g], inicio, meta)


@pytest.mark.parametrize("algo", A_BORDO)
def test_misma_ruta_en_mapas_aleatorios(planear_host, algo, tmp_path):
    rnd = random.Random(2026)
    for k in range(60):
        filas, cols = (16, 16) if k < 5 else (rnd.randint(1, 16), rnd.randint(1, 16))
        m = mapa_aleatorio(rnd, filas, cols, p_pared=rnd.choice([0.0, 0.2, 0.4]))
        costo_giro = rnd.choice([0.5, 0.8, 1.0, 2.0])
        p = tmp_path / f"m{k}.txt"
        p.write_text(mapas.a_texto(m), encoding="utf-8")
        r = subprocess.run([str(planear_host), str(p), str(costo_giro), algo], capture_output=True, text=True)
        py = ejecutar(REGISTRO[algo](costo_giro), m)
        if py is None:
            assert r.returncode == 1 and r.stdout.startswith("SIN RUTA"), f"mapa {k}"
            continue
        assert r.returncode == 0, f"mapa {k}: " + r.stdout + r.stderr
        ruta_cpp, metricas = r.stdout.splitlines()
        acciones, costo, expandidos = metricas.split()
        assert (ruta_cpp, int(expandidos)) == (py.ruta, py.nodos_expandidos), f"mapa {k}, costo_giro={costo_giro}"
        assert float(costo) == pytest.approx(py.costo, rel=1e-5)


@pytest.mark.parametrize("texto, error", [
    ("inicio 0 0 N\nmeta 0 0\n111\n100\n111\n", "borde"),
    ("inicio 0 0 N\nmeta 0 0\n111\n111\n111\n", "celda"),
    ("meta 0 0\n111\n101\n111\n", "inicio"),
    ("inicio 0 0 N\nmeta 0 0\n111\n1a1\n111\n", "0 y 1"),
])
def test_mismos_errores_que_python(planear_host, tmp_path, texto, error):
    p = tmp_path / "m.txt"
    p.write_text(texto, encoding="utf-8")
    with pytest.raises(mapas.ErrorMapa):
        mapas.leer(p)
    r = subprocess.run([str(planear_host), str(p)], capture_output=True, text=True)
    assert r.returncode == 1 and r.stdout.startswith("ERR") and error in r.stdout
