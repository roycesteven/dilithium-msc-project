<!-- Part of docs/LAS.md, split by report chapter (2026-07-06). Index: docs/LAS.md.
     Section numbering is preserved verbatim, so external references like
     "LAS.md §10" resolve to this file. Do not renumber sections. -->

## 10. Build and run
```
cd ref
make test/test_las3   && ./test/test_las3     # functional tests
make test/test_zkp3   && ./test/test_zkp3     # pi proof of knowledge (opt-in: needs vendored LaZer, see README)
make test/test_swap3  && ./test/test_swap3    # Fig. 1 atomic swap incl. pi (opt-in: needs vendored LaZer)
make test/test_serde3 && ./test/test_serde3   # serialisation: round-trip / verify-from-bytes / tamper
make test/test_kat3   && ./test/test_kat3     # deterministic known-answer test (reproducibility)
make test/bench_las3  && ./test/bench_las3    # per-operation timings + direct rejection rate
make test/bench_compare3 && ./test/bench_compare3  # LAS vs optimised Dilithium-3
```
All are mode-independent; `-DDILITHIUM_MODE=2/5` behave identically.

