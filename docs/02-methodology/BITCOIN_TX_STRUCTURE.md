# Bitcoin transaction structure and where an adaptor signature actually goes

**Purpose.** Meeting 8 (2026-07-31) raised one substantive gap: the project reports a
transaction size for the UTXO swap but never says what a transaction *contains*, so
there is no way to see which components the adaptor layer adds, or whether the result
could sit in a real Bitcoin transaction at all. This document is the investigation
behind that answer. It is the source of truth for the report's transaction-structure
section and for the two diagrams (standard transaction vs adaptor-modified
transaction).

**Status of the numbers here.** Everything in §1–§3 is *format*, taken from the BIPs and
from Bitcoin Core's `src/policy/policy.h`; it is not measured and does not need to be.
Everything in §5 is *derived arithmetic* over those formats plus the project's measured
object sizes (`SIGNATURE_BYTES`, `PUBLIC_KEY_BYTES`, from
`evidence/stage2/latest/bench_swap.log`). Nothing here is a benchmark result and nothing
here was estimated.

**⚠️ SINCE 2026-08-06, THE ARITHMETIC IN §5 IS NO LONGER ONLY DERIVED — IT IS VALIDATED.**
Transactions of these shapes were built, relayed and mined on a stock Bitcoin Core v31.1
regtest node, and the client's own `vsize`/`weight` were read back:
`evidence/btc_regtest/latest/`, write-up
`docs/03-results/TWO_LEG_REAL_CLIENT_EXPERIMENT.md`. §5.1's 110 vB and §2's 111 vB were
reproduced exactly by a real client. Where a figure below is now measured rather than
derived, the section says so. This does not make §5 a benchmark: it makes the size model
one that a client has checked.

---

## 1. The standard Bitcoin transaction

### 1.1 Legacy serialization (pre-SegWit)

| Bytes | Field | Type | Notes |
|---|---|---|---|
| 4 | `version` | int32 LE | currently 1 or 2 |
| varies | `tx_in count` | compactSize | |
| varies | `tx_in[]` | — | see below |
| varies | `tx_out count` | compactSize | |
| varies | `tx_out[]` | — | see below |
| 4 | `lock_time` | uint32 LE | height or Unix time |

`tx_in`:

| Bytes | Field | Notes |
|---|---|---|
| 32 | `previous_output.txid` | |
| 4 | `previous_output.vout` | together the 36-byte **outpoint** |
| varies | `script bytes` | compactSize length of `scriptSig` |
| varies | `scriptSig` | **legacy: this is where the signature lived** |
| 4 | `sequence` | RBF signalling / relative timelock (BIP68) |

`tx_out`:

| Bytes | Field | Notes |
|---|---|---|
| 8 | `value` | satoshis, int64 LE |
| varies | `pk_script bytes` | compactSize |
| varies | `scriptPubKey` | the **spending condition** |

**compactSize encoding** (needed for exact byte counts): values 0–252 take 1 byte;
253–0xffff take 3 (`0xfd` + uint16); 0x10000–0xffffffff take 5 (`0xfe` + uint32);
above that 9 (`0xff` + uint64).

### 1.2 SegWit serialization (BIP141/BIP144)

```
[nVersion][marker][flag][txins][txouts][witness][nLockTime]
```

`marker` MUST be `0x00` (1 byte) and `flag` MUST be `0x01` (1 byte). The `witness`
field carries **one stack per input**, each serialised as a compactSize item count
followed by, per item, a compactSize length and the item bytes.

**The key structural fact for this project:** for a native SegWit output, `scriptSig` is
empty and the signature has moved out of the input and into the **witness** field. So
the correct answer to "where does the signature go in a Bitcoin transaction?" is: in the
witness, not in the input script.

### 1.3 Weight and virtual size

- `weight = base_size * 3 + total_size`, where `base_size` is the serialization with
  marker, flag and witness removed. Equivalently **non-witness bytes count 4 WU each,
  witness bytes count 1 WU each.**
- `vsize = ceil(weight / 4)`.
- Block limit: `weight <= 4,000,000`.

