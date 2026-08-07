# Real clients: a two-leg swap on the EVM, and LAS verified by a patched Bitcoin Core

**What this document is.** The write-up for three experiments run on 2026-08-06, all of
which replace a model with a real client. It is the source of truth for their claims,
their scope, and — as importantly — what they do *not* establish.

**Why they exist.** Until now the two venues sat at different evidential levels. The EVM
had a real node but settled a single leg over an opaque message; Bitcoin had no client at
all — `scripts/gen_bitcoin_tx_data.py` is arithmetic (its whole import list is `argparse,
math, re, sys, pathlib`) and `rust/las-swap/src/utxo.rs` is a `Vec<Tx>` whose `mine()` is
`self.height += blocks`. Both labelled themselves honestly, so **nothing here retracts
anything**; these close a gap the project already documented.

| stage | client | what settles it | evidence |
|---|---|---|---|
| 1 | two `anvil` nodes, two chain ids | a whole Fig. 1 swap, both legs verified on-chain | `evidence/onchain_twoleg/20260806_141719/` |
| 2 | stock Bitcoin Core v31.1 | LAS-sized objects carried, relayed, mined | `evidence/btc_regtest/20260806_142224/` |
| 3 | Bitcoin Core v31.1 **patched** | LAS verified as a consensus rule | `evidence/btc_las_node/20260806_154707/` |

---

## 1. Stage 1 — a two-leg atomic swap across two real Ethereum clients

### 1.1 The gap this closed, which was not a small one

`AdaptorSwap.claimLASVerifiedOpt` requires
`keccak256(abi.encode(aHatPacked, tHatPacked, tPacked, message)) == sw.lasContext`. That
commits the verification parameters and an **opaque `message` chosen by the funder** — in
the measured one-transaction run, `msg.bin`, whose contents are `msg[i] = i`. Not bound:
domain separator, chain id, contract address, escrow id, payer, beneficiary, amount.

So two settled legs would have evidenced **two valid LAS signatures, not a swap**. A
signature could be replayed onto another escrow funded with the same context, and nothing
tied it to the payment being made.

### 1.2 The fix: a derived message, in a contract with one way out

`LASRegister.claimMessage` derives

```
message = keccak256(abi.encode(DOMAIN, block.chainid, address(this),
                               id, payer, beneficiary, amount))
```

Every field is intrinsic or already in storage, so the contract computes it at claim time
and the claimer supplies nothing that is trusted. It stays **exactly 32 bytes**, which
preserves the one-transaction result's scope: the SHAKE256 preimage is
`pack(t) ‖ pack(w') ‖ M` and the sponge dominates execution, so a longer message would
have changed the thing being measured.

There is **no leg index**. It is not in the `Swap` struct, so the contract could not derive
it and would have to accept it as an argument — reintroducing the unbound input the change
removes. It is also redundant: the legs live on different chains, in different contracts,
under different ids.

**`AdaptorSwapBound` is a new contract, not a modification.** `AdaptorSwap` is a
measurement harness: it deliberately exposes `claimLAS`, which pays out after a length
check and one `keccak256` and performs **no verification** — that is the on-chain floor it
exists to price. The consequence is that no escrow there is gated on verification at all;
any OPEN escrow can be drained through the floor path. Fine for measuring, fatal for
settling. Adding a mode flag to gate it would have changed the gas of the paths the
published figures were measured against, so the settling contract is separate and has
exactly two exits: `claimBound` (full verification) and `refund`.

`evm/test/AdaptorSwapBound.t.sol` holds 13 controls. Each asserts the **revert reason**,
because every one of them reverts either way and a test that only checked "it reverted"
would keep passing if the binding check were deleted. The positive control asserts the call
gets *past* binding and dies at `LAS verify failed`. One control is behavioural rather than
ABI-based: it sends complete, well-formed calldata for each unverified path against a live
funded escrow and requires that no funds move — a bare 4-byte selector would revert on
argument decoding whether or not the function existed.

### 1.3 The run

