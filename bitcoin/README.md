# Bitcoin: carriage on a stock node, and (Stage 3) verification on a patched one

Two separate questions, deliberately kept apart:

| stage | node | question | claim |
|---|---|---|---|
| **2** | **stock, unmodified** Bitcoin Core | can a transaction carrying LAS-sized objects be built, relayed, mined, and what does it weigh? | construction, policy, broadcast, mining, serialisation, carriage — **not verification** |
| **3** | Bitcoin Core **patched** with `OP_CHECKLASSIGVERIFY` | can a node verify a LAS signature as a consensus rule, and what does it cost? | verification and settlement |

Stage 2's spend is authorised by an ordinary **BIP340 Schnorr** signature; the LAS bytes are
pushed and dropped. Stock Bitcoin Script has no lattice opcode, so nothing in Stage 2 may
be described as Bitcoin verifying a LAS signature — that sentence belongs only to Stage 3,
and only against the patched build.

## What you need for Stage 2

Both must be the **same release**, and the source tree must be a **clean git checkout at
exactly the pinned tag**. The runner enforces all of this and refuses to proceed otherwise.

```bash
# 1. the binary — download a release from bitcoincore.org and verify its signatures
#    (SHA256SUMS + SHA256SUMS.asc) exactly as the project's own instructions describe
BTC_BIN=/opt/bitcoin-28.0/bin/bitcoind

# 2. the source, at the same tag
git clone https://github.com/bitcoin/bitcoin ~/src/bitcoin
git -C ~/src/bitcoin checkout v28.0
```

Nothing is compiled for Stage 2. The source tree is needed for two reasons: the transaction
is built with Core's own `test/functional/test_framework` helpers, and the standardness and
consensus limits the write-up cites are read from `src/policy/policy.h` and
`src/script/interpreter.cpp` **at the tag actually being run** rather than from
documentation.

### Why a git checkout and not a tarball

An archive digest pins what was *downloaded*; it cannot show the tree you are *running
against* is unmodified. `git status --porcelain` can, and `HEAD == the tag's commit` pins it
exactly — `git describe` would also match commits made after the tag. Supply
`BTC_SRC_ARCHIVE` as well if you have one and its SHA-256 is recorded too, but the checkout
is the gate.

## Running it

```bash
BTC_TAG=v28.0 \
BTC_BIN=/opt/bitcoin-28.0/bin/bitcoind \
BTC_SRC=~/src/bitcoin \
./scripts/run_btc_regtest_carriage.sh
```

Evidence lands in `evidence/btc_regtest/<timestamp>/`, with `latest` moved only on success.

### The three transactions

| | what | why |
|---|---|---|
| A1 | 1-in/1-out **P2WPKH**, signed by the node's own wallet | a real DER signature, which is what corrects the projection's 64-byte raw-ECDSA baseline |
| A2 | 1-in/1-out **P2TR key-path** | a second reference point |
| A3 | **carriage**: P2TR script-path whose witness holds the LAS signature and public key as **22 chunked stack items** (13 + 9, ≤520 B each), dropped by the leaf | the projected transaction, made of real bytes, put to a real client |

### Two nodes, one chain

`-acceptnonstdtxn=1` is a startup option, so default and permissive policy are two datadirs.
They are **peered onto one chain** and every funding block is synced before anything is
offered to a mempool: a node lacking the funding transaction answers `missing-inputs`, which
would look like a policy difference but is not. That answer fails the run rather than being
reported.

### What fails the run, and what is merely recorded

- **Gate** — A3 must mine. Without a mined transaction there is no measured `vsize`/`weight`
  and the stage yields nothing.
- **Gate** — this project's size model (`bitcoin/tools/btc_sizes.py`, the same arithmetic
  behind the report's byte counts) must reproduce the client's `vsize` and `weight` for every
  mined transaction. This is *not* "A1 must equal 110 vB": a DER signature's length varies,
  which is precisely why the oracle is a real client rather than a published constant.
- **Result, recorded either way** — the default-policy verdict for each transaction. Whether
  520-byte witness items are standard is not predicted here; the node is asked, and whatever
  it answers is the finding.

## Files

- `tools/btc_carriage.py` — builds A3 with Core's own helpers (keys, `taproot_construct`,
  BIP341 sighash, witness assembly, serialisation). Every symbol and every attribute whose
  name has moved between releases is resolved against the pinned tree and reported by name
  if missing, so a version mismatch fails immediately instead of producing a subtly wrong
  transaction. The control block is assembled from its BIP341 parts rather than read from a
  convenience attribute, and the txid comes from the node.
- `tools/btc_sizes.py` — this project's BIP141/BIP144 size model. Deliberately independent
  of Core: checking a Core helper against Core's consensus code would test nothing.
- `tools/btc_model_check.py` — compares that model with a `decoderawtransaction` result.
- `patches/` *(Stage 3)* — the consensus diff, kept as patches so it is auditable without
  vendoring Core's history.
- `las_consensus/` *(Stage 3)* — the C++ shim over `base_verify_packed`.

`core/` and any build tree are git-ignored; nothing under this directory is compiled during
Stage 2.