This 4× discount on witness bytes is load-bearing for a post-quantum signature, because
a post-quantum signature is *entirely* witness data. See §5.3.

---

## 2. The two spend types used as the comparison baselines

**P2WPKH** (SegWit v0, the ordinary "modern Bitcoin address" spend):
- `scriptPubKey` = `OP_0 <20-byte HASH160(pubkey)>` → **22 bytes**
- witness stack = `<signature> <pubkey>`; DER signature + sighash byte ≈ 71–72 B,
  compressed public key 33 B.

**P2TR key path** (SegWit v1, BIP341):
- `scriptPubKey` = `OP_1 <32-byte tweaked x-only pubkey>` → **34 bytes**
- witness stack = a single **64-byte** BIP340 Schnorr signature (65 with a non-default
  sighash byte). Key-path spends execute *no script at all*.

Note in both cases the output commits only to a **hash** of the public key (or a tweaked
key). The public key itself is revealed in the witness at spending time, not at funding
time. This matters in §4.

---

## 3. The limits that a large signature runs into

From Bitcoin Core `src/policy/policy.h` and the script interpreter:

| Constant | Value | Kind | Meaning |
|---|---|---|---|
| `MAX_SCRIPT_ELEMENT_SIZE` | **520 B** | consensus | maximum size of any item pushed on the script stack |
| `MAX_STANDARD_P2WSH_STACK_ITEM_SIZE` | **80 B** | policy | max size of each witness stack item in a standard P2WSH script |
| `MAX_STANDARD_TAPSCRIPT_STACK_ITEM_SIZE` | **80 B** | policy | same, for BIP342 tapscript |
| `MAX_STANDARD_P2WSH_STACK_ITEMS` | 100 | policy | |
| `MAX_STANDARD_P2WSH_SCRIPT_SIZE` | 3600 B | policy | max standard witnessScript |
| `MAX_STANDARD_TX_WEIGHT` | **400,000 WU** | policy | max weight Core will relay or mine |
| `MAX_BLOCK_WEIGHT` | **4,000,000 WU** | consensus | |

**Two of these were verified against the source actually being run, and one was measured.**
Reading the pinned v31.1 tree (`src/script/interpreter.cpp`, `ExecuteWitnessScript`) settles
a question this table left implicit — whether the 520-byte limit applies to the *initial
witness stack* or only to pushes performed by the script:

```cpp
// Disallow stack item size > MAX_SCRIPT_ELEMENT_SIZE in witness stack
for (const valtype& elem : stack) {
    if (elem.size() > MAX_SCRIPT_ELEMENT_SIZE) return set_error(serror, SCRIPT_ERR_PUSH_SIZE);
}
```

It applies to the initial stack, at **consensus** level. So neither a 6,736-byte signature
nor a 4,416-byte public key can reach a script as one element, however permissive the relay
policy: chunking is required, not merely prudent.

The 80-byte tapscript standardness limit was then **measured** rather than cited. A
transaction carrying those objects as 520-byte witness chunks was offered to two v31.1 nodes
differing only in policy: the default node refused it with its own reason string,
`bad-witness-nonstandard`, while the permissive node accepted it, mined it, and **the
default node accepted the containing block**. Standardness and validity are different
questions, and the run separates them
(`evidence/btc_regtest/latest/A3_testmempoolaccept_default.json`).

---

## 4. Where each protocol object actually goes

