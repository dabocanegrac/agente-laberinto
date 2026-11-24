LATEXMK = latexmk
TEXFLAGS = -pdf -interaction=nonstopmode -halt-on-error -outdir=build
MAIN = main.tex
OUT = informe_agente_laberinto.pdf

.PHONY: all pdf clean distclean check

all: pdf

pdf:
	mkdir -p build
	$(LATEXMK) $(TEXFLAGS) $(MAIN)
	cp build/main.pdf $(OUT)

check: pdf
	@if grep -E "Undefined control sequence|LaTeX Error|Reference .* undefined|Citation .* undefined" build/main.log; then \
		echo "ERROR: se detectaron errores o referencias/citas indefinidas en build/main.log"; \
		exit 1; \
	else \
		echo "OK: compilacion limpia sin errores ni referencias/citas indefinidas."; \
	fi

clean:
	rm -rf build

distclean: clean
	rm -f $(OUT)
