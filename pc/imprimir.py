"""Convierte un mapa en PDF para armar el laberinto físico.

    python -m pc.imprimir maps/fisicos/mapa1.txt                 # los dos PDF
    python -m pc.imprimir maps/fisicos/mapa1.txt --celda 22 --linea 2 --margen 5

Genera en build/impresion/:
  <mapa>_plotter.pdf   escala 1:1, una sola página del tamaño del laberinto. Imprimir al 100 % (sin
                       "ajustar a la página") y comprobar la regla de 10 cm del borde.
  <mapa>_armado.pdf    plano con medidas y lista de piezas, para armarlo con cinta aislante o con tiras
                       de cartulina negra sobre una base blanca.

Medidas en cm. "celda" es la distancia entre los centros de dos líneas (el paso de la grilla):
el blanco libre entre dos paredes es celda - linea.

Requiere reportlab: pip install reportlab
"""

from __future__ import annotations

import argparse
import math
import sys
from dataclasses import dataclass
from pathlib import Path

from pc import mapa as mapas

RAIZ = Path(__file__).resolve().parents[1]


@dataclass
class Pieza:
    nombre: str
    horizontal: bool
    x: float      # esquina superior izquierda, en cm, desde la esquina exterior del laberinto
    y: float
    largo: float
    ancho: float

    @property
    def w(self):
        return self.largo if self.horizontal else self.ancho

    @property
    def h(self):
        return self.ancho if self.horizontal else self.largo


def piezas(m: mapas.Mapa, celda: float, linea: float) -> list[Pieza]:
    """Tiras rectas sin superponerse: las horizontales van enteras y las verticales se cortan donde las tocan."""
    # paredes en cada borde de la grilla
    horiz = [[(y < m.filas and m.hay_pared(y, c, "N")) or (y > 0 and m.hay_pared(y - 1, c, "S"))
              for c in range(m.cols)] for y in range(m.filas + 1)]
    vert = [[(x < m.cols and m.hay_pared(f, x, "O")) or (x > 0 and m.hay_pared(f, x - 1, "E"))
             for f in range(m.filas)] for x in range(m.cols + 1)]

    res = []
    for y in range(m.filas + 1):
        c = 0
        while c < m.cols:
            if not horiz[y][c]:
                c += 1
                continue
            c0 = c
            while c < m.cols and horiz[y][c]:
                c += 1
            res.append(Pieza(f"H{len(res) + 1}", True, c0 * celda, y * celda, (c - c0) * celda + linea, linea))

    def poste_cubierto(x, f):  # una horizontal pasa por el poste (x, f)
        return (x > 0 and horiz[f][x - 1]) or (x < m.cols and horiz[f][x])

    n_h = len(res)
    for x in range(m.cols + 1):
        f = 0
        while f < m.filas:
            if not vert[x][f]:
                f += 1
                continue
            f0 = f
            f += 1
            while f < m.filas and vert[x][f] and not poste_cubierto(x, f):
                f += 1
            y_ini = f0 * celda + (linea if poste_cubierto(x, f0) else 0)
            y_fin = f * celda + (0 if poste_cubierto(x, f) else linea)
            res.append(Pieza(f"V{len(res) - n_h + 1}", False, x * celda, y_ini, y_fin - y_ini, linea))
    return res


def tiras_necesarias(largos: list[float], largo_tira: float) -> tuple[int, list[float]]:
    """Cuántas tiras de largo_tira hacen falta (primero las piezas más largas). Las piezas más largas
    que una tira se parten en pedazos."""
    pedazos = []
    for l in largos:
        while l > largo_tira:
            pedazos.append(largo_tira)
            l -= largo_tira
        if l > 0:
            pedazos.append(l)
    tiras = []
    for p in sorted(pedazos, reverse=True):
        for i, libre in enumerate(tiras):
            if libre >= p:
                tiras[i] -= p
                break
        else:
            tiras.append(largo_tira - p)
    return len(tiras), pedazos


def _cm():
    from reportlab.lib.units import cm
    return cm


