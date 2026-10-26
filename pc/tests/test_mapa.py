from pathlib import Path

import pytest

from pc import mapa as mapas

RAIZ = Path(__file__).resolve().parents[2]
MAPAS = sorted((RAIZ / "maps").rglob("*.txt"))


def escribir(tmp_path, texto):
    p = tmp_path / "m.txt"
    p.write_text(texto, encoding="utf-8")
    return p


def test_lee_mapa_minimo(tmp_path):
    # 1x2 celdas, sin pared entre las dos
    m = mapas.leer(escribir(tmp_path, "inicio 0 0 E\nmeta 0 1\n11111\n10001\n11111\n"))
    assert (m.filas, m.cols, m.meta) == (1, 2, (0, 1))
    assert m.inicio.ori == "E"
    assert not m.hay_pared(0, 0, "E")
    assert m.hay_pared(0, 0, "N") and m.hay_pared(0, 1, "E")


def test_espacios_y_comentarios(tmp_path):
    m = mapas.leer(escribir(tmp_path, "# hola\ninicio 0 0 E  # al este\nmeta 0 1\n1 1 1 1 1\n1 0 1 0 1\n1 1 1 1 1\n"))
    assert m.hay_pared(0, 0, "E")


def test_falta_borde(tmp_path):
    with pytest.raises(mapas.ErrorMapa, match="borde"):
        mapas.leer(escribir(tmp_path, "inicio 0 0 N\nmeta 0 0\n111\n100\n111\n"))


def test_celda_marcada_como_pared(tmp_path):
    with pytest.raises(mapas.ErrorMapa, match="celda"):
        mapas.leer(escribir(tmp_path, "inicio 0 0 N\nmeta 0 0\n111\n111\n111\n"))


def test_tamano_par(tmp_path):
    with pytest.raises(mapas.ErrorMapa, match="2n\\+1"):
        mapas.leer(escribir(tmp_path, "inicio 0 0 N\nmeta 0 0\n1111\n1001\n1111\n"))


def test_inicio_fuera(tmp_path):
    with pytest.raises(mapas.ErrorMapa, match="inicio"):
        mapas.leer(escribir(tmp_path, "inicio 3 0 N\nmeta 0 0\n111\n101\n111\n"))


@pytest.mark.parametrize("ruta", MAPAS, ids=lambda p: p.stem)
def test_a_texto_ida_y_vuelta(ruta, tmp_path):
    m = mapas.leer(ruta)
    otra = mapas.leer(escribir(tmp_path, mapas.a_texto(m)))
    assert (otra.paredes, otra.inicio, otra.meta) == (m.paredes, m.inicio, m.meta)
