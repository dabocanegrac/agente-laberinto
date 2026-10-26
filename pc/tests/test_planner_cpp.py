"""El planificador del carro (C++) tiene que dar la MISMA ruta que pc/planners/bfs.py en todos los mapas.

Compila arduino/agente/src/planner/ para el PC. Busca el compilador en este orden:
$CXX, g++, clang++ y, si no hay ninguno, `python -m ziglang c++` (pip install ziglang).
Si no encuentra ninguno, el test se salta.
"""

import importlib.util
import os
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
]


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


@pytest.mark.parametrize("costo_giro", [0.5, 1.0, 2.0])
@pytest.mark.parametrize("ruta", MAPAS, ids=lambda p: p.stem)
def test_misma_ruta_que_python(planear_host, ruta, costo_giro):
    r = subprocess.run([str(planear_host), str(ruta), str(costo_giro)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    ruta_cpp, metricas = r.stdout.splitlines()
    acciones, costo, expandidos = metricas.split()

    py = ejecutar(REGISTRO["bfs"](costo_giro), mapas.leer(ruta))
    assert ruta_cpp == py.ruta
    assert int(acciones) == len(py.movimientos)
    assert float(costo) == pytest.approx(py.costo)
    assert int(expandidos) == py.nodos_expandidos


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
