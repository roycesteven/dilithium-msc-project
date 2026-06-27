# Build the MSc dissertation PDF.
#   make            -> report.pdf (latexmk if available, else manual pdflatex+bibtex)
#   make wordcount  -> word.count (requires texcount) then a wordcount build
#   make clean      -> remove build artefacts
#   make distclean  -> also remove the PDF

MAIN = report

.PHONY: all clean distclean wordcount

all: $(MAIN).pdf

ifeq ($(shell command -v latexmk 2>/dev/null),)
# ---- fallback: explicit pdflatex + bibtex sequence ----
$(MAIN).pdf: $(MAIN).tex refs.bib chapters/*.tex muthesis.cls
	pdflatex -interaction=nonstopmode -halt-on-error $(MAIN).tex
	-bibtex $(MAIN)
	pdflatex -interaction=nonstopmode -halt-on-error $(MAIN).tex
	pdflatex -interaction=nonstopmode -halt-on-error $(MAIN).tex
else
# ---- preferred: latexmk drives the whole build ----
$(MAIN).pdf: $(MAIN).tex refs.bib chapters/*.tex muthesis.cls
	latexmk -pdf -interaction=nonstopmode -halt-on-error $(MAIN).tex
endif

# Word count (muthesis `wordcount` option reads word.count). Do not run from
# inside LaTeX; the texcount output may need manual editing.
wordcount:
	@if command -v texcount >/dev/null 2>&1; then \
		texcount -1 -sum -relaxed -inc report.tex > word.count; \
	elif [ -f texcount.pl ]; then \
		perl texcount.pl -1 -sum -relaxed -inc report.tex > word.count; \
	else \
		echo "ERROR: texcount not installed."; \
		exit 127; \
	fi
	@echo "word.count written; enable the 'wordcount' class option to print it."
	@cat word.count

clean:
	rm -f *.aux *.bbl *.blg *.log *.lof *.lot *.toc *.out *.fls \
	      *.fdb_latexmk chapters/*.aux

distclean: clean
	rm -f $(MAIN).pdf word.count