Two `anvil` instances, chain ids 31337 and 31338, each with its own deployment. **Two
escrows on one chain would not be a cross-chain swap** and would never exercise the
property the protocol exists for.

The whole of eprint 2020/845 Fig. 1, with each step gated:

1. u₁ generates `(Y, y)` and **π** (30,715 B, LaZer); u₂ **verifies π** before pre-signing —
   without it, u₂ has no assurance `Y` admits a ternary witness, so the `y` it later
   extracts need not complete leg A and the atomicity argument does not close.
2. Both parties `PreSign` over their leg's derived message; **both `PreVerify` before any
   `Adapt`**. A cross-leg `PreVerify` is required to *fail*.
3. u₁ adapts and settles leg B on chain 2.
4. The runner **slices σ₂ out of the mined transaction**, decoding the ABI head rather than
   using a constant offset, and hands those bytes — and only those — to
   `extract_and_adapt`, a separate program with no access to the local copy.
5. u₂ settles leg A on chain 1.

| | leg B (chain 2) | leg A (chain 1) |
|---|---|---|
| status | SUCCESS | SUCCESS |
| gasUsed | 16,413,616 | 16,413,420 |
| cap (EIP-7825) | 16,777,216 | 16,777,216 |
| calldata byte-equal | YES | YES |

σ₂ recovered from the chain was byte-identical to the broadcast copy; the live replay
control (leg A's claim carrying leg B's message) was refused with the contract's own
`message not bound` **and** a real broadcast left the escrow untouched; both fee-adjusted
payouts were exactly the escrowed wei.

**The payout assertion adds the fee back.** In Fig. 1 each claimant is also the beneficiary
of the leg it claims, so it pays for its own payout: a raw balance delta understates it by
exactly `gasUsed × effectiveGasPrice`, both read from the receipt.

---

## 2. Stage 2 — carriage on a stock, unmodified Bitcoin Core

### 2.1 Scope, which is narrow on purpose

**Claims:** construction, policy acceptance or refusal, broadcast, mining, serialisation,
and the real `vsize`/`weight` of LAS-sized objects.
**Does not claim:** anything about LAS verification. Stock Script has no lattice opcode; the
spend is authorised by an ordinary BIP340 Schnorr signature and the LAS bytes are dropped
from the stack. No line of this stage may be described as Bitcoin verifying a LAS signature.

### 2.2 Pins, which are gates rather than notes

"We ran it on Bitcoin Core" means nothing if the tree carried a patch. So: the binary must
report the pinned tag; `HEAD` must equal the **tag's commit** (`git describe` also matches
commits made *after* a tag); and `git status --porcelain` must be empty. An extracted
tarball is refused — its digest pins the download, not the tree being run against, and it
cannot show the tree is unmodified now.

Recorded: `v31.1`, source commit `9be056a8a72b624dae9623b2f7bded92c2a21c91`, binary
SHA-256 `986e63b3…` (verified against the published `SHA256SUMS`), and the Core helpers used.

### 2.3 Provenance: Core's own helpers, and version drift handled rather than guessed

The carriage transaction is built with `test/functional/test_framework` — keys,
`taproot_construct`, the BIP341 sighash, witness assembly, serialisation. Consensus-adjacent
crypto written a second time is where a subtly wrong result still looks right.

That framework is internal API and it moves. Two traps were hit and are now handled by
resolving against the pinned tree rather than by recall:

- `CTransaction.nVersion` is `.version` in this release; `TaprootSignatureHash` takes
  `leaf_script=`. Both are probed, and a mismatch fails immediately naming the symbol.
- **`TaprootLeafInfo` has no control block** (its fields are `script, version, merklebranch,
  leaf_hash`), so the control block is assembled from its BIP341 parts,
  `leaf_version | parity ‖ internal_key ‖ merkle_branch`.
- `TaprootSignatureMsg` defaults `codeseparator_pos=-1` and then encodes it with
  `to_bytes(4, "little", signed=False)`, which raises `OverflowError`. The default is
  unusable; BIP341's sentinel `0xFFFFFFFF` is passed explicitly. Relying on the default
  would be relying on a bug.

