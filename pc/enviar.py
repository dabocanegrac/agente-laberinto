"""Manda un mapa al carro. El carro calcula la ruta a bordo y la devuelve.

    python -m pc.enviar maps/simulados/ejemplo_5x5.txt                    # por Wi-Fi (red del carro o hotspot)
    python -m pc.enviar maps/simulados/ejemplo_5x5.txt --puerto COM6      # por cable (pip install pyserial)
    python -m pc.enviar maps/simulados/ejemplo_5x5.txt --costo-giro 0.8

Por Wi-Fi busca el carro solo: en la última IP donde lo encontró, en su red propia (192.168.4.1) o barriendo
la subred del hotspot (el hotspot del celular no deja pasar mDNS ni broadcast). --ip fija la dirección.
Compara la ruta del carro con la de pc/planners/bfs.py: si no coinciden, termina con error.
"""

import argparse
import ipaddress
import pathlib
import socket
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor

from pc import mapa as mapas
from pc.modelo import COSTO_GIRO
from pc.planners import REGISTRO, ejecutar

IP_RED_PROPIA = "192.168.4.1"
PUERTO_TCP = 23
ULTIMA_IP = pathlib.Path.home() / ".agente-laberinto-ip"


def _ips_locales():
    """IPv4 privadas de este PC (la de la ruta por defecto y, en el Mac, las de las interfaces Wi-Fi)."""
    ips = set()
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        try:
            s.connect(("10.255.255.255", 1))   # no manda nada: solo elige la interfaz
            ips.add(s.getsockname()[0])
        except OSError:
            pass
    try:
        ips.update(socket.gethostbyname_ex(socket.gethostname())[2])
    except OSError:
        pass
    if sys.platform == "darwin":
        for iface in ("en0", "en1"):
            r = subprocess.run(["ipconfig", "getifaddr", iface], capture_output=True, text=True)
            if r.stdout.strip():
                ips.add(r.stdout.strip())
    return [ip for ip in ips if ipaddress.ip_address(ip).is_private and not ip.startswith("127.")]


def _responde(ip, puerto, timeout):
    try:
        socket.create_connection((ip, puerto), timeout=timeout).close()
        return ip
    except OSError:
        return None


def barrer_subredes(puerto=PUERTO_TCP):
    """Busca el puerto del carro en las /24 de este PC. Devuelve la IP o None."""
    propias = set(_ips_locales())
    candidatas = [str(h) for ip in propias for h in ipaddress.ip_network(f"{ip}/24", strict=False).hosts()
                  if str(h) not in propias]
    with ThreadPoolExecutor(max_workers=128) as ex:
        for ip in ex.map(lambda h: _responde(h, puerto, 0.8), candidatas):
            if ip:
                return ip
    return None


def conectar_tcp(ip=None, puerto=PUERTO_TCP, timeout=5):
    """Abre el socket con el carro: a `ip`, o a la última IP conocida, a su red propia o barriendo el hotspot."""
    if ip:
        return socket.create_connection((ip, puerto), timeout=timeout)
    conocidas = [ULTIMA_IP.read_text().strip()] if ULTIMA_IP.exists() else []
    for candidata in conocidas + [IP_RED_PROPIA]:
        try:
            return socket.create_connection((candidata, puerto), timeout=1.5)
        except OSError:
            pass
    print("(buscando el carro en el hotspot...)", file=sys.stderr)
    ip = barrer_subredes(puerto)
    if not ip:
        raise ConnectionError(f"no encontré el carro: ni en su red ({IP_RED_PROPIA}) ni en el hotspot")
    print(f"(carro en {ip})", file=sys.stderr)
    ULTIMA_IP.write_text(ip)
    return socket.create_connection((ip, puerto), timeout=timeout)


class ConexionTcp:
    def __init__(self, ip, puerto, timeout):
        self.sock = conectar_tcp(ip, puerto, timeout)
        self.sock.settimeout(timeout)
        self.archivo = self.sock.makefile("rwb")

    def enviar(self, linea):
        self.archivo.write(linea.encode() + b"\n")
        self.archivo.flush()

    def recibir(self):
        linea = self.archivo.readline()
        if not linea:
            raise ConnectionError("el carro cerró la conexión")
        return linea.decode(errors="replace").strip()

    def cerrar(self):
        self.sock.close()


class ConexionSerial:
    def __init__(self, puerto, timeout):
        try:
            import serial
        except ImportError:
            sys.exit("para mandar por cable hace falta pyserial: pip install pyserial")
        self.ser = serial.Serial(puerto, 115200, timeout=timeout)
        time.sleep(2)  # al abrir el puerto la placa se reinicia
        self.ser.reset_input_buffer()

    def enviar(self, linea):
        self.ser.write(linea.encode() + b"\n")

    def recibir(self):
        linea = self.ser.readline()
        if not linea:
            raise TimeoutError("el carro no respondió")
        return linea.decode(errors="replace").strip()

    def cerrar(self):
        self.ser.close()


def esperar(con, *prefijos):
    """Lee líneas hasta una que empiece con alguno de los prefijos. Imprime el log (#)."""
    while True:
        linea = con.recibir()
        if linea.startswith("#"):
            print(linea)
        if linea.startswith(prefijos):
            return linea


def main(argv=None):
    p = argparse.ArgumentParser(description="Manda un mapa al carro y verifica la ruta que calcula.")
    p.add_argument("mapa")
    p.add_argument("--ip", help="dirección del carro (por defecto: su red propia o el hotspot)")
    p.add_argument("--puerto-tcp", type=int, default=PUERTO_TCP, help=argparse.SUPPRESS)
    p.add_argument("--puerto", help="puerto serie (COM6, /dev/cu.usbserial-...) en vez de Wi-Fi")
    p.add_argument("--costo-giro", type=float, default=COSTO_GIRO)
    p.add_argument("--timeout", type=float, default=5.0)
    args = p.parse_args(argv)

    m = mapas.leer(args.mapa)
    esperada = ejecutar(REGISTRO["bfs"](args.costo_giro), m)

    con = ConexionSerial(args.puerto, args.timeout) if args.puerto else ConexionTcp(args.ip, args.puerto_tcp, args.timeout)
    try:
        con.enviar(f"COSTO_GIRO {args.costo_giro:g}")
        # COSTO_GIRO vuelve a planear el mapa viejo; INFO sirve de barrera para descartar esa respuesta.
        con.enviar("INFO")
        esperar(con, "OK mapa=")
        con.enviar("MAPA")
        for linea in mapas.a_texto(m).splitlines():
            con.enviar(linea)
        con.enviar("FIN")
        resp = esperar(con, "OK MAPA", "ERR")
        print(resp)
        if resp.startswith("ERR"):
            return 1
        resp = esperar(con, "RUTA", "ERR")
        print(resp)
        if resp.startswith("ERR"):
            return 1
    finally:
        con.cerrar()

    ruta_carro = resp.removeprefix("RUTA").strip()
    if esperada is None or ruta_carro != esperada.ruta:
        print(f"ERROR: la ruta del carro no coincide con la de Python:\n  python: {esperada.ruta if esperada else 'sin ruta'}")
        return 1
    print("OK: el carro calculó la misma ruta que Python")
    return 0


if __name__ == "__main__":
    sys.exit(main())
