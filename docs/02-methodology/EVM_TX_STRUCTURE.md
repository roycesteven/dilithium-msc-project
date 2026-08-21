# Ethereum transaction structure and where an adaptor signature actually goes

**Purpose.** Meeting 8 (2026-07-31) required the transaction to be *broken down* — what
a standard transaction contains, and exactly which components the adaptor layer adds —
and ruled that *"the same question applies to the EVM anyway; the EVM becomes a
discussion of a more advanced solution after a fully complete Bitcoin solution."* This
document is the EVM counterpart of `BITCOIN_TX_STRUCTURE.md`, written to that ruling and
in the same shape, so the two venues can be compared field by field.

**Sequencing note (Meeting 8).** Bitcoin/UTXO is the deliverable of record. This
document exists so the EVM work is *discussed as the more advanced alternative*, not
re-opened as a second application. No new EVM measurement was taken for it: every gas
figure quoted here comes from the captured `forge --gas-report` in
`evidence/onchain/latest/gas_report.log` (run `20260730_164836`, `sig_bytes=6736`), and
the report consumes them as macros emitted by `scripts/gen_report_data.py`.

**Status of the numbers.** §1–§2 are *format*, from the Ethereum yellow paper and the
named EIPs; not measured and not needing to be. §3 is *derived arithmetic* over those
formats plus the project's measured object sizes. §4 is *measured* gas, quoted from the
evidence file above and never retyped from memory.

---

## 1. The standard Ethereum transaction

A type-2 (EIP-1559) transaction is an RLP-encoded list:

| Field | Type | Notes |
|---|---|---|
| `chain_id` | uint | replay protection (EIP-155) |
| `nonce` | uint | sender's transaction counter |
| `max_priority_fee_per_gas` | uint | tip (EIP-1559) |
| `max_fee_per_gas` | uint | cap (EIP-1559) |
| `gas_limit` | uint | |
| `destination` | 20 B or empty | contract address, or empty for creation |
| `amount` | uint | wei |
| **`data`** | byte string | **the calldata — where everything interesting lives** |
| `access_list` | list | EIP-2930 |
| `signature_y_parity`, `signature_r`, `signature_s` | — | **the transaction's own secp256k1 signature** |

**The structural fact that drives everything below.** Unlike Bitcoin, Ethereum has *two
completely separate* notions of signature:

1. The **transaction signature** (`y_parity, r, s`) — secp256k1 ECDSA, verified by
   consensus, fixed at 65 bytes, and *not replaceable*. Every transaction, including
   one that settles a post-quantum swap, is still authorised by an ECDSA signature.
2. Any **application-level signature** — an opaque byte string inside `data`, verified
   by contract code that the application deploys itself.

A LAS signature is unavoidably of the second kind. This is the sharpest contrast with
Bitcoin: in Bitcoin the adaptor signature *replaces* the signature the consensus rules
check (which is why a consensus change is required and why the stack-element limits
bind); in Ethereum it is *additional payload* that consensus never inspects, so no
consensus change is needed — the entire cost lands in calldata and execution instead.

## 2. What calldata costs

| Rule | Value | Source |
|---|---|---|
| non-zero calldata byte | 16 gas | EIP-2028 |
| zero calldata byte | 4 gas | EIP-2028 |
| calldata floor cost | 10 gas / token | EIP-7623 |
| **per-transaction gas cap** | **16,777,216** (2²⁴) | **EIP-7825** |
| `keccak256` | 30 + 6 per word | yellow paper |

The per-transaction cap of EIP-7825 is the binding constraint for this project and is
quoted throughout as `\gasCap`-family macros.

---

## 3. Where each adaptor component goes

The swap objects, and their fate on each venue:

| object | size (target set) | Bitcoin | Ethereum |
|---|---|---|---|
| adapted signature `σ` | 6,736 B | input's **witness stack** (replaces the ordinary signature) | **calldata** argument of `claimLAS*` |
| public key `pk = t` | 4,416 B | witness stack item 2; output commits only to its hash | **calldata** (verified path) or a fund-time commitment |
| public parameters `A′` | matrix | not carried — implied by the key | **calldata** (verified path), bound by a fund-time `keccak256(A′,t,M)` |
| statement `Y` | 4,416 B | **not on chain** — off-chain adaptor communication | **not on chain** — same |
| witness `y` | 704 B | revealed *implicitly* by publishing `σ`, never a field | same |
| proof of knowledge `π` | ≈31 kB | **off-chain only** | **off-chain only** |

Two observations carry across both venues and are the reason the communication
conclusion does not change with the venue:

- **The statement, the witness and π never touch the chain.** The adaptor layer adds
  *no on-chain field at all* in the honest path. What the chain sees is one ordinary
  signature, of the size the underlying signature scheme dictates. This is the
  scriptless-script property, and it is the same on both venues.
- **The whole on-chain cost difference between a classical and a post-quantum swap is
  therefore the signature size**, not the adaptor construction — 6,736 B against 64/65 B.

### 3.1 The three claim paths actually implemented

`evm/src/AdaptorSwap.sol` deploys three, so the cost of each *design choice* is
separable rather than conflated:

| function | what the chain does | what it does **not** do |
|---|---|---|
| `claimClassical(id, v, r, s)` | `ecrecover` — a precompile | — |
| `claimLAS(id, sigPacked)` | charges calldata for the packed signature + one `keccak256` over it | **does not verify the lattice signature** — this is the *calldata floor*, the unavoidable cost of merely transporting `σ` |
| `claimLASVerified(id, sigPacked, AprimeHat, t, message)` | full native lattice verification (`LASVerify`, reusing vendored ZKNox ETHDILITHIUM SHAKE256/NTT/SampleInBall), bound to the escrow by a fund-time `keccak256(A′,t,M)` commitment | — |