### 2.4 Two nodes, one chain

`-acceptnonstdtxn=1` is a startup option, so the two policy regimes are two datadirs. They
are **peered onto one chain** and every funding block is synced before anything is offered
to a mempool: a node lacking the funding transaction answers `missing-inputs`, which looks
like a policy difference but is not. That answer fails the run rather than being reported.

### 2.5 Results

| | default policy | permissive | mined | vsize | weight |
|---|---|---|---|---|---|
| A1 P2WPKH, signed by the node | ACCEPTED | ACCEPTED | ✓ | **110** | 437 |
| A2 P2TR key path | ACCEPTED | ACCEPTED | ✓ | **111** | 444 |
| A3 carriage (22 chunks + Schnorr + leaf + control = 25 items) | **REJECTED: `bad-witness-nonstandard`** | ACCEPTED | ✓ | 2,939 | 11,753 |

Three things follow.

**The published reference figures are reproduced by a real client.** A1 and A2 landed on
110 and 111 vB — the two constants `gen_bitcoin_tx_data.py` self-checks against, now
confirmed rather than assumed.

**The policy/consensus split is now MEASURED, not cited.** §3 of `BITCOIN_TX_STRUCTURE.md`
previously carried a table of constants read from `policy.h`. A3 is refused by default
relay policy — with the node's own reason string — yet is consensus-valid, mined, and the
**default-policy node accepted the containing block**. Standardness and validity are
different questions and the run separates them.

**This project's size model reproduced the client's `vsize` and `weight` for every mined
transaction.** The model (`bitcoin/tools/btc_sizes.py`) is deliberately independent of Core:
checking a Core helper against Core's consensus code would test nothing. The gate is *not*
"A1 must equal 110 vB" — a DER signature's length varies, which is exactly why the oracle is
a client and not a constant.

### 2.6 What this corrected in the report

`gen_bitcoin_tx_data.py` projected configuration 1 from the **64-byte compact ECDSA
signature** `bench_swap` reports. That is the right number for a communication measurement
and the wrong one for a P2WPKH witness, which requires DER plus a sighash byte. The
understated classical baseline inflated every post-quantum ratio.

The measurement settles it: A1, signed by a real node, is 191 B / 437 WU / 110 vB — exactly
what a 71-byte witness item produces, and exactly what the script's own self-check already
demanded. The generator now projects the reference witness shape, and the macros moved to
agree with §5.3 of `BITCOIN_TX_STRUCTURE.md`, which had been right all along.

(`\btcOneSig` still reports 64 B, which is correct: it is the signature *object* size from
the harness. The 71 bytes are the *witness item*, DER-encoded with its sighash byte.)

---

## 3. Stage 3 — LAS verified by a patched Bitcoin Core

### 3.1 What makes this different from Stage 2

The leaf is `<sha256(pk)> OP_CHECKLASSIGVERIFY OP_1` and there is **no Schnorr signature
anywhere**. Nothing authorises the spend except the patched rule accepting the LAS
signature. Accepted and mined therefore means verified.

### 3.2 The consensus change: one mechanism, minimally

`0xbb` is removed from `IsOpSuccess` and defined as `OP_CHECKLASSIGVERIFY` — the BIP342
`OP_SUCCESSx` upgrade path, which is what those slots are reserved for. No new leaf version,
no change to `MAX_SCRIPT_ELEMENT_SIZE`.

**The 520-byte limit is not optional, and the pinned source says so.**
`ExecuteWitnessScript` applies `MAX_SCRIPT_ELEMENT_SIZE` to the **initial witness stack**:

```cpp
// Disallow stack item size > MAX_SCRIPT_ELEMENT_SIZE in witness stack
for (const valtype& elem : stack) {
    if (elem.size() > MAX_SCRIPT_ELEMENT_SIZE) return set_error(serror, SCRIPT_ERR_PUSH_SIZE);
}
```

