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

.PHONY: all selftest clean

all: las_btc_tool

las_btc_tool: $(SHIM_C) $(LAS_C) las_consensus.h las_consensus_params.h
	$(CC) $(CFLAGS) $(WARN) $(LAS_SET) -I. -I$(REF) $(SHIM_C) $(LAS_C) -lpthread -o $@

selftest: las_btc_tool
	./las_btc_tool selftest

clean:
	rm -f las_btc_tool
