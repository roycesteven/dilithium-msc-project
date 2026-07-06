# LAS — A Lattice-Based Adaptor Signature on Dilithium Primitives

*Design, implementation, correctness, testing, application and benchmarks.*

This document is the technical reference for the implementation in `ref/las.{c,h}`,
`ref/amhl.{c,h}`, `ref/chain.{c,h}`, `ref/serialize.{c,h}`, and the
tests/benchmarks under `ref/test/` (`test_las.c`, `test_swap.c`, `test_pcn.c`,
`test_amhl.c`, `test_serde.c`, `test_kat.c`, `bench_las.c`, `bench_compare.c`,
`bench_app.c`). It is written to be the source material for the dissertation
chapter; section numbering maps roughly onto report sections.

---

**This file is now the index (hub).** On 2026-07-06 the body was split into
per-chapter part files (content verbatim, section numbering preserved) so the
`docs/` tree mirrors the report structure (`report/latex/report.tex`). Any
existing reference of the form "`LAS.md §N`" resolves via this table:

| § | Section | File (report-chapter folder) |
| --- | --- | --- |
| §1 | Introduction and motivation | `docs/01-introduction/LAS-01-introduction-and-motivation.md` |
| §2 | Mathematical background | `docs/02-methodology/LAS-02-mathematical-background.md` |
| §3 | The base signature | `docs/02-methodology/LAS-03-base-signature.md` |
| §4 | The adaptor extension (LAS, variant B) | `docs/02-methodology/LAS-04-adaptor-extension.md` |
| §5 | Implementation | `docs/02-methodology/LAS-05-implementation.md` |
| §6 | Testing | `docs/02-methodology/LAS-06-testing.md` |
| §7 | Application: post-quantum atomic swap | `docs/02-methodology/LAS-07-application-atomic-swap.md` |
| §8 | Performance (measured) | `docs/03-results/LAS-08-performance-measured.md` |
| §9 | Limitations and future work | `docs/04-evaluation/LAS-09-limitations-future-work.md` |
| §10 | Build and run | `docs/A-appendix/LAS-10-build-and-run.md` |
| §11 | References | below, in this file |

Full chapter map of every document: `docs/DOCS_BY_CHAPTER.md`.

---

## 11. References
1. M. F. Esgin, O. Ersoy, Z. Erkin. *Post-Quantum Adaptor Signatures and Payment
   Channel Networks*. ESORICS 2020 / IACR eprint 2020/845.
2. L. Ducas et al. *CRYSTALS-Dilithium* (ML-DSA / FIPS 204). Reference C
   implementation reused here.
3. A. Erwig et al. / poqeth. *Integration template for PQ scriptless scripts*. IACR
   eprint 2025/091.
4. M. Ajtai. *Generating hard instances of lattice problems*. STOC 1996 (`f_A`).