`claimLAS` is the honest lower bound and must always be described as such: it answers
"what would this cost if verification were free?", which isolates the calldata term from
the execution term.

---

## 4. Measured cost on the EVM

All figures from `evidence/onchain/latest/gas_report.log`; the report cites them as
macros (`\gasClassical`, `\gasLasFloor`, `\gasLasVerified`), never as literals.
(The `gasNaysay*` macros were dropped on 2026-08-19 when Naysayer left the
dissertation; the contract and its evidence stay, but no figure reaches the report.)

⚠️ **Both columns are EXECUTION gas, not transaction totals.** `forge --gas-report`
excludes the 21,000 intrinsic charge and the calldata cost, so the right-hand column
compares execution alone against a *per-transaction* cap. Execution is a lower bound on
the total — `21,000 + max(4·tokens + execution, 10·tokens)`, `tokens = zero_bytes +
4·non_zero_bytes` (EIP-7623) — so a row **above** the cap on execution definitively does
not fit, while a row **below** it is undecided until calldata and the intrinsic charge
are added. The one row measured as a whole transaction is `claimLASVerifiedOpt`, via a
real client receipt (`evidence/onchain_onetx/`).

| path | execution gas (median) | execution vs the 16,777,216 cap |
|---|---|---|
| `claimClassical` | 75,751 | 0.5 % |
| `claimLAS` (calldata floor, **no verification**) | 290,640 | 1.7 % |
| `claimLASVerified` (**full native verification**) | 56,647,378 | **3.4× the cap** |
| Naysayer `optimisticClaim` | 1,169,171 | 7 % |
| Naysayer `naysayNorm` (cheap fraud proof) | 291,310 | 1.7 % |
| Naysayer `naysayWprime` | 13,148,124 | 78 % |
| Naysayer `naysayDigest` (**largest measured fraud-proof path**) | 28,182,443 | **1.7× the cap** |

⚠️ `naysayWprime`'s 78 % is one of the undecided rows: it carries the whole public
matrix `A'` as `uint256[][]`, ~246 kB of coefficients before ABI encoding, and none of
that is in the figure. Re-encoding the committed vectors gives 353,988 calldata bytes
and a total of ~15.1 M gas (~0.90× the cap) for the tested instance — *derived* from the
measured execution gas plus an ABI reconstruction, not a receipt, and one instance only.
`naysayDigest` is the largest fraud-proof path **for the tested 32-byte message and
vectors**; that ordering survives charging every one of `naysayWprime`'s calldata bytes
as non-zero, but execution gas is itself data-dependent, so it is not a claim over all
inputs.

**The decomposition that matters.** Transporting the signature costs 290,640 gas — 3.8×
a classical claim, entirely affordable. Verifying it costs 195× more again. The gap is
not calldata: it is that **SHAKE256 is not EVM-native**, so the Fiat–Shamir hash must be
executed in EVM bytecode. That is why the size story and the deployability story point
in opposite directions here, and why local optimisation could not close it.

**Why the Naysayer variant does not rescue it.** The optimistic construction moves the
happy path under the cap (1.2 M gas), which is the result it was built to demonstrate.
But an optimistic scheme is only sound if the fraud proof is *executable*, and the
digest dispute — the one that catches a faulty hash, i.e. exactly the SHAKE256 term
above — is itself 1.7× the cap on execution gas alone. A fraud proof that cannot be mined is not a fraud
proof. This is a **negative result, and it is stated as one**: the optimistic route
relocates the problem rather than solving it.

**Conclusion, in Meeting-8's terms.** The EVM is the more advanced venue in the sense
that it needs *no consensus change* — the contract is deployable today, and the honest
path of the optimistic variant already fits. It is the less advanced venue in the sense
that execution alone for the only path which actually *verifies* the post-quantum
signature is 3.4× the per-transaction cap, while execution alone for the largest
measured dispute of the optimistic alternative is 1.7× it. Bitcoin inverts both: it needs a consensus change, but once granted, the
signature is merely *bytes in a witness*, metered by size and not by execution, and the
protocol becomes unremarkable. That contrast — not a gas number — is what the EVM work
contributes to the report.

### 4.1 What would have to change

Each is a *direction*, not a result, and none was implemented:

- a **SHAKE256 precompile** (or a dispute that avoids a full SHAKE256 pass), which
  attacks the dominant term directly;
- a **Merkle opening of the single disputed matrix row** instead of the whole `A′`,
  which attacks the `naysayWprime` calldata term;
- a **succinct proof of verification**, which replaces execution with proof
  verification — and, to stay post-quantum, must not be a pairing-based SNARK. The
  project's `rust/las-stark` explores exactly this and is honestly a *gadget*, not a
  complete proof of on-chain verification.

---

## 5. Sources

- EIP-1559 (fee market, type-2 transaction), EIP-2930 (access lists), EIP-155 (replay
  protection) — transaction fields in §1.
- EIP-2028 — calldata gas (16 / 4).
- EIP-7623 — calldata floor cost.
- **EIP-7825 — per-transaction gas cap of 16,777,216**, the constraint every figure in
  §4 is measured against.
- Measured gas: `evidence/onchain/latest/gas_report.log`, produced by
  `scripts/run_onchain_gas.sh` (which re-exports the signature fixture from `ref/`
  first, so gas is always measured at the current `SIGNATURE_BYTES`).
