"""This project's Bitcoin transaction size model — BIP141/BIP144 field arithmetic.

DELIBERATELY INDEPENDENT OF BITCOIN CORE. The point of the carriage experiment is to check
THIS model — the one `scripts/gen_bitcoin_tx_data.py` uses for every byte count in the
report — against a real client's `vsize`/`weight`. Computing the sizes with a Core helper
and comparing them to Core's consensus code would test nothing.

No imports beyond the standard library, and none from Core's test framework, so the checker
can run against a decoded transaction without a source tree present.
"""
import math


def compact_size(n):
    """Bitcoin's variable-length integer width, per the raw transaction format."""
    if n < 0xFD:
        return 1
    if n <= 0xFFFF:
        return 3
    if n <= 0xFFFFFFFF:
        return 5
    return 9


def model_sizes(script_sig_lens, script_pubkey_lens, witness_stacks):
    """Sizes from a transaction's ACTUAL shape.

    script_sig_lens    : per input, the scriptSig byte-length (0 for a segwit spend)
    script_pubkey_lens : per output, the scriptPubKey byte-length
    witness_stacks     : per input, the list of witness item byte-lengths ([] if none)

    Returns base/witness/total sizes, weight = 3·base + total, and vsize = ceil(weight/4).
    The witness section (marker, flag, counts, items) is present only when at least one
    input has a witness, which is what BIP144 specifies and what the node will report.
    """
    base = 4 + compact_size(len(script_sig_lens))
    for slen in script_sig_lens:
        base += 36 + compact_size(slen) + slen + 4   # outpoint + scriptSig + sequence
    base += compact_size(len(script_pubkey_lens))
    for slen in script_pubkey_lens:
        base += 8 + compact_size(slen) + slen        # value + scriptPubKey
    base += 4                                        # nLockTime

    any_witness = any(len(s) > 0 for s in witness_stacks)
    wit = 0
    if any_witness:
        wit = 2  # marker + flag
        for stack in witness_stacks:
            wit += compact_size(len(stack))
            for ilen in stack:
                wit += compact_size(ilen) + ilen

    total = base + wit
    weight = base * 3 + total
    return {
        "base_size": base,
        "witness_size": wit,
        "total_size": total,
        "weight": weight,
        "vsize": math.ceil(weight / 4),
    }


def shape_from_decoded(decoded):
    """Extract the shape `model_sizes` needs from a `decoderawtransaction` result.

    Reads only field LENGTHS, never the node's own `vsize`/`weight` — those are the answer
    being checked, and reading them here would make the comparison circular.
    """
    script_sigs, witnesses = [], []
    for vin in decoded["vin"]:
        hexsig = vin.get("scriptSig", {}).get("hex", "")
        script_sigs.append(len(hexsig) // 2)
        witnesses.append([len(w) // 2 for w in vin.get("txinwitness", [])])
    spks = [len(vout["scriptPubKey"]["hex"]) // 2 for vout in decoded["vout"]]
    return script_sigs, spks, witnesses
