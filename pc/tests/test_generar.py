"""Pruebas del generador de mapas aleatorios (pc/generar.py)."""

from __future__ import annotations

from pathlib import Path

import pytest

from pc import mapa as mapas
from pc.generar import CARPETA_SIMULADOS, MAX_COLS, MAX_FILAS, generar_mapa, main
from pc.modelo import Estado
from pc.planners import REGISTRO


def _contar_pasos_libres(m: mapas.Mapa) -> int:
    """Cuenta el número de paredes internas abiertas (valor '0') entre celdas adyacentes."""
    alto = 2 * m.filas + 1
    ancho = 2 * m.cols + 1
    lineas = mapas.a_texto(m).strip().splitlines()
    grilla = lineas[2:]  # omitir inicio y meta
    libres = 0
    for y in range(1, alto - 1):
        for x in range(1, ancho - 1):
            if (y + x) % 2 == 1 and grilla[y][x] == "0":
                libres += 1
    return libres


@pytest.mark.parametrize("filas,cols", [(2, 2), (5, 5), (8, 12), (16, 16)])
def test_genera_mapa_valido_y_soluble(tmp_path: Path, filas: int, cols: int):
    m = generar_mapa(filas, cols, ciclos=0.15, semilla=filas * 10 + cols)
    archivo = tmp_path / f"mapa_{filas}x{cols}.txt"
    archivo.write_text(mapas.a_texto(m), encoding="utf-8")

    cargado = mapas.leer(archivo)
    assert cargado.filas == filas
    assert cargado.cols == cols

    for nombre, cls in REGISTRO.items():
        planner = cls()
        res = planner.planear(cargado, cargado.inicio, cargado.meta)
        assert res is not None, f"el planificador {nombre} no encontró ruta"
        assert res.estados[0] == cargado.inicio
        assert res.estados[-1].celda == cargado.meta


def test_arbol_sin_ciclos_y_grilla_abierta():
    filas, cols = 4, 5
    # Árbol generador sin ciclos: exactamente filas * cols - 1 aristas
    m_arbol = generar_mapa(filas, cols, ciclos=0.0, semilla=42)
    assert _contar_pasos_libres(m_arbol) == filas * cols - 1

    # Grilla completamente abierta: todas las paredes internas abiertas
    m_abierto = generar_mapa(filas, cols, ciclos=1.0, semilla=42)
    assert _contar_pasos_libres(m_abierto) == filas * (cols - 1) + (filas - 1) * cols


def test_semilla_reproducible():
    m1 = generar_mapa(7, 7, ciclos=0.25, semilla=12345)
    m2 = generar_mapa(7, 7, ciclos=0.25, semilla=12345)
    assert mapas.a_texto(m1) == mapas.a_texto(m2)


def test_validaciones_generar():
    with pytest.raises(ValueError, match="el tamaño debe estar entre"):
        generar_mapa(0, 5)
    with pytest.raises(ValueError, match="el tamaño debe estar entre"):
        generar_mapa(MAX_FILAS + 1, 5)
    with pytest.raises(ValueError, match="el tamaño debe estar entre"):
        generar_mapa(5, 0)
    with pytest.raises(ValueError, match="el tamaño debe estar entre"):
        generar_mapa(5, MAX_COLS + 1)
    with pytest.raises(ValueError, match="al menos 2 celdas"):
        generar_mapa(1, 1)
    with pytest.raises(ValueError, match="ciclos debe estar entre"):
        generar_mapa(4, 4, ciclos=-0.1)
    with pytest.raises(ValueError, match="ciclos debe estar entre"):
        generar_mapa(4, 4, ciclos=1.1)
    with pytest.raises(ValueError, match="misma celda"):
        generar_mapa(4, 4, inicio=Estado(0, 0, "E"), meta=(0, 0))


def test_cli_genera_archivos(tmp_path: Path):
    archivo_unico = tmp_path / "individual.txt"
    ret = main([
        "--filas", "5",
        "--cols", "5",
        "--salida", str(archivo_unico),
        "--semilla", "88",
        "--esperado",
    ])
    assert ret == 0
    assert archivo_unico.exists()
    assert archivo_unico.with_suffix(".esperado").exists()
    m_indiv = mapas.leer(archivo_unico)
    assert m_indiv.filas == 5 and m_indiv.cols == 5

    carpeta_lote = tmp_path / "lote"
    ret_lote = main([
        "-n", "3",
        "--filas", "4",
        "--cols", "6",
        "--salida", str(carpeta_lote),
        "--semilla", "50",
    ])
    assert ret_lote == 0
    archivos_generados = sorted(carpeta_lote.glob("*.txt"))
    assert len(archivos_generados) == 3
    for arch in archivos_generados:
        m = mapas.leer(arch)
        assert m.filas == 4 and m.cols == 6


def test_ucs_vs_bfs_diferencia_costo_y_acciones():
    mapa_10x10 = CARPETA_SIMULADOS / "simulado_10x10.txt"
    assert mapa_10x10.exists()
    m = mapas.leer(mapa_10x10)

    bfs = REGISTRO["bfs"]()
    ucs = REGISTRO["ucs"]()

    res_bfs = bfs.planear(m, m.inicio, m.meta)
    res_ucs = ucs.planear(m, m.inicio, m.meta)

    assert res_bfs is not None
    assert res_ucs is not None

    # BFS minimiza número de acciones; UCS minimiza costo acumulado
    assert len(res_bfs.movimientos) < len(res_ucs.movimientos)
    assert res_ucs.costo < res_bfs.costo
