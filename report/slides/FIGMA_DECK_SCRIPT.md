# Figma deck — spoken script

Deck: Figma file `8Vs37oLXZw8x3z6T5mA1j4`, section **DECK — 13 slides (recording order)**.
Beats live in section **DEMO BEATS**. Pace assumption **112 wpm** (the measured delivery rate,
not the older assumed 150). `data-time` equivalents below are *planning budgets*: only a timed
full rehearsal settles the runtime.

Rules this script follows: SPOKEN interprets, the slide carries the figures; no function-call
narration; every caveat that scopes a claim stays in SPOKEN, not BACKUP.

---

## 01 · Title
**SPOKEN:** Hello, I'm Royce Steven. This project asks whether the signature behind cross-chain swaps can move to post-quantum cryptography, and what that costs.

**BACKUP:** Title agreed with the supervisor at Meeting 12: name the adaptor signature, drop
"exotic signature schemes".

---

## 02 · Why: the quantum clock
**SPOKEN:** First, why now. One 2026 estimate puts fewer than half a million physical qubits at recovering a user's signing key in minutes. No such machine exists today; what is falling is the estimated cost. But a ledger's record is permanent, and changing its cryptography needs network-wide agreement. NIST already proposes retiring this class of signature after 2035.

**BACKUP:** Estimate is Babbush et al., ePrint 2026/625, cited in the report at §1.1 and in
`fig:whynow`. The NIST row is IR 8547 (draft, 2024) at 128-bit strength and above, where
secp256k1 sits — not the 112-bit row deprecated after 2030. Earlier RSA-2048 estimates were
removed from both report and deck: different target, and no ratio may be taken across targets.

---

## 03 · Why: the application
**SPOKEN:** Here is the application. Alice holds bitcoin, Bob escrows ether, and neither wants to move first. A swap links both payments: either both settle or neither does. Watch the lanes — each payment settles on its own chain, and no coin crosses. Only information crosses: one secret, revealed when the first leg settles.

**BACKUP:** Bitcoin and Ethereum are the motivating venues for this picture; the implemented
demonstration later is scoped as UTXO-coin-for-UTXO-coin, following the LAS paper. Prices are
value-matched at spot, CoinGecko 2026-08-21 11:57 UTC — re-read them before recording on
another day. The Ethereum leg settles through an escrow contract, not an account-to-account
payment.

---

## 04 · Why: the gap
**SPOKEN:** So where is the gap? What the chains sign with today is vulnerable to Shor. Its replacement is already standardised, and migration is under way well beyond blockchains: Cloudflare is already moving its own infrastructure. But the signature that makes a swap work is much less mature. Other advanced types are being built; the adaptor case much less so. Hence: can we build it, what does it cost, and can a chain verify it?

**BACKUP:** Why pay for this rather than a hash-time-locked contract? The hash lock shows — a
script on both chains, the same hash on both, and larger transactions. An adaptor signature
moves the lock inside the signature, so both settlements look like ordinary payments. What is
removed is the explicit protocol link; timing and amounts can still correlate the two legs.

---

## 05 · Method: what an adaptor signature does
**SPOKEN:** The method, in three stages. A locked signature can be checked against a public statement but cannot spend. Add the secret and it becomes an ordinary signature — indistinguishable on chain from a normal payment. Hold it beside the matching locked one, against the same statement, and the secret falls out. That recovery ties two payments together without a middleman.

**BACKUP:** Extraction takes the statement, the completed signature *and* the matching
pre-signature — the published signature alone does not reveal the witness. The recovered value
is checked against the statement before it is used. Publishing is application behaviour, not a
primitive of the scheme.

---

## 06 · Method: what I built
**SPOKEN:** Here is the method itself. The lattice core is reused; the adaptor layer is what I added. What makes a signature adaptable is a single substitution: the ordinary signature hashes its commitment, while the pre-signature hashes that commitment plus the public statement. That one addition is the whole mechanism. I then built the scheme again in Rust, independently, and the two agree byte for byte on a pinned known-answer value: an implementation check, not a security proof.

**BACKUP:** The pinned value covers packed outputs over four fixed vectors — public key, secret
key, signature, pre-signature and the adapted signature. Pre-verification and extraction are
asserted per vector, never hashed. The one substantive change is that pre-signing commits to
the ordinary commitment plus the public statement.

---

## 07 · Demo A: walking the swap  *(4 beats)*
**SPOKEN:** The first demonstration — the swap running as a model swap across two UTXO ledgers. Alice prepares Bob's locked payment. Bob checks it before preparing Alice's; he commits nothing until those checks pass. Alice completes her side with her secret, settling on Ledger B. Bob holds that published signature against the locked payment he already had, recovers the same secret, and completes Ledger A. Both ledgers see ordinary spends — no swap script, no shared hash.

