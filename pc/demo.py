"""Interfaz web para la demo: mapa, algoritmos, carro en vivo, telemetría y control manual.

    python -m pc.demo                       # abre http://localhost:8000 en el navegador
    python -m pc.demo --ip 10.55.105.33     # sin buscar el carro
    python -m pc.demo --sin-carro           # solo mapas y algoritmos (sin conectarse)

Sirve pc/demo/index.html y hace de puente con el carro: mantiene la única conexión TCP (el carro atiende un
cliente a la vez, así que mientras corre no se puede usar pc.enviar ni nc) y reenvía cada línea que llega al
navegador por Server-Sent Events. Los comandos del navegador llegan por POST y se mandan tal cual.
Solo usa la librería estándar: la demo no depende de internet ni de pip.
"""

import argparse
import json
import queue
import socket
import sys
import threading
import time
import webbrowser
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

from pc import mapa as mapas
from pc.enviar import PUERTO_TCP, conectar_tcp
from pc.modelo import COSTO_GIRO
from pc.planners import REGISTRO, ejecutar

RAIZ = Path(__file__).resolve().parent.parent
CARPETA_MAPAS = RAIZ / "maps"
PAGINA = Path(__file__).resolve().parent / "demo" / "index.html"


class Carro:
    """Conexión con el carro en un hilo propio: se reconecta sola y publica cada línea a los navegadores."""

    def __init__(self, ip=None, puerto=PUERTO_TCP):
        self.ip = ip
        self.puerto = puerto
        self.sock = None
        self.ip_actual = None
        self.lock = threading.Lock()
        self.clientes: list[queue.Queue] = []
        self.historial = deque(maxlen=400)   # para el navegador que se conecta tarde

    def publicar(self, evento):
        if evento["tipo"] == "linea":
            self.historial.append(evento)
        for q in list(self.clientes):
            q.put(evento)

    def estado(self):
        return {"tipo": "estado", "conectado": self.sock is not None, "ip": self.ip_actual}

    def enviar(self, linea, auto=False):
        with self.lock:
            if self.sock is None:
                return False
            try:
                self.sock.sendall(linea.encode() + b"\n")
            except OSError:
                return False
        self.publicar({"tipo": "enviado", "texto": linea, "auto": auto, "t": time.time()})
        return True

    def correr(self):
        while True:
            try:
                sock = conectar_tcp(self.ip, self.puerto)
            except (OSError, ConnectionError):
                time.sleep(8)   # cada intento sin IP barre la subred: no hacerlo seguido
                continue
            sock.settimeout(None)
            self.ip_actual = sock.getpeername()[0]
            with self.lock:
                self.sock = sock
            self.publicar(self.estado())
            try:
                for crudo in sock.makefile("rb"):
                    texto = crudo.decode(errors="replace").strip()
                    if texto:
                        self.publicar({"tipo": "linea", "texto": texto, "t": time.time()})
            except OSError:
                pass
            with self.lock:
                self.sock = None
            sock.close()
            self.publicar(self.estado())
            time.sleep(1)


def listar_mapas():
    return [{"ruta": str(p.relative_to(RAIZ)), "nombre": str(p.relative_to(CARPETA_MAPAS).with_suffix(""))}
            for p in sorted(CARPETA_MAPAS.rglob("*.txt"))]


def ruta_mapa(relativa):
    p = (RAIZ / relativa).resolve()
    if CARPETA_MAPAS.resolve() not in p.parents or p.suffix != ".txt":
        raise ValueError("el mapa tiene que estar en maps/")
    return p


def describir_mapa(relativa, costo_giro):
    m = mapas.leer(ruta_mapa(relativa))
    algoritmos = []
    for nombre in sorted(REGISTRO):
        cls = REGISTRO[nombre]
        res = ejecutar(cls(costo_giro), m)
        if res is None:
            algoritmos.append({"nombre": nombre, "optimiza": cls.optimiza, "error": "sin ruta"})
            continue
        algoritmos.append({
            "nombre": nombre, "optimiza": cls.optimiza, "ruta": res.ruta, "acciones": len(res.movimientos),
            "costo": res.costo, "expandidos": res.nodos_expandidos, "tiempo_ms": res.tiempo_ms,
            "estados": [[e.fila, e.col, e.ori] for e in res.estados],
        })
    return {
        "ruta": relativa, "nombre": m.nombre, "filas": m.filas, "cols": m.cols, "paredes": m.paredes,
        "inicio": [m.inicio.fila, m.inicio.col, m.inicio.ori], "meta": list(m.meta),
        "texto": mapas.a_texto(m), "algoritmos": algoritmos,
    }