So a 6,736-byte signature cannot arrive as one element at consensus level, and chunking is
required rather than merely prudent: `ceil(6736/520) = 13` and `ceil(4416/520) = 9`, 22
chunks, every length fixed so a short chunk cannot shift the reconstruction.

`CheckLASSignature` mirrors `CheckSchnorrSignature` — same `SignatureHashSchnorr`, same
`ScriptExecutionData` — so a LAS signature commits to the same transaction digest a Schnorr
signature would. **`SIGHASH_DEFAULT` only**: the other types narrow what a signature commits
to, and a post-quantum signature whose binding could be narrowed that way would authorise a
payment its signer never saw.

The diff is 45 files, 7,495 insertions, 1 deletion, and it **applies cleanly to a pristine
v31.1 checkout** — `bitcoin/patches/0001-op-checklassigverify-v31.1.patch`, SHA-256
`6fb8161d…`. The vendored LAS sources are included in it: a patch that omitted them would
not reproduce the build.

### 3.3 The shim, and why it is C

`bitcoin/las_consensus/las_consensus.c` expands the public parameters once (`pthread_once`,
because Core verifies scripts in parallel; expanding per call would make every measurement a
measurement of matrix expansion) and calls `base_verify_packed`. **No new cryptography is
written for the node.**

It is C, not C++, because the `ref/` headers carry C11 `_Static_assert` wire-size anchors
that a C++ translation unit cannot parse. The alternatives were to macro over
`_Static_assert` — silently disabling the assertions that pin the wire sizes — or to
re-declare the ref ABI by hand in C++, giving two declarations free to drift. Both trade a
real safety property for a cosmetic one. Core already links C libraries; libsecp256k1 is C.

`randombytes_abort.c` **replaces** the reference entropy source. A node only verifies, so
the keygen/sign paths are unreachable — but "unreachable" is a claim, and linking the real
implementation would let a mistake become silently non-deterministic validation, which is a
chain split rather than a bug report. It aborts instead.

**The consensus parameter seed** is `SHA-256("LAS-CONSENSUS-PARAMS-v1")` =
`e2a16bef…`. A hard-coded constant nobody can re-derive is a constant nobody can check, so
the runner hashes the preimage in Python every run and compares it to what the compiled
tool prints. (The check is not in the C selftest: `ref/` ships SHAKE, not SHA-256, and a
check that cannot actually be performed is worse than none.)

### 3.4 Controls, at two levels

**Shim level, before any node existed** (`las_btc_tool selftest`): positive control accepted;
seven negatives rejected — signature bit flipped, public key bit flipped, message bit
flipped, signature one byte short, public key one byte short, valid signature over the wrong
message, valid signature under the wrong key. Plus two message-length refusals and two
cross-context controls proving negatives 6 and 7 fail because of *binding* rather than
because those signatures were malformed.

**Node level, as a CONSENSUS question.** Every case goes to `generateblock ... submit=false`,
which builds a block containing the transaction and runs block validation without submitting
it. `testmempoolaccept` would answer a different question — it mixes in relay policy, under
which these witnesses are non-standard for a reason Stage 2 already measured and which has
nothing to do with LAS. Only a `TestBlockValidity failed:` error counts as a rejection; any
other failure is propagated as a hard error, because recording an RPC fault as "REJECTED"
would manufacture a passing control out of a broken run.

**The differential control is the heart of it.** On a **stock** v31.1 node `0xbb` is still
`OP_SUCCESS`, so tapscript succeeds unconditionally: the stock node accepts every one of
these whatever the signature says. That is the soft-fork property, and it makes the stock
node a control that cannot be reacting to the signature.

