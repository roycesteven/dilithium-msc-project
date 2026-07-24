CC ?= /usr/bin/cc
CFLAGS += -Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls \
  -Wshadow -Wvla -Wpointer-arith -O3 -fomit-frame-pointer
NISTFLAGS += -Wno-unused-result -O3 -fomit-frame-pointer
SOURCES = sign.c packing.c polyvec.c poly.c ntt.c reduce.c rounding.c
HEADERS = config.h params.h api.h sign.h packing.h polyvec.h poly.h ntt.h \
  reduce.h rounding.h symmetric.h randombytes.h
KECCAK_SOURCES = $(SOURCES) fips202.c symmetric-shake.c
KECCAK_HEADERS = $(HEADERS) fips202.h

# Optional reproducibility flags injected by scripts/run_fair_benchmarks.sh, e.g.
#   make test/bench_levels3 REPRO_FLAGS='-DLAS_GIT_COMMIT=\"abc1234\" -DLAS_GIT_BRANCH=\"main\"'
# A plain `make` leaves them empty (bench_levels then prints git=n/a). Metadata only;
# it does not affect any measurement.
REPRO_FLAGS ?=

.PHONY: all speed shared clean


#   test/test_dilithium2 \
#   test/test_dilithium3 \
#   test/test_dilithium5 \
#   test/test_vectors2 \
#   test/test_vectors3 \
#   test/test_vectors5 \


# test/test_swap3 and test/test_zkp3 are NOT in `all`: they need the vendored
# LaZer library (one-time build, see the pi section below and README) -- the
# same opt-in pattern as test/bench_classical (secp256k1-zkp).
all: \
  test/test_las2 \
  test/test_las3 \
  test/test_las5 \
  test/bench_levels_paper \
  test/bench_levels2 \
  test/bench_levels3 \
  test/bench_levels5 \
  test/bench_criterion3 \
  test/test_basesig_paper \
  test/test_basesig2 \
  test/test_basesig3 \
  test/test_basesig5 \
  test/bench_app3 \
  test/test_pcn3 \
  test/test_amhl3 \
  test/test_serde3 \
  test/test_serde_l2 \
  test/test_serde_l3 \
  test/test_serde_l5 \
  test/test_kat3 \
  test/test_contract3

nistkat: \
  nistkat/PQCgenKAT_sign2 \
  nistkat/PQCgenKAT_sign3 \
  nistkat/PQCgenKAT_sign5

speed: \
  test/test_mul \
  test/test_speed2 \
  test/test_speed3 \
  test/test_speed5 \

shared: \
  libpqcrystals_dilithium2_ref.so \
  libpqcrystals_dilithium3_ref.so \
  libpqcrystals_dilithium5_ref.so \
  libpqcrystals_fips202_ref.so \

libpqcrystals_fips202_ref.so: fips202.c fips202.h
	$(CC) -shared -fPIC $(CFLAGS) -o $@ $<

libpqcrystals_dilithium2_ref.so: $(SOURCES) $(HEADERS) symmetric-shake.c
	$(CC) -shared -fPIC $(CFLAGS) -DDILITHIUM_MODE=2 \
	  -o $@ $(SOURCES) symmetric-shake.c

libpqcrystals_dilithium3_ref.so: $(SOURCES) $(HEADERS) symmetric-shake.c
	$(CC) -shared -fPIC $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $(SOURCES) symmetric-shake.c

libpqcrystals_dilithium5_ref.so: $(SOURCES) $(HEADERS) symmetric-shake.c
	$(CC) -shared -fPIC $(CFLAGS) -DDILITHIUM_MODE=5 \
	  -o $@ $(SOURCES) symmetric-shake.c

test/test_dilithium2: test/test_dilithium.c randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=2 \
	  -o $@ $< randombytes.c $(KECCAK_SOURCES)

test/test_dilithium3: test/test_dilithium.c randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< randombytes.c $(KECCAK_SOURCES)

test/test_dilithium5: test/test_dilithium.c randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=5 \
	  -o $@ $< randombytes.c $(KECCAK_SOURCES)

