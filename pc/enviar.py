"""Manda un mapa al carro. El carro calcula la ruta a bordo y la devuelve.

    python -m pc.enviar maps/simulados/ejemplo_5x5.txt                    # por Wi-Fi (red agente-laberinto)
    python -m pc.enviar maps/simulados/ejemplo_5x5.txt --puerto COM6      # por cable (pip install pyserial)
    python -m pc.enviar maps/simulados/ejemplo_5x5.txt --costo-giro 0.8

Compara la ruta del carro con la de pc/planners/bfs.py: si no coinciden, termina con error.
"""

import argparse
import socket
import sys
import time

from pc import mapa as mapas
from pc.modelo import COSTO_GIRO
from pc.planners import REGISTRO, ejecutar

IP_CARRO = "192.168.4.1"
PUERTO_TCP = 23


class ConexionTcp:
    def __init__(self, ip, puerto, timeout):
        self.sock = socket.create_connection((ip, puerto), timeout=timeout)
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
    p.add_argument("--ip", default=IP_CARRO)
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