| case | patched | stock |
|---|---|---|
| valid | ACCEPTED | ACCEPTED |
| output amount changed | REJECTED — *LAS signature did not verify* | ACCEPTED |
| output recipient changed | REJECTED — *LAS signature did not verify* | ACCEPTED |
| input outpoint changed (to a **real** second UTXO) | REJECTED — *LAS signature did not verify* | ACCEPTED |
| sighash over a false prevout amount | REJECTED — *LAS signature did not verify* | ACCEPTED |
| signature for a different transaction | REJECTED — *LAS signature did not verify* | ACCEPTED |
| signature over a non-sighash message | REJECTED — *LAS signature did not verify* | ACCEPTED |
| chunk reordered | REJECTED — *LAS signature did not verify* | ACCEPTED |
| chunk truncated | REJECTED — *Malformed LAS witness* | ACCEPTED |
| wrong public key | REJECTED — *public key does not match the committed hash* | ACCEPTED |

Two design points that make these mean something. The input-outpoint control points at a
**real second output**, funded for the purpose: a nonexistent outpoint would be refused for
missing inputs, which says nothing about the sighash covering inputs. And the false-prevout
control **signs the false sighash it produces** — reusing the honest signature would leave
the transaction unchanged and test nothing.

### 3.5 The result

The valid spend was mined by the patched node: txid `01b4fa7f…`, block `66b1807e…`, **vsize
2,917, weight 11,667**. The stock node accepted the same block — soft-fork compatible. The
size model reproduced the client's figures again.

### 3.6 What Stage 3 does NOT establish

- **No security analysis.** The construction is demonstrated, not proven. Sigops are charged
  as one tapscript sigop per verification, which is a demonstration choice and not a costing:
  the relative validation cost of LAS versus Schnorr is not analysed here, and a real
  deployment would have to price it.
- **This is not a proposal.** `BITCOIN_TX_STRUCTURE.md` §5.4 names three possible routes and
  says the project takes no position on which is preferable. Implementing one as a
  feasibility demonstration does not change that, and the write-up must not be read as
  advocating a soft fork.
- **A patched node is not Bitcoin.** "Configurations 2 and 3 could not settle on Bitcoin as
  it stands" remains true and remains in the report.
- **Not wired into the swap.** Stage 3 settles a single LAS-authorised spend. The two-leg
  Bitcoin swap over this opcode was not run.
- **No performance measurement.** Validation cost per input was not measured.

---

## 4. Reproducing

```bash
# Stage 1 — needs the vendored LaZer build for pi
./scripts/run_onchain_two_leg.sh

# Stage 2 — stock Core; binary and source must be the same release
BTC_TAG=v31.1 BTC_BIN=…/bitcoin-31.1/bin/bitcoind BTC_SRC=…/bitcoin \
  ./scripts/run_btc_regtest_carriage.sh

# Stage 3 — apply the patch to a clean v31.1 checkout, build, then run
git -C "$BTC_SRC" checkout v31.1
git -C "$BTC_SRC" apply bitcoin/patches/0001-op-checklassigverify-v31.1.patch
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_GUI=OFF -DBUILD_TESTS=OFF \
      -DENABLE_IPC=OFF && cmake --build build -j4 --target bitcoind bitcoin-cli
BTC_TAG=v31.1 BTC_SRC=… BTC_BIN_PATCHED=…/build/bin/bitcoind \
  BTC_BIN_STOCK=…/bitcoin-31.1/bin/bitcoind ./scripts/run_btc_las_node.sh

# the shim alone, without any node
make -C bitcoin/las_consensus selftest
```

Every runner keeps a failing run's evidence, does **not** move `latest`, and exits non-zero.

## 5. Sources

- eprint 2020/845 §4.1 Fig. 1 — the swap protocol these stages execute.
- BIP341 (sighash, control block, NUMS point), BIP342 (`OP_SUCCESSx`, tapscript).
- Bitcoin Core v31.1 at commit `9be056a8`: `src/script/interpreter.cpp`
  (`ExecuteWitnessScript`, `CheckSchnorrSignature`), `src/script/script.cpp`
  (`IsOpSuccess`), `test/functional/test_framework/{script,key,messages}.py`.
- `docs/02-methodology/BITCOIN_TX_STRUCTURE.md` — the size model these runs validate.
- `docs/03-results/GAS_LIMIT_INVESTIGATION.md` §7 — the one-transaction EVM result Stage 1
  extends to a whole swap.