test/test_las2: test/test_las.c basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h serialize.c serialize.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=2 -DLAS_N=4 -DELL=4 -DKAPPA=39 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c $(KECCAK_SOURCES)

test/test_las3: test/test_las.c basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h serialize.c serialize.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c $(KECCAK_SOURCES)

test/test_las5: test/test_las.c basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h serialize.c serialize.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=5 -DLAS_N=8 -DELL=7 -DKAPPA=60 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c $(KECCAK_SOURCES)

# ---- pi targets (Fig. 1 proof of knowledge, eprint 2020/845 Section 4.1) ----
# Need the vendored LaZer library (third_party/lazer, git-ignored) built once:
#   git clone https://github.com/lazer-crypto/lazer third_party/lazer
#   make lazer.h liblazer.a     (HEXL via cmake; MPFR/GMP headers required --
#                                see README "pi / atomic swap" for the exact
#                                no-sudo conda recipe used on this machine)
# ref/relation_zk_params.h is COMMITTED (generated from scripts/las_pi_params.py
# by LaZer's `sage lin-codegen.sage`), so sage is NOT needed to build these.
LAZER_DIR ?= $(abspath ../third_party/lazer)
LAZER_DEP_PREFIX ?= $(HOME)/micromamba/envs/lazer-build
LAZER_LIBS = $(LAZER_DIR)/liblazer.a \
  -L$(LAZER_DEP_PREFIX)/lib -Wl,-rpath,$(LAZER_DEP_PREFIX)/lib -lmpfr -lgmp -lm \
  $(LAZER_DIR)/third_party/hexl-development/build/hexl/lib64/libhexl.a -lstdc++

# The bridge is the ONLY TU that includes lazer.h; ref headers and lazer.h
# cannot share a TU, so it is compiled separately with -I$(LAZER_DIR) and no
# ref-specific flags (it is parameter-set independent: fixed D3 statement).
relation_zk_lazer.o: relation_zk_lazer.c relation_zk_lazer.h relation_zk_params.h
	$(CC) -Wall -Wextra -Wshadow -O3 -I$(LAZER_DIR) -c -o $@ relation_zk_lazer.c

test/test_zkp3: test/test_zkp.c relation.c relation.h relation_zk.c relation_zk.h \
  relation_zk_lazer.h relation_zk_lazer.o setup.c setup.h las_types.h randombytes.c \
  $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< relation.c relation_zk.c setup.c randombytes.c $(KECCAK_SOURCES) \
	  relation_zk_lazer.o $(LAZER_LIBS)

test/test_swap3: test/test_swap.c basesig.c basesig.h relation.c relation.h \
  relation_zk.c relation_zk.h relation_zk_lazer.h relation_zk_lazer.o las.c las.h \
  setup.c setup.h las_types.h serialize.c serialize.h randombytes.c \
  $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< basesig.c relation.c relation_zk.c las.c setup.c serialize.c \
	  randombytes.c $(KECCAK_SOURCES) relation_zk_lazer.o $(LAZER_LIBS)

# NOTE: bench_las.c and bench_compare.c were REMOVED -- their outputs
# (bench_las3 rejection rate; bench_compare3 LAS-vs-Dilithium-3) are not used as
# primary evidence.  Rejection sampling now lives in bench_levels' diagnostic
# section A; the original/optimised Dilithium head-to-head is deliberately kept
# out of the primary table.

# Primary algorithm-matched, parameter-matched adaptor-overhead benchmark:
# the SEPARATE simplified Dilithium-style base signature (basesig.c) vs the LAS
# adaptor operations (las.c), at matched parameters.  basesig.c and las.c are
# distinct translation units -- the base scheme does not touch the LAS protocol.
# Each target sets LAS_N/LAS_ELL/LAS_KAPPA explicitly for the paper,
# Level-2-aligned, Level-3-aligned, and Level-5-aligned parameter sets.
# DILITHIUM_MODE is used only to satisfy the Dilithium build system/params.h;
# original/optimised Dilithium timings are not part of this benchmark.
LEVELS_DEPS = test/bench_levels.c basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h serialize.c serialize.h randombytes.c \
  test/cpucycles.c test/cpucycles.h \
  $(KECCAK_SOURCES) $(KECCAK_HEADERS)
