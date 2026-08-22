# Deck assets

Images embedded into `video_deck.html` by `scripts/gen_slides.py` (as base64
data URIs, so the deck stays a single self-contained file).

## `uom_logo.png` — the University of Manchester mark

**Provenance.** Taken from the university's own template,
`report/slides/Master_169 presentation(2).pptx`, where the mark is a *slide
master* shape (which is why it repeats on every slide). In that file it is
stored as `ppt/media/image1.emf` — a vector metafile, so it cannot be embedded
in HTML directly. It was therefore rasterised from the template's own rendered
PDF, `Master_169 presentation(2).pdf`, at the exact position and size the master
places it:

    off  x=523875  y=509588   ext cx=1663700 cy=711200   (EMU)
      => x=0.5729" y=0.5573"      1.8194" x 0.7777"

Rebuild it with:

    pdftocairo -png -r 800 -x 458 -y 446 -W 1456 -H 622 -singlefile \
        "Master_169 presentation(2).pdf" uom_raw

then flood-fill the surrounding page to transparent (a *border-seeded* flood,
never a global white key — the wordmark's own letters are white on purple and a
global key would punch holes through them), resize to 700x299, and quantise to
96 colours. 16 KB.

**Do not redraw it.** The mark is a trademarked asset; it is reproduced here,
not reconstructed. It is displayed at the template's aspect ratio.
