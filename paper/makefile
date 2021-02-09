all: clean mfog-ieee.pdf

clean:
	-rm mfog-ieee.pdf

mfog-ieee.pdf:
	latexmk -pdf mfog-ieee.tex

figures/%.svg.pdf: figures/%.svg
	inkscape -D $? -o $@ --export-latex
