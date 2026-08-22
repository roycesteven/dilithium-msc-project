#!/usr/bin/env python3
"""Decide whether on-chain LAS verification can still fit ONE EIP-7825 transaction at D5.

⚠ ONLY DILITHIUM-III IS MEASURED END TO END. This script produces a DERIVED verdict resting on
a MEASURED lower bound. Report it as derived, never as measured (→ EVIDENCE-OR-SILENCE).

It refuses to conclude anything it cannot support: if the absorbPad growth measurement is missing,
the answer is UNRESOLVED, not a guess.

THE ARGUMENT, IN FULL

  D5 fits only if      21000 + calldata_charge(D5) + execution(D5)  <  16,777,216

  * calldata is pushed the way that FAVOURS D5 fitting: every byte D5 adds over D3 is assumed
    ZERO (the cheapest a byte can be under EIP-7623). No real packed lattice payload achieves
    this — one byte in every packed 4-byte coefficient word is structurally zero (each
    coefficient is < q < 2^23) and the rest are near-uniform — so the true charge is higher
    and the real margin worse. The zero count does not depend on byte order, which differs
    across the payload: aHatPacked/tHatPacked are big-endian, tPacked little-endian.
    ⚠ This is the step that defeated two earlier attempts: it is not enough to push execution
    the adverse way while leaving calldata content free. A worst case binds only when EVERY
    free variable is pushed the adverse way at once.

  * ABI framing is recomputed, not reused: each dynamic `bytes` is padded to a 32-byte
    multiple, and D3's signature (6736 B) needs 16 bytes of pad while D5's (9184 B) needs none.

  * execution is bounded BELOW as

        execution(D5)  >=  execution(D3) - sampleInBall(D3) - decodeZ(D3) + deltaAbsorb

    Every stage of LASVerifyOpt is a deterministic loop over n, ell, n+ell or n·ell EXCEPT two:
    `SampleInBall` (rejection loop) and `_decodeZ` (branches on coefficient value, `if gt(f,
    137935)`). Those two could in principle cost LESS at D5 despite kappa and n+ell growing, so
    their whole measured D3 cost is subtracted. The NTT, pointwise, packing and preimage-assembly
    stages contain only loop-counter comparisons — verified by reading ZKNOX_NTT_dilithium.sol
    and _mulInto — so at D5 they run strictly more iterations of identical code and cannot
    shrink. Their growth is counted as ZERO, which is conservative.

    `deltaAbsorb` comes from evm/test/LASShakeGrowth.t.sol, taken against a fixed-size arena.
    Memory-expansion gas cancels exactly in the difference; the differing pad-tail PATH does NOT
    cancel — it is simply MEASURED, since whatever it costs falls inside the difference.
    ⚠ It is the exact difference of two `absorbPad` CALLS and nothing more — not the whole
    verifier hash/check delta. LASGasBreakdown's named SHAKE stage is `init() + absorbPad()` and
    stops there; this isolates the absorbPad difference. The verifier additionally runs
    `_digestMatches`, longer at D5 (six lanes against eight), and that positive term is not
    credited. No claim is made about TOTAL verifier growth, which is not provably positive —
    that is exactly why the two data-dependent stages are subtracted whole above.

  Rearranged, D5 is shown NOT to fit exactly when

        deltaAbsorb  >=  slack + sampleInBall(D3) + decodeZ(D3)

  where `slack` is what the D5-favourable calldata leaves against the cap with execution frozen.

Usage:
  forge test --match-contract LASShakeGrowthTest -vv > /tmp/shakegrowth.log   # no --gas-report
  python3 scripts/derive_onchain_d5_bound.py --growth-log /tmp/shakegrowth.log
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EV_TX = ROOT / "evidence" / "onchain_onetx" / "latest"
EV_GAS = ROOT / "evidence" / "onchain" / "latest" / "gas_report.log"

TX_GAS_CAP = 16_777_216  # EIP-7825, per transaction
INTRINSIC = 21_000
RING_N = 256
POLY_PACKED_BYTES = RING_N * 4

# (name, n, ell, kappa, ctilde_bytes, static-asserted signature bytes)
SETS = {
    "D3": ("Simplified Dilithium-III", 6, 5, 49, 48, 6736),
    "D5": ("Simplified Dilithium-V", 8, 7, 60, 64, 9184),
}


def sig_bytes(n: int, ell: int, kappa: int, ctilde: int) -> int:
    bound = kappa * RING_N * (n + ell) - kappa
    z_bits = (2 * bound).bit_length()
    return ctilde + (n + ell) * RING_N * z_bits // 8


def payload_bytes(n: int, ell: int, sig: int) -> int:
    """signature + aHat + tHat + tPacked."""
    return sig + (n * ell + 2 * n) * POLY_PACKED_BYTES


def abi_padding(n: int, ell: int, sig: int) -> int:
    """Each dynamic `bytes` is padded up to a 32-byte multiple. NOT parameter-independent."""
    parts = (sig, n * ell * POLY_PACKED_BYTES, n * POLY_PACKED_BYTES, n * POLY_PACKED_BYTES, 32)
    return sum((-x) % 32 for x in parts)


def grep_int(text: str, pattern: str, label: str) -> int:
    m = re.search(pattern, text)
    if not m:
        raise SystemExit(f"could not find {label} (pattern {pattern!r})")
    return int(m.group(1).replace(",", ""))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--growth-log", type=Path, default=None,
                    help="output of `forge test --match-contract LASShakeGrowthTest -vv`")
    args = ap.parse_args()

    for p in (EV_TX / "claim.calldata", EV_TX / "claim_receipt.json", EV_GAS):
        if not p.exists():
            print(f"missing measured input: {p}", file=sys.stderr)
            return 2

    # ---- measured anchor: the real client receipt --------------------------
    raw = (EV_TX / "claim.calldata").read_text().strip()
    cd = bytes.fromhex(raw[2:] if raw.startswith("0x") else raw)
    zero_d3 = cd.count(0)
    nonzero_d3 = len(cd) - zero_d3
    tokens_d3 = zero_d3 + 4 * nonzero_d3
    gas_used = int(str(json.loads((EV_TX / "claim_receipt.json").read_text())["gasUsed"]), 0)

    execution_d3 = gas_used - INTRINSIC - 4 * tokens_d3
    if 10 * tokens_d3 >= 4 * tokens_d3 + execution_d3:
        print("EIP-7623 floor branch bound — recovering execution this way is invalid", file=sys.stderr)
        return 1

    # ---- measured stage attribution ---------------------------------------
    gas_log = EV_GAS.read_text()
    sample_in_ball = grep_int(gas_log, r"OPTIMISED stage: SampleInBall \(challenge\) (\d+)", "SampleInBall stage")
    decode_z = grep_int(gas_log, r"OPTIMISED stage: decode z \+ norm gate (\d+)", "decode-z stage")
    shake_preimage = grep_int(gas_log, r"SHAKE256 over the ([\d,]+)-byte preimage", "SHAKE preimage length")

    # ---- model check: reproduce the static-asserted wire sizes -------------
    for key, (name, n, ell, kappa, ct, expect) in SETS.items():
        got = sig_bytes(n, ell, kappa, ct)
        if got != expect:
            print(f"MODEL CHECK FAILED: {name} signature {got} != {expect}", file=sys.stderr)
            return 1
    _, n3, l3, k3, ct3, s3 = SETS["D3"]
    _, n5, l5, k5, ct5, s5 = SETS["D5"]
    if shake_preimage != 2 * n3 * POLY_PACKED_BYTES + 32:
        print(f"MODEL CHECK FAILED: measured preimage {shake_preimage} != "
              f"derived {2 * n3 * POLY_PACKED_BYTES + 32}", file=sys.stderr)
        return 1

    # ---- D5 calldata, pushed the way that favours D5 ----------------------
    framing = len(cd) - payload_bytes(n3, l3, s3)  # selector, offsets, lengths, message, padding
    bytes_d5 = payload_bytes(n5, l5, s5) + framing - abi_padding(n3, l3, s3) + abi_padding(n5, l5, s5)
    added = bytes_d5 - len(cd)
    if added < 0:
        print("unexpected: D5 calldata smaller than D3", file=sys.stderr)
        return 1
    tokens_d5 = (zero_d3 + added) + 4 * nonzero_d3  # every added byte assumed ZERO
    frozen_total = INTRINSIC + max(4 * tokens_d5 + execution_d3, 10 * tokens_d5)
    slack = TX_GAS_CAP - frozen_total
    threshold = slack + sample_in_ball + decode_z

    print("=" * 74)
    print("MEASURED")
    print("=" * 74)
    print(f"  D3 client receipt        : {gas_used} gas   ({EV_TX.relative_to(ROOT)})")
    print(f"  EIP-7825 cap             : {TX_GAS_CAP}   headroom {TX_GAS_CAP - gas_used}")
    print(f"  D3 calldata              : {len(cd)} B ({zero_d3} zero / {nonzero_d3} non-zero), "
          f"tokens {tokens_d3}, charge {4 * tokens_d3}")
    print(f"  D3 execution (recovered) : {execution_d3}")
    print(f"  D3 SampleInBall stage    : {sample_in_ball}   [data-dependent -> subtracted whole]")
    print(f"  D3 decode-z stage        : {decode_z}   [data-dependent -> subtracted whole]")
    print()
    print("=" * 74)
    print("DERIVED — D5 calldata pushed the way that FAVOURS fitting")
    print("=" * 74)
    print(f"  ABI padding D3 -> D5     : {abi_padding(n3, l3, s3)} -> {abi_padding(n5, l5, s5)} B")
    print(f"  D5 calldata              : {bytes_d5} B ({added:+d} vs D3, every added byte assumed ZERO)")
    print(f"  D5 calldata charge       : {4 * tokens_d5} ({4 * tokens_d5 - 4 * tokens_d3:+d})")
    print(f"  total with execution FROZEN at D3: {frozen_total}  -> {slack} gas UNDER the cap")
    print()
    print(f"  absorbPad growth needed to close it:")
    print(f"      deltaAbsorb  >=  slack {slack} + SampleInBall {sample_in_ball} + decodeZ {decode_z}")
    print(f"      deltaAbsorb  >=  {threshold}")
    print()

    if args.growth_log is None or not args.growth_log.exists():
        print("=" * 74)
        print("VERDICT: UNRESOLVED — no absorbPad growth measurement supplied.")
        print("=" * 74)
        print("  Run, WITHOUT --gas-report (the inspector inflates gasleft() deltas):")
        print("    cd evm && forge test --match-contract LASShakeGrowthTest -vv > /tmp/shakegrowth.log")
        print("    python3 scripts/derive_onchain_d5_bound.py --growth-log /tmp/shakegrowth.log")
        print("  Until then D5 stays UNRESOLVED. Do not write that it fits, fails, or needs")
        print("  more optimisation — none of those is supported.")
        return 0

    text = args.growth_log.read_text()
    delta_absorb = grep_int(text, r"absorbPad DELTA gas D5 minus D3\D*(\d+)", "absorbPad delta")
    per_permute = None
    m = re.search(r"permute MARGINAL gas per call\D*(\d+)", text)
    if m:
        per_permute = int(m.group(1))

    execution_d5_lb = execution_d3 - sample_in_ball - decode_z + delta_absorb
    total_d5_lb = INTRINSIC + max(4 * tokens_d5 + execution_d5_lb, 10 * tokens_d5)

    print("=" * 74)
    print("MEASURED absorbPad GROWTH  (exact difference of two absorbPad calls — see script header)")
    print("=" * 74)
    print(f"  deltaAbsorb (D5 - D3)    : {delta_absorb}   [fixed arena: memory expansion cancels, tail path measured]")
    if per_permute is not None:
        print(f"  cross-check 30 x permute : {30 * per_permute}   (permute {per_permute} gas/call)")
    print()
    print(f"  execution(D5) lower bound: {execution_d5_lb}")
    print(f"  total(D5) lower bound    : {total_d5_lb}  vs cap {TX_GAS_CAP}  ({total_d5_lb - TX_GAS_CAP:+d})")
    print()
    print("=" * 74)
    if delta_absorb >= threshold:
        print("VERDICT: D5 EXCEEDS ONE TRANSACTION — DERIVED: a lower bound computed from")
        print("         measured quantities. The bound is arithmetic, not a measurement.")
        print("=" * 74)
        print(f"  Measured absorbPad growth {delta_absorb} clears the {threshold} needed, "
              f"by {delta_absorb - threshold} gas ({delta_absorb / threshold:.1f}x).")
        print("  Wording that is supported: 'derived from measured quantities, one transaction")
        print("  is exceeded at Dilithium-V'. NOT supported: calling it measured at D5, or")
        print("  quoting a D5 gas total — only a lower bound was established, not a value.")
    else:
        print("VERDICT: UNRESOLVED — the measured growth does not close the gap.")
        print("=" * 74)
        print(f"  Measured absorbPad growth {delta_absorb} is short of the {threshold} needed "
              f"by {threshold - delta_absorb} gas.")
        print("  D5 stays UNRESOLVED. The remaining route is to measure D5 outright, which")
        print("  needs LASVerifierOpt's assembly unpacker re-instantiated (seven hard-coded D3")
        print("  sites) and D5 test vectors generated.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
