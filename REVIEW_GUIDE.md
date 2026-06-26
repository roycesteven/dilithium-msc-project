Core implementation files
Original Dilithium files kept intact
ref/sign.c
ref/sign.h

These remain the original Dilithium signing API. They are kept so the baseline signing implementation is still reviewable.

LAS implementation added
ref/las.c
ref/las.h

These implement the LAS adaptor-signature construction:
PreSign, PreVerify, Adapt, and Extract.

Fair benchmark baseline
ref/basesig.c
ref/basesig.h

These implement a simplified Dilithium-style base signature used only for fair benchmarking against LAS. This avoids comparing LAS against the fully optimised production Dilithium signing path.

Serialization
ref/serialize.c
ref/serialize.h

These define packed byte encodings for LAS public keys, statements, signatures, pre-signatures, witnesses, and secret keys.

Tests and benchmarks
ref/test/test_las.c: LAS correctness tests.
ref/test/test_basesig.c: base signature correctness tests.
ref/test/test_contract.c: labelled adaptor correctness contract.
ref/test/test_serde.c: serialization tests.
ref/test/test_swap.c: atomic-swap demonstration.
ref/test/test_pcn.c: payment-channel demonstration.
ref/test/bench_levels.c: primary fair benchmark across parameter levels.
ref/test/bench_app.c: application-level benchmark.
Reproducibility scripts
scripts/run_benchmark_suite.sh: one-command benchmark/evidence pipeline.
scripts/plot_las_benchmarks.py: parses logs and generates CSV/figures.
Useful local comparison commands

Show that original Dilithium signing files are unchanged from the baseline:

git diff dilithium-baseline..main -- ref/sign.c ref/sign.h

Compare original Dilithium signing code against LAS code:

git diff --no-index ref/sign.c ref/las.c || true
git diff --no-index ref/sign.h ref/las.h || true

Show only LAS-related files added on top of the baseline:

git diff --name-status dilithium-baseline..main -- \
  ref/las.c ref/las.h \
  ref/basesig.c ref/basesig.h \
  ref/serialize.c ref/serialize.h \
  ref/test/bench_levels.c
Summary

The project does not replace Dilithium's original signing API. Instead, it adds LAS as a separate adaptor-signature implementation on top of the Dilithium reference primitives, then evaluates it using a simplified base-signature comparator, correctness tests, and reproducible benchmarks.
EOF