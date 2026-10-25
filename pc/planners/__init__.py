"""Importa todos los módulos de esta carpeta para que sus @registrar llenen REGISTRO."""

import importlib
import pkgutil

from pc.planners.base import REGISTRO, Planner, Resultado, ejecutar, registrar

for _m in pkgutil.iter_modules(__path__):
    if _m.name != "base":
        importlib.import_module(f"{__name__}.{_m.name}")

__all__ = ["REGISTRO", "Planner", "Resultado", "ejecutar", "registrar"]
