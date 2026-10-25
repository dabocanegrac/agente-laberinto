import pytest

from pc import mapa as mapas


def escribir(tmp_path, texto):
    p = tmp_path / "m.txt"
    p.write_text(texto, encoding="utf-8")
    return p


def test_lee_mapa_minimo(tmp_path):
    m = mapas.leer(escribir(tmp_path, "1 2\ninicio 0 0 E\nmeta 0 1\n13 7\n"))
    assert (m.filas, m.cols, m.meta) == (1, 2, (0, 1))
    assert m.inicio.ori == "E"
    assert not m.hay_pared(0, 0, "E")


def test_pared_compartida_inconsistente(tmp_path):
    # (0,0) no tiene pared E pero (0,1) sí tiene pared O
    with pytest.raises(mapas.ErrorMapa, match="no coincide"):
        mapas.leer(escribir(tmp_path, "1 2\ninicio 0 0 E\nmeta 0 1\n13 15\n"))


def test_falta_borde(tmp_path):
    with pytest.raises(mapas.ErrorMapa, match="borde"):
        mapas.leer(escribir(tmp_path, "1 1\ninicio 0 0 N\nmeta 0 0\n14\n"))


def test_inicio_fuera(tmp_path):
    with pytest.raises(mapas.ErrorMapa, match="inicio"):
        mapas.leer(escribir(tmp_path, "1 1\ninicio 3 0 N\nmeta 0 0\n15\n"))
