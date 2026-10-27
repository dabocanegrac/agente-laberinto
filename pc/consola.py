"""Consola para hablar con el carro por Wi-Fi (red agente-laberinto) o por cable: calibrar, GO, STOP.

    python -m pc.consola
    python -m pc.consola --puerto /dev/cu.wchusbserial...      # por cable (pip install pyserial)
    > SENS
    > PASO F
    > SET t_celda 750
    > GO

Lo que escriba se manda tal cual; lo que responda el carro sale en pantalla. Una línea vacía manda STOP.
Salir con Ctrl+C (manda STOP antes de cerrar). Comandos: ver arduino/agente/agente.ino.
"""

import argparse
import sys
import threading

from pc.enviar import ConexionSerial, conectar_tcp


def escuchar(leer_linea):
    try:
        while True:
            linea = leer_linea()
            if linea is None:
                break
            if linea:
                print(linea.decode(errors="replace").rstrip())
    except OSError:
        pass
    print("(el carro cerró la conexión)")


def main(argv=None):
    p = argparse.ArgumentParser(description="Consola del carro (TCP o cable).")
    p.add_argument("--ip", help="dirección del carro (por defecto: su red propia o el hotspot)")
    p.add_argument("--puerto", help="puerto serie (COM6, /dev/cu.wchusbserial...) en vez de Wi-Fi")
    args = p.parse_args(argv)

    if args.puerto:
        con = ConexionSerial(args.puerto, timeout=0.2)
        escribir, cerrar = con.ser.write, con.cerrar
        leer_linea = con.ser.readline   # b"" en timeout: se ignora
    else:
        sock = conectar_tcp(args.ip)
        sock.settimeout(None)
        archivo = sock.makefile("rb")
        escribir, cerrar = sock.sendall, sock.close
        leer_linea = lambda: archivo.readline() or None
    threading.Thread(target=escuchar, args=(leer_linea,), daemon=True).start()
    try:
        while True:
            linea = input().strip() or "STOP"
            escribir(linea.encode() + b"\n")
    except (KeyboardInterrupt, EOFError):
        escribir(b"STOP\n")
    finally:
        cerrar()
    return 0


if __name__ == "__main__":
    sys.exit(main())
