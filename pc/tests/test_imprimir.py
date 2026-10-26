from pathlib import Path

import pytest

from pc import imprimir
from pc import mapa as mapas

RAIZ = Path(__file__).resolve().parents[2]
MAPAS = sorted((RAIZ / "maps").rglob("*.txt"))
CELDA, LINEA = 22, 2


def pared_en_grilla(m, gy, gx):
    """¿La posición (gy, gx) de la grilla es negra? Los postes sueltos (sin ninguna pared) no se imprimen."""
    def h(y, c):  # pared horizontal en el borde y, columna c
        return 0 <= c < m.cols and ((y < m.filas and m.hay_pared(y, c, "N")) or (y > 0 and m.hay_pared(y - 1, c, "S")))

    def v(x, f):  # pared vertical en el borde x, fila f
        return 0 <= f < m.filas and ((x < m.cols and m.hay_pared(f, x, "O")) or (x > 0 and m.hay_pared(f, x - 1, "E")))

    if gy % 2 == 0 and gx % 2 == 0:
        y, x = gy // 2, gx // 2
        return h(y, x - 1) or h(y, x) or v(x, y - 1) or v(x, y)
    if gy % 2 == 0:
        return h(gy // 2, gx // 2)
    if gx % 2 == 0:
        return v(gx // 2, gy // 2)
    return False


def rango(g):  # cm que ocupa la posición g de la grilla
    return range(g // 2 * CELDA, g // 2 * CELDA + LINEA) if g % 2 == 0 else range(g // 2 * CELDA + LINEA, (g + 1) // 2 * CELDA)


@pytest.mark.parametrize("ruta", MAPAS, ids=lambda p: p.stem)
def test_piezas_cubren_las_paredes_sin_superponerse(ruta):
    m = mapas.leer(ruta)
    esperado = {(x, y) for gy in range(2 * m.filas + 1) for gx in range(2 * m.cols + 1) if pared_en_grilla(m, gy, gx)
                for y in rango(gy) for x in rango(gx)}
    pintado, area = set(), 0
    for p in imprimir.piezas(m, CELDA, LINEA):
        celdas = {(x, y) for x in range(int(p.x), int(p.x + p.w)) for y in range(int(p.y), int(p.y + p.h))}
        area += len(celdas)
        pintado |= celdas
    assert pintado == esperado
    assert area == len(pintado), "hay piezas superpuestas"


def test_tiras_necesarias():
    n, pedazos = imprimir.tiras_necesarias([60, 50, 40, 130], 100)
    assert sorted(pedazos) == [30, 40, 50, 60, 100]
    assert n == 3  # 100 | 60+40 | 50+30


def test_genera_los_pdf(tmp_path):
    pytest.importorskip("reportlab")
    assert imprimir.main([str(MAPAS[0]), "--salida", str(tmp_path)]) == 0
    for sufijo in ("plotter", "armado"):
        pdf = tmp_path / f"{MAPAS[0].stem}_{sufijo}.pdf"
        assert pdf.read_bytes().startswith(b"%PDF")
