"""Prueba pc/enviar.py contra un carro simulado que habla el mismo protocolo que arduino/agente/agente.ino."""

import socket
import threading
from pathlib import Path

from pc import enviar
from pc import mapa as mapas
from pc.planners import REGISTRO, ejecutar

RAIZ = Path(__file__).resolve().parents[2]
EJEMPLO = RAIZ / "maps/simulados/ejemplo_5x5.txt"


def carro_simulado(servidor, tmp_path, ruta_falsa=None):
    con, _ = servidor.accept()
    f = con.makefile("rwb")
    escribir = lambda t: (f.write(t.encode() + b"\n"), f.flush())
    escribir("# agente-laberinto (simulado)")
    costo_giro, lineas, recibiendo = 0.5, [], False
    for raw in f:
        l = raw.decode().strip()
        if recibiendo:
            if l == "FIN":
                p = tmp_path / "recibido.txt"
                p.write_text("\n".join(lineas), encoding="utf-8")
                m = mapas.leer(p)
                escribir(f"OK MAPA {m.filas}x{m.cols}")
                res = ejecutar(REGISTRO["bfs"](costo_giro), m)
                escribir("RUTA " + (ruta_falsa or res.ruta))
                break
            lineas.append(l)
        elif l.startswith("COSTO_GIRO"):
            costo_giro = float(l.split()[1])
            escribir(f"OK costo_giro={costo_giro:g}")
            escribir("ERR el mapa no tiene ruta de inicio a meta")  # respuesta vieja que hay que descartar
        elif l == "INFO":
            escribir("OK mapa=ninguno ruta=0")
        elif l == "MAPA":
            recibiendo = True
    con.close()


def correr(tmp_path, *extra, ruta_falsa=None):
    servidor = socket.create_server(("127.0.0.1", 0))
    puerto = servidor.getsockname()[1]
    hilo = threading.Thread(target=carro_simulado, args=(servidor, tmp_path, ruta_falsa), daemon=True)
    hilo.start()
    try:
        return enviar.main([str(EJEMPLO), "--ip", "127.0.0.1", "--puerto-tcp", str(puerto), *extra])
    finally:
        hilo.join(5)
        servidor.close()


def test_envia_mapa_y_verifica_ruta(tmp_path, capsys):
    assert correr(tmp_path, "--costo-giro", "0.8") == 0
    assert "misma ruta que Python" in capsys.readouterr().out


def test_detecta_ruta_distinta(tmp_path, capsys):
    assert correr(tmp_path, ruta_falsa="F F F") == 1
    assert "no coincide" in capsys.readouterr().out
