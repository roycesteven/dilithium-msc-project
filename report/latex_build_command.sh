From report/latex/ (the directory with report.tex and the Makefile):


cd report/latex
make
That runs latexmk if available, otherwise falls back to the manual sequence (pdflatex → bibtex → pdflatex → pdflatex) and produces report.pdf.

Other targets in that Makefile:

make wordcount — runs texcount then a wordcount build
make clean — remove build artefacts (.aux, .log, etc.)
make distclean — also remove report.pdf
If you'd rather run it by hand without make:


cd report/latex
pdflatex -interaction=nonstopmode -halt-on-error report.tex
bibtex report
pdflatex -interaction=nonstopmode -halt-on-error report.tex
pdflatex -interaction=nonstopmode -halt-on-error report.tex
The three pdflatex passes (with bibtex in between) are needed so the \cref cross-references to the new tab:notation and tab:complete-l3 and the bibliography all resolve.


# To rebuild (yours to run)

python3 /home/melly/dilithium-msc-project/scripts/make_report_tables.py    # regenerate table images if CSVs change
cd /home/melly/dilithium-msc-project/report/latex && make                  # the final PDF — your call
Note: your repo has stale build artifacts (report.aux, empty report.bbl, etc.) from the 18:59 broken-bib build; make (or make clean first) will refresh them.