test/bench_levels_paper: $(LEVELS_DEPS)
	$(CC) $(CFLAGS) $(REPRO_FLAGS) -DDILITHIUM_MODE=3 -DLAS_N=4 -DELL=4 -DKAPPA=60 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c test/cpucycles.c $(KECCAK_SOURCES) -lm
test/bench_levels2: $(LEVELS_DEPS)
	$(CC) $(CFLAGS) $(REPRO_FLAGS) -DDILITHIUM_MODE=2 -DLAS_N=4 -DELL=4 -DKAPPA=39 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c test/cpucycles.c $(KECCAK_SOURCES) -lm
test/bench_levels3: $(LEVELS_DEPS)
	$(CC) $(CFLAGS) $(REPRO_FLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c test/cpucycles.c $(KECCAK_SOURCES) -lm
test/bench_levels5: $(LEVELS_DEPS)
	$(CC) $(CFLAGS) $(REPRO_FLAGS) -DDILITHIUM_MODE=5 -DLAS_N=8 -DELL=7 -DKAPPA=60 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c test/cpucycles.c $(KECCAK_SOURCES) -lm

# C mirror of the Rust Criterion.rs statistical benchmark (see
# rust/fips204-las/benches/las_bench.rs and test/bench_criterion.c's header):
# same warm-up / 300-sample linear-ramp / 60 s methodology, same rejection
# gates, same operation set.  Built only at the Simplified Dilithium-III set,
# pairing the Rust port's fixed (6,5,49) parameters.
CRITERION_DEPS = test/bench_criterion.c basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h serialize.c serialize.h randombytes.c \
  $(KECCAK_SOURCES) $(KECCAK_HEADERS)
test/bench_criterion3: $(CRITERION_DEPS)
	$(CC) $(CFLAGS) $(REPRO_FLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c $(KECCAK_SOURCES) -lm

# CHECK-gated randomised correctness test for the separate base signature (basesig.c)
# and its interlock with the LAS adaptor (las.c), per matched parameter set.
# This is test evidence over NTESTS iterations, not a formal/exhaustive proof.
BASESIG_TEST_DEPS = test/test_basesig.c basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h serialize.c serialize.h randombytes.c \
  $(KECCAK_SOURCES) $(KECCAK_HEADERS)
test/test_basesig_paper: $(BASESIG_TEST_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=4 -DELL=4 -DKAPPA=60 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c $(KECCAK_SOURCES)
test/test_basesig2: $(BASESIG_TEST_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=2 -DLAS_N=4 -DELL=4 -DKAPPA=39 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c $(KECCAK_SOURCES)
test/test_basesig3: $(BASESIG_TEST_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c $(KECCAK_SOURCES)
test/test_basesig5: $(BASESIG_TEST_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=5 -DLAS_N=8 -DELL=7 -DKAPPA=60 \
	  -o $@ $< basesig.c relation.c las.c setup.c serialize.c randombytes.c $(KECCAK_SOURCES)

test/bench_app3: test/bench_app.c amhl.c amhl.h serialize.h las.c las.h setup.c setup.h las_types.h serialize.c serialize.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< amhl.c las.c setup.c serialize.c randombytes.c $(KECCAK_SOURCES)

test/test_pcn3: test/test_pcn.c chain.c chain.h las.c las.h setup.c setup.h las_types.h serialize.c serialize.h randombytes.c \
  $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< chain.c las.c setup.c serialize.c randombytes.c $(KECCAK_SOURCES)

test/test_amhl3: test/test_amhl.c amhl.c amhl.h chain.c chain.h las.c las.h setup.c setup.h las_types.h serialize.c serialize.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< amhl.c chain.c las.c setup.c serialize.c randombytes.c $(KECCAK_SOURCES)

test/test_serde3: test/test_serde.c serialize.c serialize.h basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=4 -DELL=4 -DKAPPA=60 \
	  -o $@ $< serialize.c basesig.c relation.c las.c setup.c randombytes.c $(KECCAK_SOURCES)

# Serde across parameter sets (test_serde3 above covers the paper dims, 4,4,60):
# these sweep the Dilithium-2/3/5-aligned sets.  serialize.h now derives the z field
# width from the parameters (18 bits for paper/D2, 19 for D3/D5), so every set packs
# losslessly and round-trips.
SERDE_LEVELS_DEPS = test/test_serde.c serialize.c serialize.h basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
test/test_serde_l2: $(SERDE_LEVELS_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=2 -DLAS_N=4 -DELL=4 -DKAPPA=39 \
	  -o $@ $< serialize.c basesig.c relation.c las.c setup.c randombytes.c $(KECCAK_SOURCES)
test/test_serde_l3: $(SERDE_LEVELS_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< serialize.c basesig.c relation.c las.c setup.c randombytes.c $(KECCAK_SOURCES)
test/test_serde_l5: $(SERDE_LEVELS_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=5 -DLAS_N=8 -DELL=7 -DKAPPA=60 \
	  -o $@ $< serialize.c basesig.c relation.c las.c setup.c randombytes.c $(KECCAK_SOURCES)

test/test_kat3: test/test_kat.c serialize.c serialize.h basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< serialize.c basesig.c relation.c las.c setup.c randombytes.c $(KECCAK_SOURCES)

# Consolidated correctness-contract evaluation harness (itemised PASS/FAIL).
test/test_contract3: test/test_contract.c serialize.c serialize.h basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< serialize.c basesig.c relation.c las.c setup.c randombytes.c $(KECCAK_SOURCES)

# Exports a real packed LAS adapted signature for the EVM gas benchmark (evm/).
test/export_packed: test/export_packed.c serialize.c serialize.h basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< serialize.c basesig.c relation.c las.c setup.c randombytes.c $(KECCAK_SOURCES)

# Exports a COMPLETE golden verification vector (pp.a_prime, t, M, sig + isolated
# NTT/pointwise golden vectors) for the on-chain Solidity verifier's stage tests.
test/export_verify_vector: test/export_verify_vector.c serialize.c serialize.h basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h poly.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< serialize.c basesig.c relation.c las.c setup.c randombytes.c $(KECCAK_SOURCES)

# Exports a PURE DIGEST-FAULT vector (tampered c_tilde, trace consistent with the tampered
# challenge) for the optimistic Naysayer verifier test. Reuses pp_normal.bin/t.bin/msg.bin
# from export_verify_vector (run that first).
test/export_naysayer_vectors: test/export_naysayer_vectors.c serialize.c serialize.h basesig.c basesig.h relation.c relation.h las.c las.h setup.c setup.h las_types.h poly.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49 \
	  -o $@ $< serialize.c basesig.c relation.c las.c setup.c randombytes.c $(KECCAK_SOURCES)

# Classical adaptor baseline (objectives B2.ii). Requires the vendored library:
#   git clone --depth 1 https://github.com/BlockstreamResearch/secp256k1-zkp \
#       ../third_party/secp256k1-zkp
# Not part of `all` (needs the clone). Vendored TUs build with -w (their code);
# the harness itself builds with the full project CFLAGS.
SECP = ../third_party/secp256k1-zkp
test/bench_classical: test/bench_classical.c
	$(CC) -O3 -w -I$(SECP) -I$(SECP)/src -DENABLE_MODULE_ECDSA_ADAPTOR=1 \
	  -c $(SECP)/src/secp256k1.c -o $(SECP)/secp256k1.o
	$(CC) -O3 -w -I$(SECP) -I$(SECP)/src -c $(SECP)/src/precomputed_ecmult.c \
	  -o $(SECP)/precomputed_ecmult.o
	$(CC) -O3 -w -I$(SECP) -I$(SECP)/src -c $(SECP)/src/precomputed_ecmult_gen.c \
	  -o $(SECP)/precomputed_ecmult_gen.o
	$(CC) $(CFLAGS) -I$(SECP)/include -o $@ $< $(SECP)/secp256k1.o \
	  $(SECP)/precomputed_ecmult.o $(SECP)/precomputed_ecmult_gen.o -lm

test/test_vectors2: test/test_vectors.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=2 \
	  -o $@ $< $(KECCAK_SOURCES)

test/test_vectors3: test/test_vectors.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< $(KECCAK_SOURCES)

test/test_vectors5: test/test_vectors.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=5 \
	  -o $@ $< $(KECCAK_SOURCES)

test/test_speed2: test/test_speed.c test/speed_print.c test/speed_print.h \
  test/cpucycles.c test/cpucycles.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=2 \
	  -o $@ $< test/speed_print.c test/cpucycles.c randombytes.c \
	  $(KECCAK_SOURCES)

test/test_speed3: test/test_speed.c test/speed_print.c test/speed_print.h \
  test/cpucycles.c test/cpucycles.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< test/speed_print.c test/cpucycles.c randombytes.c \
	  $(KECCAK_SOURCES)

test/test_speed5: test/test_speed.c test/speed_print.c test/speed_print.h \
  test/cpucycles.c test/cpucycles.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=5 \
	  -o $@ $< test/speed_print.c test/cpucycles.c randombytes.c \
	  $(KECCAK_SOURCES)

test/test_mul: test/test_mul.c randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -UDBENCH -o $@ $< randombytes.c $(KECCAK_SOURCES)

nistkat/PQCgenKAT_sign2: nistkat/PQCgenKAT_sign.c nistkat/rng.c nistkat/rng.h $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(NISTFLAGS) -DDILITHIUM_MODE=2 \
	  -o $@ $< nistkat/rng.c $(KECCAK_SOURCES) $(LDFLAGS) -lcrypto

nistkat/PQCgenKAT_sign3: nistkat/PQCgenKAT_sign.c nistkat/rng.c nistkat/rng.h $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(NISTFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< nistkat/rng.c $(KECCAK_SOURCES) $(LDFLAGS) -lcrypto

nistkat/PQCgenKAT_sign5: nistkat/PQCgenKAT_sign.c nistkat/rng.c nistkat/rng.h $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(NISTFLAGS) -DDILITHIUM_MODE=5 \
	  -o $@ $< nistkat/rng.c $(KECCAK_SOURCES) $(LDFLAGS) -lcrypto

clean:
	rm -f *~ test/*~ *.gcno *.gcda *.lcov
	rm -f libpqcrystals_dilithium2_ref.so
	rm -f libpqcrystals_dilithium3_ref.so
	rm -f libpqcrystals_dilithium5_ref.so
	rm -f libpqcrystals_fips202_ref.so
	rm -f test/test_dilithium2
	rm -f test/test_dilithium3
	rm -f test/test_dilithium5
	rm -f test/test_las2
	rm -f test/test_las3
	rm -f test/test_las5
	rm -f test/test_swap3
	rm -f test/test_zkp3
	rm -f relation_zk_lazer.o
	rm -f test/bench_levels_paper
	rm -f test/bench_levels2
	rm -f test/bench_levels3
	rm -f test/bench_levels5
	rm -f test/bench_criterion3
	rm -f test/test_basesig_paper
	rm -f test/test_basesig2
	rm -f test/test_basesig3
	rm -f test/test_basesig5
	rm -f test/bench_app3
	rm -f test/test_pcn3
	rm -f test/test_amhl3
	rm -f test/test_serde3
	rm -f test/test_serde_l2
	rm -f test/test_serde_l3
	rm -f test/test_serde_l5
	rm -f test/test_kat3
	rm -f test/test_contract3
	rm -f test/test_vectors2
	rm -f test/test_vectors3
	rm -f test/test_vectors5
	rm -f test/test_speed2
	rm -f test/test_speed3
	rm -f test/test_speed5
	rm -f test/test_mul
	rm -f nistkat/PQCgenKAT_sign2
	rm -f nistkat/PQCgenKAT_sign3
	rm -f nistkat/PQCgenKAT_sign5
	rm -f test/export_packed
	rm -f test/export_verify_vector
	rm -f test/export_naysayer_vectors
	rm -f test/bench_classical