def pdf_plotter(m, ps, ruta_pdf, celda, linea, margen):
    from reportlab.lib.colors import Color, black
    from reportlab.pdfgen import canvas

    cm = _cm()
    ancho = m.cols * celda + linea + 2 * margen
    alto = m.filas * celda + linea + 2 * margen
    c = canvas.Canvas(str(ruta_pdf), pagesize=(ancho * cm, alto * cm))
    c.setTitle(f"Laberinto {m.nombre} (escala 1:1)")
    c.setFillColor(black)
    for p in ps:  # el PDF mide desde abajo; el mapa desde arriba
        c.rect((margen + p.x) * cm, (alto - margen - p.y - p.h) * cm, p.w * cm, p.h * cm, stroke=0, fill=1)

    # Datos y regla en gris claro, pegados al borde del papel: el sensor frontal se asoma ~4 cm por fuera.
    gris = Color(0.75, 0.75, 0.75)
    c.setFillColor(gris)
    c.setStrokeColor(gris)
    c.setFont("Helvetica", 9)
    i = m.inicio
    c.drawString(0.4 * cm, 0.4 * cm,
                 f"{m.nombre}  |  {m.filas}x{m.cols} celdas de {celda:g} cm, línea {linea:g} cm  |  "
                 f"inicio fila {i.fila} col {i.col} mirando al {i.ori}  |  meta fila {m.meta[0]} col {m.meta[1]}  |  "
                 f"imprimir al 100 %")
    x0 = (ancho - 0.4 - 10) * cm
    c.setLineWidth(1)
    c.line(x0, 0.5 * cm, x0 + 10 * cm, 0.5 * cm)
    for k in range(11):
        c.line(x0 + k * cm, 0.5 * cm, x0 + k * cm, (0.8 if k % 5 == 0 else 0.65) * cm)
    c.drawRightString(x0 - 0.2 * cm, 0.4 * cm, "10 cm:")
    c.showPage()
    c.save()
    return ancho, alto


