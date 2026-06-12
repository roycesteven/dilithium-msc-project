CC ?= /usr/bin/cc
CFLAGS += -Wall -Wextra -Wpedantic -Wmissing-prototypes -Wredundant-decls \
  -Wshadow -Wvla -Wpointer-arith -O3 -fomit-frame-pointer
NISTFLAGS += -Wno-unused-result -O3 -fomit-frame-pointer
SOURCES = sign.c packing.c polyvec.c poly.c ntt.c reduce.c rounding.c
HEADERS = config.h params.h api.h sign.h packing.h polyvec.h poly.h ntt.h \
  reduce.h rounding.h symmetric.h randombytes.h
KECCAK_SOURCES = $(SOURCES) fips202.c symmetric-shake.c
KECCAK_HEADERS = $(HEADERS) fips202.h

.PHONY: all speed shared clean

all: \
  test/test_dilithium2 \
  test/test_dilithium3 \
  test/test_dilithium5 \
  test/test_vectors2 \
  test/test_vectors3 \
  test/test_vectors5 \
  test/test_las2 \
  test/test_las3 \
  test/test_las5 \
  test/test_swap3 \
  test/bench_las3 \
  test/bench_compare3 \
  test/bench_app3 \
  test/test_pcn3 \
  test/test_amhl3 \
  test/test_serde3 \
  test/test_kat3

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
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=2 \
	  -o $@ $< las.c randombytes.c $(KECCAK_SOURCES)

test/test_las3: test/test_las.c las.c las.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< las.c randombytes.c $(KECCAK_SOURCES)

test/test_las5: test/test_las.c las.c las.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=5 \
	  -o $@ $< las.c randombytes.c $(KECCAK_SOURCES)

test/test_swap3: test/test_swap.c las.c las.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< las.c randombytes.c $(KECCAK_SOURCES)

test/bench_las3: test/bench_las.c las.c las.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< las.c randombytes.c $(KECCAK_SOURCES)

test/bench_compare3: test/bench_compare.c las.c las.h randombytes.c $(KECCAK_SOURCES) \
  $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< las.c randombytes.c $(KECCAK_SOURCES)

test/bench_app3: test/bench_app.c amhl.c amhl.h serialize.h las.c las.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< amhl.c las.c randombytes.c $(KECCAK_SOURCES)

test/test_pcn3: test/test_pcn.c chain.c chain.h las.c las.h randombytes.c \
  $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< chain.c las.c randombytes.c $(KECCAK_SOURCES)

test/test_amhl3: test/test_amhl.c amhl.c amhl.h chain.c chain.h las.c las.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< amhl.c chain.c las.c randombytes.c $(KECCAK_SOURCES)

test/test_serde3: test/test_serde.c serialize.c serialize.h las.c las.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
	  -o $@ $< serialize.c las.c randombytes.c $(KECCAK_SOURCES)

test/test_kat3: test/test_kat.c serialize.c serialize.h las.c las.h \
  randombytes.c $(KECCAK_SOURCES) $(KECCAK_HEADERS)
	$(CC) $(CFLAGS) -DDILITHIUM_MODE=3 \
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
	rm -f test/bench_las3
	rm -f test/bench_compare3
	rm -f test/bench_app3
	rm -f test/test_pcn3
	rm -f test/test_amhl3
	rm -f test/test_serde3
	rm -f test/test_kat3
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
