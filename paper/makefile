all: clean mfog-ieee.pdf

clean:
	-rm mfog-ieee.pdf

mfog-ieee.pdf:
	latexmk -pdf mfog-ieee.tex

figures/mfog-arch-fisica.svg.pdf: figures/mfog-arch-fisica.svg
	inkscape -D figures/mfog-arch-fisica.svg -o figures/mfog-arch-fisica.svg.pdf --export-latex