def pdf_armado(m, ps, ruta_pdf, celda, linea, margen, pliego):
    from reportlab.lib import colors
    from reportlab.lib.pagesizes import landscape, letter
    from reportlab.pdfgen import canvas

    cm = _cm()
    pw, ph = landscape(letter)
    c = canvas.Canvas(str(ruta_pdf), pagesize=(pw, ph))
    c.setTitle(f"Laberinto {m.nombre}: plano y piezas")
    ancho_lab = m.cols * celda + linea
    alto_lab = m.filas * celda + linea
    base_w, base_h = ancho_lab + 2 * margen, alto_lab + 2 * margen

    # ---------- página 1: plano ----------
    c.setFont("Helvetica-Bold", 15)
    c.drawString(1.5 * cm, ph - 1.6 * cm, f"Laberinto {m.nombre}: plano para armar")
    c.setFont("Helvetica", 9.5)
    datos = [
        f"{m.filas}x{m.cols} celdas. Celda {celda:g} cm (entre centros de líneas), línea {linea:g} cm, "
        f"blanco entre paredes {celda - linea:g} cm.",
        f"Laberinto: {ancho_lab:g} x {alto_lab:g} cm. Base blanca: {base_w:g} x {base_h:g} cm "
        f"(con {margen:g} cm de margen).",
        "Medidas desde la esquina exterior superior izquierda del laberinto (punto 0,0), a la esquina "
        "superior izquierda de cada pieza.",
    ]
    for k, t in enumerate(datos):
        c.drawString(1.5 * cm, ph - (2.4 + 0.5 * k) * cm, t)

    caja_x, caja_y = 2.5 * cm, 1.4 * cm
    caja_w, caja_h = pw - 4 * cm, ph - 5.6 * cm
    esc = min(caja_w / (ancho_lab * cm), caja_h / (alto_lab * cm))   # cm reales -> cm en el papel
    ox = caja_x + (caja_w - ancho_lab * cm * esc) / 2
    oy_top = caja_y + caja_h - (caja_h - alto_lab * cm * esc) / 2

    def X(v):
        return ox + v * cm * esc

    def Y(v):
        return oy_top - v * cm * esc

    # cotas de la grilla
    c.setFont("Helvetica", 7)
    c.setFillColor(colors.grey)
    c.setStrokeColor(colors.lightgrey)
    c.setLineWidth(0.3)
    for k in range(m.cols + 1):
        c.line(X(k * celda + linea / 2), Y(0) + 4, X(k * celda + linea / 2), Y(alto_lab))
        c.drawCentredString(X(k * celda), Y(0) + 7, f"{k * celda:g}")
    for k in range(m.filas + 1):
        c.line(X(0) - 4, Y(k * celda + linea / 2), X(ancho_lab), Y(k * celda + linea / 2))
        c.drawRightString(X(0) - 6, Y(k * celda) - 2.5, f"{k * celda:g}")

    # inicio y meta
    c.setFillColor(colors.Color(0.2, 0.55, 0.25))
    c.setFont("Helvetica-Bold", max(7, min(14, celda * cm * esc / 3)))
    cx = lambda col: X(col * celda + celda / 2 + linea / 2)
    cy = lambda fila: Y(fila * celda + celda / 2 + linea / 2)
    c.drawCentredString(cx(m.inicio.col), cy(m.inicio.fila) - 4, f"INICIO {m.inicio.ori}")
    c.setFillColor(colors.Color(0.75, 0.2, 0.15))
    c.drawCentredString(cx(m.meta[1]), cy(m.meta[0]) - 4, "META")

    # piezas
    for p in ps:
        c.setFillColor(colors.black)
        c.rect(X(p.x), Y(p.y + p.h), p.w * cm * esc, p.h * cm * esc, stroke=0, fill=1)
    c.setFont("Helvetica-Bold", 6.5)
    for p in ps:
        c.setFillColor(colors.Color(0.1, 0.35, 0.8))
        if p.horizontal:
            c.drawCentredString(X(p.x + p.w / 2), Y(p.y) + 2, p.nombre)
        else:
            c.drawString(X(p.x + p.w) + 2, Y(p.y + p.h / 2) - 2, p.nombre)
    c.setFillColor(colors.black)
    c.showPage()

    # ---------- página 2: piezas y materiales ----------
    largos = [p.largo for p in ps]
    total = sum(largos)
    largo_tira = max(pliego)
    tiras_por_pliego = math.floor(min(pliego) / linea)
    n_tiras, pedazos = tiras_necesarias(largos, largo_tira)
    partidas = [p for p in ps if p.largo > largo_tira]

    y = ph - 1.6 * cm
    c.setFont("Helvetica-Bold", 15)
    c.drawString(1.5 * cm, y, f"Laberinto {m.nombre}: piezas y materiales")
    c.setFont("Helvetica", 9.5)
    y -= 0.9 * cm
    lineas_txt = [
        f"Total de línea negra: {total / 100:.2f} m en {len(ps)} piezas de {linea:g} cm de ancho.",
        f"Con cinta aislante (19 mm ≈ {linea:g} cm): una pasada por pieza, unos {math.ceil(total / 100 * 1.1)} m "
        "contando 10 % de desperdicio.",
        f"Con cartulina negra: tiras de {linea:g} x {largo_tira:g} cm sacadas de pliegos de "
        f"{pliego[0]:g} x {pliego[1]:g} cm ({tiras_por_pliego} tiras por pliego). Hacen falta {n_tiras} tiras "
        f"= {math.ceil(n_tiras / tiras_por_pliego)} pliego(s).",
        f"Base blanca de {base_w:g} x {base_h:g} cm. Ojo: la cartulina negra y la cinta tienen que ser MATE y "
        "absorber IR; probarlas con los sensores antes (deben dar > 400).",
        "Orden: marcar el punto 0,0, pegar primero las horizontales (H) y después las verticales (V), "
        "que van entre ellas sin montarse.",
    ]
    if partidas:
        lineas_txt.append("Piezas más largas que una tira (van en varios pedazos a tope): "
                          + ", ".join(p.nombre for p in partidas) + ".")
    for t in lineas_txt:
        c.drawString(1.5 * cm, y, t)
        y -= 0.5 * cm

    y -= 0.4 * cm
    cols_x = [1.5, 3.3, 6.0, 8.2, 10.4, 13.0]
    encab = ["Pieza", "Dirección", "x (cm)", "y (cm)", "Largo (cm)", "Ancho (cm)"]

    def encabezado(yy):
        c.setFont("Helvetica-Bold", 9)
        for cx_, t in zip(cols_x, encab):
            c.drawString(cx_ * cm, yy, t)
        c.setLineWidth(0.5)
        c.line(1.5 * cm, yy - 0.15 * cm, 15 * cm, yy - 0.15 * cm)
        c.setFont("Helvetica", 9)
        return yy - 0.5 * cm

    y = encabezado(y)
    for p in ps:
        if y < 1.2 * cm:
            c.showPage()
            y = encabezado(ph - 1.5 * cm)
        fila = [p.nombre, "horizontal" if p.horizontal else "vertical", f"{p.x:g}", f"{p.y:g}", f"{p.largo:g}",
                f"{p.ancho:g}"]
        for cx_, t in zip(cols_x, fila):
            c.drawString(cx_ * cm, y, t)
        y -= 0.42 * cm

    # resumen por largo, para cortar en serie
    y -= 0.3 * cm
    if y < 3 * cm:
        c.showPage()
        y = ph - 1.5 * cm
    c.setFont("Helvetica-Bold", 9)
    c.drawString(1.5 * cm, y, "Para cortar (agrupado por largo):")
    c.setFont("Helvetica", 9)
    y -= 0.45 * cm
    por_largo = {}
    for l in pedazos:
        por_largo[l] = por_largo.get(l, 0) + 1
    texto = "   ".join(f"{n} x {l:g} cm" for l, n in sorted(por_largo.items(), reverse=True))
    for k in range(0, len(texto), 130):
        c.drawString(1.5 * cm, y, texto[k:k + 130])
        y -= 0.42 * cm
    c.showPage()
    c.save()
    return total, n_tiras


