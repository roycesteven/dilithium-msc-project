# Build the consensus shim and its tooling STANDALONE — without Bitcoin Core.
#
# The shim has to be checkable on its own. If the only way to exercise it were to build a
# patched node, then every shim bug would arrive dressed as a consensus bug, and a failing
# transaction would leave you guessing which layer was wrong. `make selftest` answers the
# crypto question before the node is ever involved.
#
#   make            build las_btc_tool
#   make selftest   build and run the positive + 7 negative controls
#
# Parameter set: D3 (n=6, ell=5, kappa=49), the project's headline set, matching
# ref/serialize.h's static assertions and every other measurement in this project.
#
# NOTE the aborting randombytes stub is linked here too, exactly as the node links it: the
# signing path used by this tool is deterministic (base_keygen_seed / base_sign_det), so if
# it ever reached for entropy the tool would crash rather than emit an unreproducible run.

REF      := ../../ref
LAS_SET  := -DDILITHIUM_MODE=3 -DLAS_N=6 -DELL=5 -DKAPPA=49
WARN     := -Wall -Wextra -Wshadow -Wpointer-arith
CFLAGS   ?= -O2

LAS_C := $(REF)/basesig.c $(REF)/serialize.c $(REF)/setup.c $(REF)/relation.c \
         $(REF)/las.c $(REF)/poly.c $(REF)/polyvec.c $(REF)/ntt.c $(REF)/reduce.c \
         $(REF)/rounding.c $(REF)/fips202.c $(REF)/symmetric-shake.c $(REF)/packing.c \
         $(REF)/sign.c

SHIM_C := las_consensus.c las_btc_tool.c randombytes_abort.c

.PHONY: all selftest bench clean

all: las_btc_tool

las_btc_tool: $(SHIM_C) $(LAS_C) las_consensus.h las_consensus_params.h
	$(CC) $(CFLAGS) $(WARN) $(LAS_SET) -I. -I$(REF) $(SHIM_C) $(LAS_C) -lpthread -o $@

selftest: las_btc_tool
	./las_btc_tool selftest

# What the new consensus rule costs a validating node: the LAS predicate timed against the
# signature checks Bitcoin already runs (BIP340 Schnorr, ECDSA), in one process on one
# machine. Needs the same vendored library the classical baseline uses:
#   git clone --depth 1 https://github.com/BlockstreamResearch/secp256k1-zkp \
#       third_party/secp256k1-zkp
# Vendored TUs build with -w (their code); the harness builds with the project's warnings.
# Built into THIS directory under distinct names: the shared $(SECP)/secp256k1.o belongs to
# ref/Makefile's bench_classical and is compiled without the Schnorr/extrakeys modules this
# benchmark needs. Rebuilding it in place would silently change what that target links.
SECP := ../../third_party/secp256k1-zkp
SECP_OBJ := secp_bench.o secp_bench_ecmult.o secp_bench_ecmult_gen.o
SECP_MODULES := -DENABLE_MODULE_ECDSA_ADAPTOR=1 -DENABLE_MODULE_SCHNORRSIG=1 \
                -DENABLE_MODULE_EXTRAKEYS=1

secp_bench.o: $(SECP)/src/secp256k1.c
	$(CC) -O3 -w -I$(SECP) -I$(SECP)/src $(SECP_MODULES) -c $< -o $@
secp_bench_ecmult.o: $(SECP)/src/precomputed_ecmult.c
	$(CC) -O3 -w -I$(SECP) -I$(SECP)/src -c $< -o $@
secp_bench_ecmult_gen.o: $(SECP)/src/precomputed_ecmult_gen.c
	$(CC) -O3 -w -I$(SECP) -I$(SECP)/src -c $< -o $@

# ⚠ THE BUILD IS PART OF THE COMPARISON. The vendored secp256k1 objects above are compiled
# at -O3, and every benchmark in this project is built with ref/Makefile's
# `-O3 -fomit-frame-pointer`. Leaving this target on the Makefile's -O2 default would time an
# -O2 lattice verifier against an -O3 curve and report the difference as the cost of the
# consensus rule. Both sides are pinned to the same flags here for that reason; do not
# replace this with the generic $(CFLAGS).
BENCH_OPT := -O3 -fomit-frame-pointer

bench_las_consensus: bench_las_consensus.c las_consensus.c randombytes_abort.c $(LAS_C) \
                     las_consensus.h las_consensus_params.h $(SECP_OBJ)
	$(CC) $(BENCH_OPT) $(WARN) $(LAS_SET) -I. -I$(REF) -I$(SECP)/include \
	  bench_las_consensus.c las_consensus.c randombytes_abort.c $(LAS_C) \
	  $(SECP_OBJ) -lpthread -lm -o $@

bench: bench_las_consensus
	./bench_las_consensus

clean:
	rm -f las_btc_tool bench_las_consensus $(SECP_OBJ)
