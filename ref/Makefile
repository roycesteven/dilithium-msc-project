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


all: \
  test/test_las2 \
  test/test_las3 \
  test/test_las5 \
  test/test_swap3 \
  test/bench_levels_paper \
  test/bench_levels2 \
  test/bench_levels3 \
  test/bench_levels5 \
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

test/test_las2: test/test_las.c las.c las.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=2 -DLAS_N=4 -DLAS_ELL=4 -DLAS_KAPPA=39 \
	  -o $@ $< las.c randombytes.c $(KECCAK_SOURCES)

test/test_las3: test/test_las.c las.c las.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 \
	  -o $@ $< las.c randombytes.c $(KECCAK_SOURCES)

test/test_las5: test/test_las.c las.c las.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=5 -DLAS_N=8 -DLAS_ELL=7 -DLAS_KAPPA=60 \
	  -o $@ $< las.c randombytes.c $(KECCAK_SOURCES)

test/test_swap3: test/test_swap.c las.c las.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 \
	  -o $@ $< las.c randombytes.c $(KECCAK_SOURCES)

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
LEVELS_DEPS = test/bench_levels.c basesig.c basesig.h las.c las.h randombytes.c \
  $(KECCAK_SOURCES) $(KECCAK_HEADERS)
test/bench_levels_paper: $(LEVELS_DEPS)
	$(CC) $(CFLAGS) $(REPRO_FLAGS) -DDILITHIUM_MODE=3 -DLAS_N=4 -DLAS_ELL=4 -DLAS_KAPPA=60 \
	  -o $@ $< basesig.c las.c randombytes.c $(KECCAK_SOURCES) -lm
test/bench_levels2: $(LEVELS_DEPS)
	$(CC) $(CFLAGS) $(REPRO_FLAGS) -DDILITHIUM_MODE=2 -DLAS_N=4 -DLAS_ELL=4 -DLAS_KAPPA=39 \
	  -o $@ $< basesig.c las.c randombytes.c $(KECCAK_SOURCES) -lm
test/bench_levels3: $(LEVELS_DEPS)
	$(CC) $(CFLAGS) $(REPRO_FLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 \
	  -o $@ $< basesig.c las.c randombytes.c $(KECCAK_SOURCES) -lm
test/bench_levels5: $(LEVELS_DEPS)
	$(CC) $(CFLAGS) $(REPRO_FLAGS) -DDILITHIUM_MODE=5 -DLAS_N=8 -DLAS_ELL=7 -DLAS_KAPPA=60 \
	  -o $@ $< basesig.c las.c randombytes.c $(KECCAK_SOURCES) -lm

# CHECK-gated randomised correctness test for the separate base signature (basesig.c)
# and its interlock with the LAS adaptor (las.c), per matched parameter set.
# This is test evidence over NTESTS iterations, not a formal/exhaustive proof.
BASESIG_TEST_DEPS = test/test_basesig.c basesig.c basesig.h las.c las.h randombytes.c \
  $(KECCAK_SOURCES) $(KECCAK_HEADERS)
test/test_basesig_paper: $(BASESIG_TEST_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=4 -DLAS_ELL=4 -DLAS_KAPPA=60 \
	  -o $@ $< basesig.c las.c randombytes.c $(KECCAK_SOURCES)
test/test_basesig2: $(BASESIG_TEST_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=2 -DLAS_N=4 -DLAS_ELL=4 -DLAS_KAPPA=39 \
	  -o $@ $< basesig.c las.c randombytes.c $(KECCAK_SOURCES)
test/test_basesig3: $(BASESIG_TEST_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 \
	  -o $@ $< basesig.c las.c randombytes.c $(KECCAK_SOURCES)
test/test_basesig5: $(BASESIG_TEST_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=5 -DLAS_N=8 -DLAS_ELL=7 -DLAS_KAPPA=60 \
	  -o $@ $< basesig.c las.c randombytes.c $(KECCAK_SOURCES)

test/bench_app3: test/bench_app.c amhl.c amhl.h serialize.h las.c las.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 \
	  -o $@ $< amhl.c las.c randombytes.c $(KECCAK_SOURCES)

test/test_pcn3: test/test_pcn.c chain.c chain.h las.c las.h randombytes.c \
  $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 \
	  -o $@ $< chain.c las.c randombytes.c $(KECCAK_SOURCES)

test/test_amhl3: test/test_amhl.c amhl.c amhl.h chain.c chain.h las.c las.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 \
	  -o $@ $< amhl.c chain.c las.c randombytes.c $(KECCAK_SOURCES)

test/test_serde3: test/test_serde.c serialize.c serialize.h las.c las.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=4 -DLAS_ELL=4 -DLAS_KAPPA=60 \
	  -o $@ $< serialize.c las.c randombytes.c $(KECCAK_SOURCES)

# Serde across parameter sets (test_serde3 above covers the paper dims, 4,4,60):
# these sweep the Dilithium-2/3/5-aligned sets.  serialize.h now derives the z field
# width from the parameters (18 bits for paper/D2, 19 for D3/D5), so every set packs
# losslessly and round-trips.
SERDE_LEVELS_DEPS = test/test_serde.c serialize.c serialize.h las.c las.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
test/test_serde_l2: $(SERDE_LEVELS_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=2 -DLAS_N=4 -DLAS_ELL=4 -DLAS_KAPPA=39 \
	  -o $@ $< serialize.c las.c randombytes.c $(KECCAK_SOURCES)
test/test_serde_l3: $(SERDE_LEVELS_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 \
	  -o $@ $< serialize.c las.c randombytes.c $(KECCAK_SOURCES)
test/test_serde_l5: $(SERDE_LEVELS_DEPS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=5 -DLAS_N=8 -DLAS_ELL=7 -DLAS_KAPPA=60 \
	  -o $@ $< serialize.c las.c randombytes.c $(KECCAK_SOURCES)

test/test_kat3: test/test_kat.c serialize.c serialize.h las.c las.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 \
	  -o $@ $< serialize.c las.c randombytes.c $(KECCAK_SOURCES)

# Consolidated correctness-contract evaluation harness (itemised PASS/FAIL).
test/test_contract3: test/test_contract.c serialize.c serialize.h las.c las.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 \
	  -o $@ $< serialize.c las.c randombytes.c $(KECCAK_SOURCES)

# Exports a real packed LAS adapted signature for the EVM gas benchmark (evm/).
test/export_packed: test/export_packed.c serialize.c serialize.h las.c las.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 -DLAS_N=6 -DLAS_ELL=5 -DLAS_KAPPA=49 \
	  -o $@ $< serialize.c las.c randombytes.c $(KECCAK_SOURCES)

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
	  $(SECP)/precomputed_ecmult.o $(SECP)/precomputed_ecmult_gen.o

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
	rm -f test/bench_levels_paper
	rm -f test/bench_levels2
	rm -f test/bench_levels3
	rm -f test/bench_levels5
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
	rm -f test/bench_classical
