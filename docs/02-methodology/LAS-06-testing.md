<!-- Part of docs/LAS.md, split by report chapter (2026-07-06). Index: docs/LAS.md.
     Section numbering is preserved verbatim, so external references like
     "LAS.md §6" resolve to this file. Do not renumber sections. -->

## 6. Testing

### 6.1 Functional tests (`ref/test/test_las.c`)
Per iteration (1000 iterations — the objectives' B1 acceptance bar of ≥1000 runs
at 100 % correctness — modes 2/3/5, random `pp`, keys, message):
1. `(pk, sk) = KeyGen`
2. `(Y, y)  = KeyGen` — statement/witness is another key pair
3. `σ̂ = PreSign(sk, Y, M)`
4. assert `PreVerify(Y, pk, σ̂, M) == true`
5. **assert `Verify(pk, σ̂, M) == false`** — the tripwire (hash omits `+Y`)
6. `σ = Adapt((Y, y), σ̂)`
7. assert `Verify(pk, σ, M) == true` — adapted sig is ordinary
8. `y' = Ext(Y, σ, σ̂)`; assert `A·y' == Y` **and** `y' == y` (exact)

Plus an ordinary `Sign`/`Verify` round-trip and a forgery check (flip a message
bit, expect Verify to reject). **Result: all assertions pass on all modes, with
zero warnings under `-Wall -Wextra -Wpedantic -Wmissing-prototypes -Wshadow -Wvla`.**

### 6.2 Why the tests are meaningful
Step 4 exercises pre-signature correctness; step 5 proves the statement is genuinely
*bound* (a pre-signature is not a usable signature); steps 6–7 prove adaptability and
that the result is an *ordinary* signature; step 8 proves witness extractability and
*exactness* (no knowledge-gap noise in this parameterisation). Together they
demonstrate the full adaptor-signature contract end to end.

### 6.3 Serialisation tests (`ref/test/test_serde.c`)
A separate suite exercises the byte-level encoding of Section 5.10 over 256 random
instances, hard-asserting: (i) **round-trip** `unpack(pack(x)) == x` for pk, sk, and
the ordinary / pre / adapted signatures; (ii) **verify-from-bytes** — a packed
`(pk, adapted σ)` verifies via `base_verify_packed`, while a packed *pre-signature*
is rejected (the tripwire survives serialisation); (iii) **tamper** — every one of
the `SIGNATURE_BYTES = 4640` single-byte flips of a valid packed signature makes it
fail verification (caught at Verify: the wire is `c_tilde ‖ BitPack(z)`, decoded
permissively); (iv) **validation** — `pack`/`unpack` reject out-of-range pk/sk
inputs (coefficient `≥ Q`, non-ternary code).
All pass, zero warnings.

### 6.4 Known-answer tests (`ref/test/test_kat.c`)
Reproducibility (objective C4) is verified by a KAT suite that fixes *all* inputs
(public-parameter seed, key seeds, statement seeds, messages) and uses the
deterministic API of Section 5.11. For `NVEC = 4` vectors it runs the full
deterministic pipeline (`keygen_seed → sign_det / presign_det → adapt → ext`),
hard-asserts the adaptor contract and that re-running the deterministic functions
yields byte-identical output, then folds the packed bytes of every object
(`pk, sk, σ, σ̂, σ_adapted`) into a single SHAKE256 digest and checks it against a
**pinned 32-byte expected value**. That one fingerprint locks down the entire
implementation: any unintended change to keygen, signing, the adaptor algebra, or
the serialisation flips the digest. Because every step is integer/SHAKE arithmetic
over a fixed canonical byte encoding, the digest is stable across machines and
compilers, and an independent verifier (Solidity/circuit) can be checked against
the same vectors. The digest is reproduced on every run; the test passes.

---