def main(argv=None):
    p = argparse.ArgumentParser(description="Genera los PDF para imprimir o armar el laberinto.")
    p.add_argument("mapa")
    p.add_argument("--celda", type=float, default=22.0, help="cm entre centros de líneas (DEC-02)")
    p.add_argument("--linea", type=float, default=2.0, help="grosor de la línea en cm")
    p.add_argument("--margen", type=float, default=5.0, help="margen blanco alrededor, en cm")
    p.add_argument("--pliego", default="70x100", help="tamaño del pliego de cartulina negra, en cm")
    p.add_argument("--salida", default=str(RAIZ / "build" / "impresion"), help="carpeta de salida")
    args = p.parse_args(argv)

    try:
        import reportlab  # noqa: F401
    except ImportError:
        sys.exit("hace falta reportlab: pip install reportlab")

    m = mapas.leer(args.mapa)
    pliego = tuple(float(v) for v in args.pliego.lower().split("x"))
    ps = piezas(m, args.celda, args.linea)
    salida = Path(args.salida)
    salida.mkdir(parents=True, exist_ok=True)

    ruta_plotter = salida / f"{m.nombre}_plotter.pdf"
    ancho, alto = pdf_plotter(m, ps, ruta_plotter, args.celda, args.linea, args.margen)
    ruta_armado = salida / f"{m.nombre}_armado.pdf"
    total, n_tiras = pdf_armado(m, ps, ruta_armado, args.celda, args.linea, args.margen, pliego)

    print(f"{ruta_plotter}  ({ancho:g} x {alto:g} cm, imprimir al 100 %)")
    print(f"{ruta_armado}  ({len(ps)} piezas, {total / 100:.2f} m de línea, {n_tiras} tiras de cartulina)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