**BACKUP:** No output is spent while the pre-signatures are exchanged; the first ledger event is
Alice's publication. Extraction needs both the published signature and the matching
pre-signature, and Bob must then complete Alice's pre-signature with the recovered value.

---

## 08 · Result: cost in time
**SPOKEN:** Now the cost, and it depends entirely on what it is measured against. Step one, classical to a post-quantum base, is the expensive one. Step two is what I measured: adding the adaptor layer to that same base, under eight percent per operation. The classical adaptor costs four-point-six times its own signing. So relative to its own base ours is cheaper, though in absolute time a lattice operation still costs more.

**BACKUP:** Overheads are paired and interleaved within each repetition; rejection attempts are
counted directly, never inferred from a timing ratio. The classical figure is derived from a
single mixed native-API tier, not a paired measurement, which is why the byte tier is shown
beside the core tier. Step one's size factor belongs to this build's simplified base — the
FIPS 204 route measures a smaller signature.

---

## 09 · Result: cost in bytes
**SPOKEN:** Size is where post-quantum actually hurts. Against a classical adaptor signature the lattice one is about seventy-two times larger, and that is a size ratio, not a timing one. So the dominant cost here is communication, not adaptor arithmetic.

**BACKUP:** As objects: 4,640 bytes against a 64-byte compact ECDSA signature, at Simplified
Dilithium-II. Inside a Bitcoin witness the classical item is DER-encoded and larger, which is
the figure the transaction table uses. The baseline is a functionality-matched ECDSA *adaptor*,
not plain ECDSA; the level match is engineering, not a proof.

---

## 10 · Result: what lands on chain
**SPOKEN:** What actually lands on chain? On Bitcoin the fields are those of an ordinary payment; only the witness slot grows. Bitcoin is more restricted, because its fields cannot be modified, so verifying this natively needs a consensus rule. Ethereum is more flexible: the lattice signature rides in contract data, verified by a contract at a gas cost. Neither venue adds an adaptor-specific field.

**BACKUP:** The Bitcoin column is read from a mined regtest swap leg and reconstructed against
BIP144 weight accounting; the Ethereum figure is one real-client receipt under EIP-7623. The
Bitcoin leg settled on a node carrying the experimental rule, so it is not a mainnet spend.
Both at Simplified Dilithium-III, one signature instance.

---

## 11 · Demo B: the client differential  *(2 beats)*
**SPOKEN:** The second demonstration. The same spend goes to two clients of the same release. The stock client accepts everything, including all nine negative controls. The patched client adds the rule: it still accepts the valid spend but rejects all nine. That difference is the evidence the rule is really checking. A patched node is not Bitcoin, and I have not analysed the rule's security.

**BACKUP:** The controls are a changed amount, a changed recipient and a foreign signature.
Verdicts come from block validity, which is consensus, not the mempool policy check. The swap
pipeline is: pre-sign both legs, complete and mine B, read the signature back from chain B,
recover the secret, complete A, mine A.

---

## 12 · Result: two boundary questions
**SPOKEN:** Two boundary questions, both settled by measurement. Does one full verification fit in a single Ethereum transaction? It does, at ninety-seven point eight percent of the cap, for one measured instance; two shortcuts I tested changed neither. And does the adaptor need the simplified research signature? Functionally, no, though that route's security is not analysed.

**BACKUP:** Shortcut one, statement truncation, failed at extraction every depth tested;
shortcut two, a succinct post-quantum proof route, was larger and slower than the deployed
prover at this relation size. One parameter set down, verification alone is charged 65 percent;
at the largest set one transaction is exceeded, and that is arithmetic, never a measured total.
Pre-signing and pre-verification still need adaptor-specific algorithms.

---

## 13 · Takeaways
**SPOKEN:** Back to the three questions. Can we build it? Yes, with a working two-ledger swap. What does it cost? Little in adaptor computation, many more bytes. Can a chain use it? Ethereum yes, through a contract with almost no margin; Bitcoin only with a new consensus rule. What limits deployment is size, verification cost and platform integration — not the adaptor layer. For Bitcoin: analyse that rule before shipping. For Ethereum: drive verification cost down. For protocol designers: budget the proof and the statement first. Thank you.

**BACKUP:** The Bitcoin rule costs 374 microseconds per input, about 11.3 times a Schnorr check
— and that ratio overstates it, because the security levels are not matched. The proof accounts
for 98.6 percent of end-to-end swap *time*, which is why the proof and the statement are the
things to optimise. Every figure carries its warrant: measured, derived, or cited.