def manejador(carro):
    class Manejador(BaseHTTPRequestHandler):
        def log_message(self, *args):   # sin una línea por cada petición en la terminal
            pass

        def responder(self, codigo, cuerpo, tipo="application/json; charset=utf-8"):
            datos = cuerpo if isinstance(cuerpo, bytes) else json.dumps(cuerpo).encode()
            self.send_response(codigo)
            self.send_header("Content-Type", tipo)
            self.send_header("Content-Length", str(len(datos)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(datos)

        def do_GET(self):
            url = urlparse(self.path)
            q = parse_qs(url.query)
            try:
                if url.path == "/":
                    self.responder(200, PAGINA.read_bytes(), "text/html; charset=utf-8")
                elif url.path == "/api/mapas":
                    self.responder(200, listar_mapas())
                elif url.path == "/api/mapa":
                    costo = float(q.get("costo_giro", [COSTO_GIRO])[0])
                    self.responder(200, describir_mapa(q["ruta"][0], costo))
                elif url.path == "/api/eventos":
                    self.eventos()
                else:
                    self.responder(404, {"error": "no existe"})
            except (KeyError, ValueError, mapas.ErrorMapa) as e:
                self.responder(400, {"error": str(e)})

        def do_POST(self):
            try:
                datos = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
                if self.path == "/api/cmd":
                    linea = str(datos["linea"]).strip()
                    if not linea or "\n" in linea or "\r" in linea:
                        raise ValueError("un comando por vez")
                    self.responder(200, {"ok": carro.enviar(linea, bool(datos.get("auto")))})
                elif self.path == "/api/mapa/enviar":
                    m = mapas.leer(ruta_mapa(datos["ruta"]))
                    costo = float(datos.get("costo_giro", COSTO_GIRO))
                    lineas = [f"COSTO_GIRO {costo:g}", "MAPA", *mapas.a_texto(m).splitlines(), "FIN"]
                    self.responder(200, {"ok": all(carro.enviar(l) for l in lineas)})
                else:
                    self.responder(404, {"error": "no existe"})
            except (KeyError, ValueError, mapas.ErrorMapa) as e:
                self.responder(400, {"error": str(e)})

        def eventos(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            q = queue.Queue()
            q.put(carro.estado())
            for evento in list(carro.historial):
                q.put({**evento, "historial": True})   # el navegador lo muestra en el log sin reaccionar
            carro.clientes.append(q)
            try:
                while True:
                    try:
                        evento = q.get(timeout=15)
                        self.wfile.write(f"data: {json.dumps(evento)}\n\n".encode())
                    except queue.Empty:
                        self.wfile.write(b": sigo aqui\n\n")   # mantiene viva la conexión
                    self.wfile.flush()
            except OSError:
                pass   # el navegador cerró la pestaña
            finally:
                carro.clientes.remove(q)

    return Manejador


def main(argv=None):
    p = argparse.ArgumentParser(description="Interfaz web de la demo del carro.")
    p.add_argument("--ip", help="dirección del carro (por defecto la busca como pc.enviar)")
    p.add_argument("--puerto", type=int, default=8000, help="puerto de la página (por defecto 8000)")
    p.add_argument("--puerto-carro", type=int, default=PUERTO_TCP, help=argparse.SUPPRESS)   # para un carro simulado
    p.add_argument("--sin-carro", action="store_true", help="no conectarse al carro")
    p.add_argument("--no-abrir", action="store_true", help="no abrir el navegador")
    args = p.parse_args(argv)

    carro = Carro(args.ip, args.puerto_carro)
    if not args.sin_carro:
        threading.Thread(target=carro.correr, daemon=True).start()
    servidor = ThreadingHTTPServer(("0.0.0.0", args.puerto), manejador(carro))
    servidor.daemon_threads = True
    url = f"http://localhost:{args.puerto}"
    print(f"Interfaz en {url} (desde otro equipo de la red: http://{socket.gethostname()}:{args.puerto})")
    if not args.no_abrir:
        webbrowser.open(url)
    try:
        servidor.serve_forever()
    except KeyboardInterrupt:
        pass
    return 0


if __name__ == "__main__":
    sys.exit(main())