This is the direct answer to the meeting question ("what kind of other stuff should the
transaction include? how do we add the pre-signature, how do we add the witness?").

| Object | Symbol | On chain? | Where |
|---|---|---|---|
| statement | `Y = t'` | **no** | off-chain message `u1 -> u2` only |
| proof of knowledge | `pi` | **no** | off-chain message `u1 -> u2` only |
| pre-signature | `sigma_hat` | **no** | off-chain message only; never broadcast |
| witness / secret | `y` | **no** | never transmitted at all — see below |
| adapted signature | `sigma` | **yes** | the input's **witness stack**, exactly where an ordinary signature goes |
| public key | `pk = t` | **yes**, at spend time | witness stack item 2; the output commits only to its hash |
| signing message | sighash | **no** | it is a *digest of the transaction*, not a field of it |

Three consequences, and they are the substance of the answer:

1. **The adaptor layer adds no on-chain field whatsoever.** A settled adaptor swap
   transaction is byte-for-byte structurally identical to an ordinary payment: same
   fields, same order. Only the adapted signature `sigma` appears, and it occupies the
   slot an ordinary signature already occupies. This is precisely the "scriptless
   script" property — the contract logic leaves no on-chain trace.

2. **The witness `y` is never added to anything.** It is not transmitted and not
   stored. `u2` *derives* it from two objects it already has: the published `sigma`
   (read off the chain) and its own `sigma_hat`, via `Ext`. Asking "how do we add the
   witness to the transaction" has the answer "we do not, and that is the mechanism".

3. **Therefore every on-chain byte of difference is attributable to the signature
   scheme, not to the adaptor construction.** The adaptor overhead is purely
   computational and purely off-chain. This is a stronger claim than the report
   currently makes and it is checkable from the field table.

The one field that *does* change shape is the **spending condition** when a refund
branch is needed. A timelocked refund is expressed in Bitcoin as a script — e.g.
`OP_IF <claim pk> OP_CHECKSIG OP_ELSE <timeout> OP_CHECKLOCKTIMEVERIFY OP_DROP <refund
pk> OP_CHECKSIG OP_ENDIF` — committed to as a P2WSH hash in `scriptPubKey`, with the
branch selector and the witnessScript revealed in the witness at spend time. The happy
path of Fig. 1 does not use it.

---

## 5. Derived sizes

Object sizes used below, measured (`evidence/stage2/latest/bench_swap.log`, run
`20260730_162109`): LAS `SIGNATURE_BYTES` = 6736, `PUBLIC_KEY_BYTES` = 4416.

### 5.1 Classical baseline — 1-in/1-out P2WPKH

| Part | Bytes |
|---|---|
| version | 4 |
| tx_in count (compactSize 1) | 1 |
| input: outpoint 36 + scriptSig len 1 + sequence 4 | 41 |
| tx_out count | 1 |
| output: value 8 + len 1 + scriptPubKey 22 | 31 |
| lock_time | 4 |
| **base size** | **82** |
| marker + flag | 2 |
| witness: count 1 + (1+71) + (1+33) | 107 |
| **total size** | **191** |

`weight = 82*3 + 191 = 437 WU`; `vsize = ceil(437/4) = 110 vB`.

(110 vB for a 1-in/1-out P2WPKH is the standard published figure, which is the
cross-check that this arithmetic is being done correctly.)

### 5.2 Post-quantum — the same transaction with LAS

Modelled as the natural analogue of P2TR: the output commits to a 32-byte hash of the
LAS public key (`scriptPubKey` = 34 B), and the witness stack reveals `<sigma> <pk>`.

| Part | Bytes |
|---|---|
| version | 4 |
| tx_in count | 1 |
| input: outpoint 36 + scriptSig len 1 + sequence 4 | 41 |
| tx_out count | 1 |
| output: value 8 + len 1 + scriptPubKey 34 | 43 |
| lock_time | 4 |
| **base size** | **94** |
| marker + flag | 2 |
| witness: count 1 + (compactSize 3 + 6736) + (compactSize 3 + 4416) | 11,159 |
| **total size** | **11,255** |

`weight = 94*3 + 11,255 = 11,537 WU`; `vsize = ceil(11,537/4) = 2,885 vB`.

### 5.3 The comparison, and the witness discount

| Metric | Classical P2WPKH | LAS | Ratio |
|---|---|---|---|
| total size | 191 B | 11,255 B | **58.9x** |
| weight | 437 WU | 11,537 WU | **26.4x** |
| vsize (what fees are charged on) | 110 vB | 2,885 vB | **26.2x** |
| transactions per 4M WU block (**upper bound**) | 9,153 | 346 | 26.4x |
| share of `MAX_STANDARD_TX_WEIGHT` | 0.11 % | **2.9 %** | — |

The per-block figures are ceilings obtained by dividing the block weight limit by one
transaction's weight. They ignore the coinbase and every other transaction competing for
the same block, so they bound *relative* capacity between the two schemes; they are not
a throughput prediction.

**The witness discount is worth a lot here.** Because a post-quantum signature is
entirely witness data, it is billed at 1 WU/byte instead of 4. That pulls the
fee-relevant multiple down from 58.9x (raw bytes) to 26.2x (vsize) — the single most
favourable structural fact about putting a lattice signature on a UTXO chain, and it
exists only because SegWit moved signatures out of the base transaction. On a
pre-SegWit chain the same signature in `scriptSig` would cost 4 WU/byte.

**On limits.** A LAS settlement transaction at 11,537 WU is 2.9 % of
`MAX_STANDARD_TX_WEIGHT` (400,000 WU), so it is comfortably relayable; roughly 34 such
inputs fit in one standard transaction. This is the answer to "should we have a limit
for this?" — Bitcoin does have one, it is a policy weight limit rather than a gas
limit, and a single post-quantum swap settlement is nowhere near it. Contrast the EVM,
where the *baseline* native verifier measured 56.6 M gas — **≈3.4× EIP-7825's
per-transaction cap of 16,777,216** — so it cannot run as one transaction. Note the
form of that ratio: the measured cost *is* ≈3.4 times the cap; it does not *exceed* it
by 3.4×. The binding EVM constraint is that per-transaction cap, **not** the block gas
limit; comparing against the block limit is the framing this project already retracted
(`docs/03-results/LAS-08-performance-measured.md`). The later *optimised* verifier does
fit one transaction at D3 — see `docs/03-results/GAS_LIMIT_INVESTIGATION.md` §7 for the
three-way D2/D3/D5 state.

### 5.4 The obstacles that remain

Fitting the weight limit is not the same as being deployable. Two obstacles, in order of
severity:

1. **No deployed consensus rule can verify a LAS signature.** Bitcoin script offers
   `OP_CHECKSIG`/`OP_CHECKSIGVERIFY`/`OP_CHECKSIGADD`, which verify ECDSA (legacy and
   SegWit v0) or BIP340 Schnorr (Taproot). No lattice verification opcode exists, and
   key-path Taproot spends require the witness to be exactly a 64/65-byte BIP340
   signature. Some consensus change would therefore be needed. Several routes exist —
   a new witness version, a new opcode occupying one of the `OP_SUCCESSx` slots
   reserved by BIP342, or a separate chain or sidechain that ships lattice
   verification from genesis — and **this project takes no position on which is
   preferable**; the point here is only that Bitcoin as it stands cannot settle
   configurations 2 and 3.

   **The second of those routes has since been built and run** (2026-08-06):
   `OP_CHECKLASSIGVERIFY` at a BIP342 `OP_SUCCESSx` slot, patched into Bitcoin Core
   v31.1, verifying `ref/basesig.c base_verify_packed` over the BIP341 sighash. A
   transaction whose only authorisation is a LAS signature was accepted and mined by the
   patched node, while a stock node of the same release — for which `0xbb` is still
   `OP_SUCCESS` — accepted the same block. Patch, controls and scope:
   `bitcoin/patches/0001-op-checklassigverify-v31.1.patch`,
   `evidence/btc_las_node/latest/`, write-up
   `docs/03-results/TWO_LEG_REAL_CLIENT_EXPERIMENT.md` §3.

   **This does not change the sentence above, and must not be read as changing it.**
   Demonstrating that one route *can* be implemented is not a position on which route
   *should* be adopted, and a patched node is not Bitcoin: as deployed, Bitcoin still
   cannot settle configurations 2 and 3. The demonstration carries no security analysis
   and no costing of the new opcode.

   What eprint 2020/845 §4 assumes is narrower than any of those routes: it assumes the
   *venue*, "a UTXO-based blockchain like Bitcoin *where the signature algorithm is
   replaced with a lattice-based signature scheme*". That is an assumption about the
   setting, not a deployment proposal, and it should not be paraphrased as one. This
   project implements the stated setting; it does not claim to run on Bitcoin as
   deployed, and does not attribute a soft-fork design to the paper.

2. **The stack-element limits bind independently of that change.** A 6,736-byte
   signature is **12.9x** over the 520-byte consensus `MAX_SCRIPT_ELEMENT_SIZE` and
   **84x** over the 80-byte standardness limit for P2WSH/tapscript stack items; the
   4,416-byte public key is 8.5x and 55x over the same two.

   **Resolved in practice without raising either limit, and without `OP_CAT`.** An
   earlier version of this paragraph said a deployment would have to raise the limits or
   reconcatenate with `OP_CAT`. There is a third option, and it is what the Stage-3 patch
   does: **the opcode reassembles the operands itself** from a fixed number of
   fixed-length witness chunks — `ceil(6736/520) = 13` and `ceil(4416/520) = 9` — with
   every chunk length checked, so a short or transposed chunk cannot shift the
   reconstruction. Nothing in the interpreter's limits changes; the new rule simply reads
   more than one stack item. Whether that is the *right* design is a separate question
   this project does not answer, but "the limits must be raised" was too strong and is
   withdrawn.

Neither obstacle is caused by the adaptor layer. Both are properties of lattice
signature sizes, and they would apply equally to a plain (non-adaptor) post-quantum
payment.

---

## 6. Fidelity gaps in the project's own UTXO model

`rust/las-swap/src/utxo.rs` models a UTXO ledger but uses a bespoke serialization.
Measured against §1, the gaps are:

| Bitcoin | model before this work | severity |
|---|---|---|
| `version` (4 B) | absent | cosmetic |
| `marker`/`flag` (2 B) | absent | cosmetic |
| compactSize counts | fixed 4-byte u32 counts | cosmetic |
| outpoint 36 B | 36 B | — matches |
| `sequence` (4 B) | absent | minor (no RBF/BIP68 semantics) |
| `lock_time` 4 B | 8 B | cosmetic |
| output commits to a **hash** of the pk (22/34 B) | full public key inline (4,416 B) | **material** — puts PQ bytes in the funding output instead of the spending witness |
| witness segregated, billed at 1 WU/B | signature appended to the same buffer | **material** — the model cannot express weight or vsize at all, so it cannot report the fee-relevant number |
| refund = script branch committed as P2WSH | `refund_pk` + `timeout` struct fields | **material** for byte counts (a real refund reveals a witnessScript) |

The three material gaps are why the model's reported 11,233 B per settled transaction
and the 11,255 B of §5.2 differ, and why the model could not previously report a vsize.
Closing them is the implementation half of this deliverable.

---

## 7. Sources

- BIP141 (SegWit, consensus layer): the witness, weight, vsize, block weight limit,
  P2WPKH/P2WSH output semantics —
  <https://github.com/bitcoin/bips/blob/master/bip-0141.mediawiki>
- BIP144 (SegWit, peer services): the wire byte layout
  `[nVersion][marker][flag][txins][txouts][witness][nLockTime]` used for every byte
  count in §5 — <https://github.com/bitcoin/bips/blob/master/bip-0144.mediawiki>.
  (BIP141 also states the serialisation order; BIP144 is the definition of the `tx`
  message format itself, which is why the size arithmetic cites it.)
- BIP341 (Taproot): key-path witness, 64/65-byte signature, 34-byte scriptPubKey —
  <https://github.com/bitcoin/bips/blob/master/bip-0341.mediawiki>
- Bitcoin Core policy constants — <https://github.com/bitcoin/bitcoin/blob/master/src/policy/policy.h>
- Raw transaction byte layout and compactSize —
  <https://developer.bitcoin.org/reference/transactions.html>
- eprint 2020/845 §4 (the "signature algorithm is replaced" assumption)
