"""El simulador sigue la ruta igual que modelo.aplicar, detecta choques y dibuja el mapa como el comentario del archivo."""

import io
from pathlib import Path

import pytest

from pc import mapa as mapas
from pc import simulador
from pc.planners import REGISTRO, ejecutar

RAIZ = Path(__file__).resolve().parents[2]
MAPAS = sorted((RAIZ / "maps").rglob("*.txt"))
EJEMPLO = RAIZ / "maps/simulados/ejemplo_5x5.txt"


@pytest.mark.parametrize("ruta", MAPAS, ids=lambda p: p.stem)
@pytest.mark.parametrize("nombre", sorted(REGISTRO))
def test_simula_la_ruta_de_cada_planificador(nombre, ruta):
    m = mapas.leer(ruta)
    res = ejecutar(REGISTRO[nombre](), m)
    assert simulador.simular(m, res.movimientos) == res.estados


def test_detecta_choque():
    m = mapas.leer(EJEMPLO)  # inicio (0, 0) mirando al este; al norte hay pared
    with pytest.raises(simulador.ErrorSimulacion) as e:
        simulador.simular(m, ["F", "L", "F"])
    assert e.value.paso == 2


def test_cuadro_coincide_con_el_dibujo_del_mapa():
    """El dibujo del comentario de ejemplo_5x5.txt, con el carro en el inicio."""
    dibujo = [l[4:] for l in EJEMPLO.read_text(encoding="utf-8").splitlines() if l.startswith("#   ")]
    m = mapas.leer(EJEMPLO)
    esperado = "\n".join(dibujo).replace("S", ">")
    assert simulador.cuadro(m, [m.inicio]) == esperado


def test_main_texto(capsys):
    assert simulador.main([str(EJEMPLO), "--texto", "--pausa", "0"]) == 0
    out = capsys.readouterr()
    assert "paso 15/15" in out.out and "llegó a la meta" in out.err


def test_main_ruta_que_choca(capsys):
    assert simulador.main([str(EJEMPLO), "--texto", "--pausa", "0", "--ruta", "F L F"]) == 1
    assert "choca con una pared" in capsys.readouterr().err


def test_main_ruta_que_no_llega(capsys):
    assert simulador.main([str(EJEMPLO), "--texto", "--pausa", "0", "--ruta", "F"]) == 1
    assert "no en la meta" in capsys.readouterr().err


def test_ventana_sin_pantalla(monkeypatch):
    plt = pytest.importorskip("matplotlib.pyplot")
    plt.switch_backend("Agg")
    monkeypatch.setattr(plt, "show", lambda: None)
    m = mapas.leer(EJEMPLO)
    res = ejecutar(REGISTRO["bfs"](), m)
    anim = simulador.animar_ventana(m, res.movimientos, res.estados, 0.01, "prueba")
    for i in range(len(res.estados)):
        anim._func(i)  # dibujar cada cuadro sin abrir la ventana
    plt.close("all")
