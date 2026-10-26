"""Consola para hablar con el carro por Wi-Fi (red agente-laberinto): calibrar, GO, STOP.

    python -m pc.consola
    > SENS
    > PASO F
    > SET t_celda 750
    > GO

Lo que escriba se manda tal cual; lo que responda el carro sale en pantalla. Una línea vacía manda STOP.
Salir con Ctrl+C (manda STOP antes de cerrar). Comandos: ver arduino/agente/agente.ino.
"""

import argparse
import socket
import sys
import threading

from pc.enviar import IP_CARRO, PUERTO_TCP


def escuchar(sock):
    archivo = sock.makefile("rb")
    for linea in archivo:
        print(linea.decode(errors="replace").rstrip())
    print("(el carro cerró la conexión)")


def main(argv=None):
    p = argparse.ArgumentParser(description="Consola TCP del carro.")
    p.add_argument("--ip", default=IP_CARRO)
    args = p.parse_args(argv)

    sock = socket.create_connection((args.ip, PUERTO_TCP), timeout=5)
    sock.settimeout(None)
    threading.Thread(target=escuchar, args=(sock,), daemon=True).start()
    try:
        while True:
            linea = input().strip() or "STOP"
            sock.sendall(linea.encode() + b"\n")
    except (KeyboardInterrupt, EOFError):
        sock.sendall(b"STOP\n")
    finally:
        sock.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
