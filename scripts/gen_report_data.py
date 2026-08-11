#!/usr/bin/env python3
"""
gen_report_data.py -- generate report/latex/generated/*.tex from captured evidence.

The report must always reflect the LATEST benchmark run, so every number the
LaTeX cites inline (overhead percentages, sizes, rejection rates, classical
baseline, Rust-port timings, run provenance) is emitted here as a \\newcommand
macro, and every data table is emitted as a complete tabular body that the
chapters \\input. Nothing in report/latex/generated/ is ever edited by hand.

Inputs (ALL read-only; this script never runs a benchmark and never invents a
number -- every value is parsed from a captured evidence file):

  evidence/latest/tables/   parameter_sets.csv, primary_timing.csv,
                            adaptor_overhead.csv, rejection_sampling.csv,
                            communication_components.csv
  evidence/latest/logs/     classical.log (classical adaptor baseline),
                            fair_l3.log (protocol line), serialization_tests.log
  evidence/latest/metadata.txt          (run id, git, compiler, CPU)
  rust/fips204-las/bench_levels_rust.log   (Rust protocol driver, mirrors C)
  rust/fips204-las/size_report_rust.log    (Rust packed sizes + C cross-check)
  rust/fips204-las/bench_las_criterion.log (Criterion.rs statistical harness)

Outputs (report/latex/generated/):
  benchmacros.tex        every inline number as a macro (+ provenance)
  tab_timing.tex         per-operation timing, all four parameter sets
  tab_overhead_target.tex  adaptor overhead at the target setting
  tab_components.tex     component sizes across the three simplified settings
  tab_complete_target.tex  object catalogue at the target setting
  tab_classical.tex      classical ECDSA adaptor vs LAS (Simplified Dilithium-II)
  tab_rust.tex           C implementation vs Rust port at the target setting
  tab_rejstats.tex       rejection-sampling statistics: model vs every sample

Validity gates (fail loudly rather than emit a wrong report):
  - all four parameter sets and all operations present in the C evidence;
  - the Rust driver's parameter set matches the C target setting (n, ell, kappa);
  - the Rust packed sizes equal the C target-setting sizes byte-for-byte;
  - each overhead percentage re-derives from the timing means within rounding.
"""
import argparse
import csv
import re
import sys
from pathlib import Path

LEVELS = ["paper", "L2", "L3", "L5"]
TARGET = "L3"                          # Simplified Dilithium-III == project target
DISPLAY = {
    "paper": "LAS-2020/845 reference",
    "L2": "Simplified Dilithium-II",
    "L3": "Simplified Dilithium-III",
    "L5": "Simplified Dilithium-V",
}
OPS = ["Setup", "KeyGen", "Sign", "Verify", "PreSign", "PreVerify", "Adapt", "Ext"]
PAIRS = ["PreSign vs Sign", "PreVerify vs Verify", "Adapt vs Verify"]


def die(msg):
    sys.exit("gen_report_data.py: ERROR: %s" % msg)


# ---------------------------------------------------------------------------
# parsers (evidence in, python structures out)
# ---------------------------------------------------------------------------

def read_csv(d, name):
    p = d / name
    if not p.exists():
        die("missing evidence CSV %s (run scripts/run_benchmark_suite.sh)" % p)
    with open(p, newline="") as f:
        return list(csv.DictReader(f))


def load_c_evidence(tables_dir):
    params = {r["level"]: r for r in read_csv(tables_dir, "parameter_sets.csv")}
    timing = {}
    for r in read_csv(tables_dir, "primary_timing.csv"):
        op = "Ext" if r["operation"].startswith("Ext") else r["operation"]
        timing.setdefault(r["level"], {})[op] = (float(r["mean_us"]), float(r["sd_us"]))
    over = {}
    for r in read_csv(tables_dir, "adaptor_overhead.csv"):
        if r["overhead_pct"]:
            over.setdefault(r["level"], {})[r["pair"]] = float(r["overhead_pct"])
    rej = {}
    for r in read_csv(tables_dir, "rejection_sampling.csv"):
        rej.setdefault(r["level"], {})[r["operation"]] = r
    comm = {}
    for r in read_csv(tables_dir, "communication_components.csv"):
        comm.setdefault(r["level"], {})[r["component"]] = int(r["bytes"])

    for lvl in LEVELS:
        if lvl not in params or lvl not in timing or lvl not in over:
            die("parameter set '%s' missing from the C evidence tables" % lvl)
        for op in OPS:
            if op not in timing[lvl]:
                die("operation '%s' missing from primary_timing.csv at %s" % (op, lvl))
        for pair in PAIRS:
            if pair not in over[lvl]:
                die("pair '%s' missing from adaptor_overhead.csv at %s" % (pair, lvl))
            a, _ = timing[lvl][pair.split(" vs ")[0]]
            b, _ = timing[lvl][pair.split(" vs ")[1]]
            rederived = 100.0 * (a - b) / b
            if abs(rederived - over[lvl][pair]) > 0.15:
                die("overhead gate: %s at %s is %.1f%% in the CSV but re-derives "
                    "to %.1f%% from primary_timing.csv" %
                    (pair, lvl, over[lvl][pair], rederived))
    return params, timing, over, rej, comm


def parse_metadata(path):
    if not path.exists():
        die("missing %s" % path)
    t = path.read_text(errors="replace")

    def g(rx):
        m = re.search(rx, t, re.M)
        return m.group(1).strip() if m else None
    meta = {
        "run_id": g(r"^run_id\s*:\s*(\S+)"),
        "git_short": g(r"^git_short\s*:\s*(\S+)"),
        "timestamp": g(r"^timestamp\s*:\s*(.+)$"),
        "compiler": g(r"^compiler\s*:\s*(.+)$"),
        "cpu": g(r"^cpu\s*:\s*(.+)$"),
    }
    for k, v in meta.items():
        if not v:
            die("could not parse '%s' from %s" % (k, path))
    return meta


def parse_fair_protocol(path):
    """The repetition scheme, from the fair-benchmark log header."""
    if not path.exists():
        die("missing %s" % path)
    t = path.read_text(errors="replace")
    m = re.search(r"(\d+)\s+repetitions x (\d+)\s*\(sign-class\)\s*/\s*(\d+)\s*"
                  r"\(verify-class\)", t)
    if not m:
        die("could not parse the repetition protocol from %s" % path)
    return {"reps": m.group(1), "iters_sign": m.group(2), "iters_verify": m.group(3)}


def parse_packed_overhead(path):
    """End-to-end (packed) adaptor overhead --- incl. validating unpack + pack ---
    from the fair log's TIER-2 packed-boundary section. This is the full-protocol
    cost a wire / on-chain consumer pays: each adaptor op additionally decodes the
    pk-sized statement Y, so it is larger than the core-tier overhead above (which
    isolates the pure adaptor arithmetic). Returns {op: pct} for PreSign/PreVerify/
    Adapt, or dies loudly if the packed tier is absent."""
    if not path.exists():
        die("missing %s" % path)
    t = path.read_text(errors="replace")
    out = {}
    for rx, op in ((r"PreSign_packed\s+vs\s+Sign_packed", "PreSign"),
                   (r"PreVerify_packed\s+vs\s+Verify_packed", "PreVerify"),
                   (r"Adapt_packed\s+vs\s+Verify_packed", "Adapt")):
        m = re.search(rx + r"[^()]*\(([+-][0-9.]+)%\)", t)
        if not m:
            die("could not parse the packed (end-to-end) overhead '%s' from %s "
                "(TIER-2 packed boundary)" % (op, path))
        out[op] = float(m.group(1))
    return out


def parse_packed_timing(path):
    """Per-operation mean +/- SD at the TIER-2 (full-protocol / packed) boundary
    from a fair log: packed bytes in / packed bytes out, including the validating
    decode and the encode.  Complements the core-tier numbers from
    primary_timing.csv so the report tables can present BOTH measurement
    boundaries side by side.  Returns {op: (mean_us, sd_us)}; dies loudly if any
    operation's packed line is absent (the tier rule requires both tiers)."""
    if not path.exists():
        die("missing %s" % path)
    t = path.read_text(errors="replace")
    out = {}
    for op in ["KeyGen", "Sign", "Verify", "PreSign", "PreVerify", "Adapt", "Ext"]:
        m = re.search(r"^\s*%s_packed\s+([0-9.]+)\s*\+/-\s*([0-9.]+)\s*us"
                      % re.escape(op), t, re.M)
        if not m:
            die("could not parse '%s_packed' from %s (TIER-2 section)"
                % (op, path))
        out[op] = (float(m.group(1)), float(m.group(2)))
    return out


def parse_tamper(path):
    if not path.exists():
        die("missing %s" % path)
    m = re.search(r"tamper: low-bit flip of all (\d+) bytes rejected",
                  path.read_text(errors="replace"))
    if not m:
        die("could not parse the tamper-test count from %s" % path)
    return int(m.group(1))


def parse_classical(path):
    if not path.exists():
        die("missing %s (classical adaptor baseline; see README.md for the "
            "one-time secp256k1-zkp clone)" % path)
    t = path.read_text(errors="replace")

    def g(label):
        m = re.search(r"%s[^0-9]*([0-9]+(?:\.[0-9]+)?)\s*\+/-\s*"
                      r"([0-9]+(?:\.[0-9]+)?)\s*us" % re.escape(label), t)
        if not m:
            die("could not parse '%s' from classical.log" % label)
        return (float(m.group(1)), float(m.group(2)))

    def gi(rx, what):
        m = re.search(rx, t)
        if not m:
            die("could not parse %s from classical.log" % what)
        return int(m.group(1))

    proto = re.search(r"(\d+) runs x (\d+) iters/op", t)
    if not proto:
        die("could not parse the classical repetition protocol from classical.log")
    return {
        "KeyGen": g("KeyGen"), "Sign": g("Sign"), "Verify": g("Verify"),
        "PreSign": g("PreSign"), "PreVerify": g("PreVerify"),
        "Adapt": g("Adapt"), "Ext": g("Ext"),
        "pk": gi(r"pk/statement\s+(\d+)\s*B", "pk size"),
        "sk": gi(r"sk/witness\s+(\d+)\s*B", "sk size"),
        "sig": gi(r"sig\s+(\d+)\s*B", "sig size"),
        "presig": gi(r"pre-sig \(adaptor\)\s+(\d+)\s*B", "pre-sig size"),
        "reps": proto.group(1), "iters": proto.group(2),
    }


def parse_rust_driver(path):
    """rust/fips204-las/bench_levels_rust.log -- mirrors the C driver exactly."""
    if not path.exists():
        die("missing %s (run scripts/run_rust_bench_suite.sh)" % path)
    t = path.read_text(errors="replace")

    m = re.search(r"parameter set: .*\(n=(\d+), ell=(\d+), kappa=(\d+), "
                  r"gamma=(\d+)\)", t)
    if not m:
        die("could not parse the Rust parameter set from %s" % path)
    params = {"n": m.group(1), "ell": m.group(2), "kappa": m.group(3),
              "gamma": m.group(4)}

    m = re.search(r"packed sizes: public key (\d+) B \| secret key / witness "
                  r"(\d+) B \| signature (\d+) B", t)
    if not m:
        die("could not parse the Rust packed sizes from %s" % path)
    sizes = {"pk": int(m.group(1)), "sk": int(m.group(2)), "sig": int(m.group(3))}

    timing = {}
    canon = {"KeyGen": "KeyGen", "Sign": "Sign", "Verify": "Verify",
             "PreSign": "PreSign", "PreVerify": "PreVerify",
             "Adapt (incl. PreVerify)": "Adapt", "Extract": "Ext"}
    for m in re.finditer(r"^Algorithm [12]\s+(.+?)\s{2,}([0-9.]+)\s+([0-9.]+)\s*$",
                         t, re.M):
        name = m.group(1).strip()
        if name in canon:
            timing[canon[name]] = (float(m.group(2)), float(m.group(3)))
    for op in ["KeyGen", "Sign", "Verify", "PreSign", "PreVerify", "Adapt", "Ext"]:
        if op not in timing:
            die("operation '%s' missing from the Rust driver log" % op)

    m = re.search(r"adaptor overhead \(per operation\): PreSign vs Sign "
                  r"([+-][0-9.]+)% \| PreVerify vs Verify ([+-][0-9.]+)% \| "
                  r"Adapt vs Verify ([+-][0-9.]+)%", t)
    if not m:
        die("could not parse the Rust overhead summary from %s" % path)
    over = {"PreSign vs Sign": float(m.group(1)),
            "PreVerify vs Verify": float(m.group(2)),
            "Adapt vs Verify": float(m.group(3))}

    m = re.search(r"rejection sampling \(measured via attempt counters\): base "
                  r"([0-9.]+) attempts/signature \(acceptance ([0-9.]+)%\) \| "
                  r"adaptor ([0-9.]+) attempts/pre-signature \(acceptance "
                  r"([0-9.]+)%\)", t)
    if not m:
        die("could not parse the Rust rejection summary from %s" % path)
    rej = {"att_base": m.group(1), "acc_base": m.group(2),
           "att_las": m.group(3), "acc_las": m.group(4)}

    m = re.search(r"protocol: (\d+) repetitions, (\d+) iterations/sign-class op, "
                  r"(\d+) iterations/verify-class op", t)
    if not m:
        die("could not parse the Rust repetition protocol from %s" % path)
    proto = {"reps": m.group(1), "iters_sign": m.group(2),
             "iters_verify": m.group(3)}
    return params, sizes, timing, over, rej, proto


def parse_rust_packed_overhead(path):
    """Rust protocol driver's TIER-2 (full-protocol / packed) adaptor overhead
    line -- the Rust twin of parse_packed_overhead, so the report can state that
    the full-protocol tier reproduces (positive, same ordering) under a second
    compiler.  Returns {op: pct}."""
    if not path.exists():
        die("missing %s (run scripts/run_rust_bench_suite.sh)" % path)
    t = path.read_text(errors="replace")
    m = re.search(r"packed adaptor overhead \(per operation\): "
                  r"PreSign vs Sign ([+-][0-9.]+)% \| "
                  r"PreVerify vs Verify ([+-][0-9.]+)% \| "
                  r"Adapt vs Verify ([+-][0-9.]+)%", t)
    if not m:
        die("could not parse the Rust packed adaptor overhead from %s "
            "(TIER 2 section)" % path)
    return {"PreSign": float(m.group(1)), "PreVerify": float(m.group(2)),
            "Adapt": float(m.group(3))}


def parse_rust_sizes(path):
    if not path.exists():
        die("missing %s (run scripts/run_rust_bench_suite.sh)" % path)
    t = path.read_text(errors="replace")
    if "ALL SIZE CHECKS PASSED" not in t:
        die("%s does not end in ALL SIZE CHECKS PASSED -- refusing to use it" % path)
    m = re.search(r"C pinned KAT digest ([0-9a-f]{8})…([0-9a-f]{4})", t)
    if not m:
        die("could not parse the pinned KAT digest from %s" % path)
    return {"kat_head": m.group(1), "kat_tail": m.group(2)}


def parse_criterion(path):
    """Criterion.rs terminal log: '<group>/<op>' then 'time: [lo mid hi]'."""
    if not path.exists():
        die("missing %s (run scripts/run_rust_bench_suite.sh)" % path)
    t = path.read_text(errors="replace")
    unit = {"ns": 1e-3, "µs": 1.0, "us": 1.0, "ms": 1e3, "s": 1e6}
    canon = {"KeyGen": "KeyGen", "Sign": "Sign", "Verify": "Verify",
             "PreSign": "PreSign", "PreVerify": "PreVerify",
             "Adapt (including its internal PreVerify)": "Adapt",
             "Extract": "Ext"}
    out = {}
    rx = re.compile(
        r"^Algorithm [12] - [^/\n]+/(.+?)\n\s+time:\s+\[([0-9.]+) (\S+) "
        r"([0-9.]+) (\S+) ([0-9.]+) (\S+)\]", re.M)
    for m in rx.finditer(t):
        name = m.group(1).strip()
        if name not in canon:
            continue
        lo = float(m.group(2)) * unit[m.group(3)]
        mid = float(m.group(4)) * unit[m.group(5)]
        hi = float(m.group(6)) * unit[m.group(7)]
        out[canon[name]] = (lo, mid, hi)
    for op in ["KeyGen", "Sign", "Verify", "PreSign", "PreVerify", "Adapt", "Ext"]:
        if op not in out:
            die("operation '%s' missing from the Criterion log" % op)
    m = re.search(r"Collecting (\d+) samples", t)
    if not m:
        die("could not parse the Criterion sample count from %s" % path)
    return out, m.group(1)


# ---------------------------------------------------------------------------
# formatting (one style everywhere: >=100 -> integer, else one decimal)
# ---------------------------------------------------------------------------

def num(x):
    return "%.0f" % x if x >= 100 else "%.1f" % x


def mean_sd(pair):
    """One precision per cell, chosen by the mean; a nonzero SD never prints
    as zero (it gains decimals instead, e.g. 43.6 +/- 0.03)."""
    m, s = pair
    dec = 0 if m >= 100 else 1
    s_txt = "%.*f" % (dec, s)
    if s > 0 and float(s_txt) == 0.0:
        s_txt = "%.2f" % s
    return "$%.*f \\pm %s$" % (dec, m, s_txt)


def pct(x):
    return "%.1f" % x


def header(sources, meta):
    lines = ["% AUTO-GENERATED by scripts/gen_report_data.py -- DO NOT EDIT.",
             "% Regenerate with scripts/sync_report.sh (runs after every benchmark suite).",
             "% Evidence run: {}  (git {})".format(meta["run_id"], meta["git_short"])]
    for s in sources:
        lines.append("% Source: " + s)
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# emitters
# ---------------------------------------------------------------------------

def parse_mldsa(ev_dir):
    """The ML-DSA adaptor experiment (evidence/mldsa_hint/latest).

    Separate evidence stream from evidence/latest: it measures a DIFFERENT
    construction (LAS on FIPS 204 as specified) and its numbers must never be
    mixed with the simplified scheme's.  Returns None when the experiment has
    not been run, so the report degrades to "not available" rather than to a
    stale number.
    """
    hint = ev_dir / "mldsa_hint_mode3.log"
    contract = ev_dir / "mldsa_contract_mode3.log"
    compare = ev_dir / "mldsa_compare_mode3.log"
    if not (hint.exists() and contract.exists() and compare.exists()):
        return None
    h, c, k = (f.read_text() for f in (hint, contract, compare))
    d = {}

    # naive vs repaired variant: the two P4 rows, in file order (V0 then V1)
    p4 = re.findall(r"P4 stock Verify ACCEPTS adapted signature\s+<-- decisive\s+"
                    r"(\d+)\s*/\s*(\d+)", h)
    p1 = re.findall(r"P1 PreVerify accepts pre-signature\s+(\d+)\s*/\s*(\d+)", h)
    if len(p4) < 2 or len(p1) < 2:
        die("could not parse the P1/P4 rows from %s" % hint)
    d["naive_p1"], d["iters"] = p1[0][0], p1[0][1]
    d["repaired_p4"] = p4[1][0]
    if p4[1][0] != p4[1][1]:
        die("the ML-DSA repaired variant did not hold P4 on every iteration "
            "(%s/%s); the report must not claim it does" % p4[1])

    m = re.search(r"PASS: (\d+)/(\d+) contract items hold", c)
    if not m:
        die("the ML-DSA contract did not PASS in %s" % contract)
    d["contract"] = "%s/%s" % (m.group(1), m.group(2))

    m = re.search(r"PreSign vs Sign \(/attempt\)\s+([-\d.]+)% \+/-\s+([\d.]+)"
                  r"\s+([-\d.]+)% \+/-\s+([\d.]+)", k)
    if not m:
        die("could not parse the paired PreSign overhead from %s" % compare)
    d["ov_simp"], d["ov_mldsa"] = m.group(1), m.group(3)

    m = re.search(r"attempts/Sign\s+([\d.]+)\s+([\d.]+)", k)
    d["att_simp"], d["att_mldsa"] = m.group(1), m.group(2)

    for key, label in (("sig", "signature"), ("stmt", r"statement Y"),
                       ("payload", r"swap payload \(sig \+ Y\)")):
        m = re.search(label + r"\s+(\d+)\s+(\d+)\s+([\d.]+)x", k)
        if not m:
            die("could not parse the '%s' size row from %s" % (label, compare))
        d[key + "_simp"], d[key + "_mldsa"], d[key + "_ratio"] = m.groups()
    return d


def emit_macros(out, meta, proto, params, timing, over, rej, comm, classical,
                rust_params, rust_sizes, rust_timing, rust_over, rust_rej,
                rust_proto, rust_kat, crit, crit_samples, tamper, packed_over,
                rust_packed_over, onchain, packed_l2, mldsa=None, onetx=None,
                pi_params=None):
    t = timing[TARGET]
    o = over[TARGET]
    c = comm[TARGET]
    c2 = comm["L2"]
    r = rej[TARGET]

    ov_max_all = max(over[lvl][p] for lvl in LEVELS for p in PAIRS)
    setup_means = [timing[lvl]["Setup"][0] for lvl in LEVELS]
    ratio_sig = c2["signature (c,z)"] / classical["sig"]
    ratio_pk = c2["pk = t"] / classical["pk"]
    # Tier-matched, exactly as tab_classical does it: the classical library's
    # single hybrid boundary is core-like for KeyGen/Sign/Verify and packed-like
    # for the four adaptor operations, so each LAS operation is taken at the tier
    # that matches it.  Keeping this in step with the table is what stops the
    # prose and the table quoting two different factors for the same operation.
    _core_like = {"KeyGen", "Sign", "Verify"}
    time_ratios = {op: (timing["L2"][op][0] if op in _core_like
                        else packed_l2[op][0]) / classical[op][0]
                   for op in ["KeyGen", "Sign", "Verify", "PreSign", "PreVerify",
                              "Adapt", "Ext"]}
    slow_op = max(time_ratios, key=time_ratios.get)
    devs = {op: 100.0 * (rust_timing[op][0] - timing[TARGET][op][0])
            / timing[TARGET][op][0]
            for op in ["KeyGen", "Sign", "Verify", "PreSign", "PreVerify",
                       "Adapt", "Ext"]}
    dev_max_op = max(devs, key=lambda k: abs(devs[k]))

    def z_pct(lvl):
        return "%.1f" % (100.0 * comm[lvl]["z"] / comm[lvl]["signature (c,z)"])

    m = []
    m.append(("benchRunId", meta["run_id"].replace("_", "\\_")))
    m.append(("benchGitShort", meta["git_short"]))
    m.append(("benchReps", proto["reps"]))
    m.append(("benchItersSign", proto["iters_sign"]))
    m.append(("benchItersVerify", proto["iters_verify"]))
    m.append(("tamperFlips", str(tamper)))
    # target-setting adaptor overhead (THE headline numbers)
    m.append(("ovPreSign", pct(o["PreSign vs Sign"])))
    m.append(("ovPreVerify", pct(o["PreVerify vs Verify"])))
    m.append(("ovAdapt", pct(o["Adapt vs Verify"])))
    m.append(("ovMaxAll", pct(ov_max_all)))
    # full-protocol (packed / end-to-end) overhead: incl. validating unpack + pack.
    # Larger than the core figures because each adaptor op decodes the pk-sized
    # statement Y -- the serialization cost, not the adaptor arithmetic.
    m.append(("packedOvPreSign", pct(packed_over["PreSign"])))
    m.append(("packedOvPreVerify", pct(packed_over["PreVerify"])))
    m.append(("packedOvAdapt", pct(packed_over["Adapt"])))
    m.append(("extMean", num(t["Ext"][0])))
    m.append(("setupMin", "%.0f" % min(setup_means)))
    m.append(("setupMax", "%.0f" % max(setup_means)))
    # rejection sampling at the target setting (measured via attempt counters)
    m.append(("rejAttBase", r["Base Sign"]["avg_attempts"]))
    m.append(("rejAttLas", r["LAS PreSign"]["avg_attempts"]))
    m.append(("rejAccBase", r["Base Sign"]["acceptance_pct"]))
    m.append(("rejAccLas", r["LAS PreSign"]["acceptance_pct"]))
    # packed sizes
    m.append(("sigBytesTwo", str(c2["signature (c,z)"])))
    m.append(("pkBytesTwo", str(c2["pk = t"])))
    m.append(("sigBytesTarget", str(c["signature (c,z)"])))
    m.append(("pkBytesTarget", str(c["pk = t"])))
    m.append(("stmtBytesTarget", str(c["Y = t'"])))
    m.append(("witBytesTarget", str(c["r'"])))
    m.append(("zPctTwo", z_pct("L2")))
    m.append(("zPctTarget", z_pct(TARGET)))
    m.append(("zPctFive", z_pct("L5")))
    # classical baseline (+ derived ratios; both factors are measured)
    m.append(("clReps", classical["reps"]))
    m.append(("clIters", classical["iters"]))
    m.append(("clSigBytes", str(classical["sig"])))
    m.append(("clPkBytes", str(classical["pk"])))
    m.append(("clPreSigBytes", str(classical["presig"])))
    m.append(("clRatioSig", "%.0f" % ratio_sig))
    m.append(("clRatioPk", "%.0f" % ratio_pk))
    m.append(("clRatioTimeMax", "%.0f" % time_ratios[slow_op]))
    m.append(("clRatioTimeMaxOp", "Extract" if slow_op == "Ext" else slow_op))
    # --- Adapt decomposition (the supervisor-requested explanation of the ~270x
    # headline factor).  The two Adapt calls do DIFFERENT WORK, so the raw ratio
    # is not a speed comparison:
    #   * LAS Adapt is obliged by eprint 2020/845 Alg. 2 line 21 to run PreVerify
    #     before it returns, and at the packed tier it also decodes sigma-hat, Y,
    #     r' and pk and re-encodes sigma.
    #   * secp256k1_ecdsa_adaptor_decrypt does neither: it deserialises the
    #     162-byte adaptor signature, inverts one scalar, multiplies, and
    #     conditionally negates.  Verification is a SEPARATE exported call.
    # Every quantity below is measured; the script only divides.
    _cl_adapt = classical["Adapt"][0]
    _las_adapt_packed = packed_l2["Adapt"][0]
    _las_prever_packed = packed_l2["PreVerify"][0]
    m.append(("clAdaptUs", "%.1f" % _cl_adapt))
    m.append(("lasAdaptPackedUs", "%.0f" % _las_adapt_packed))
    # share of LAS Adapt that is the PreVerify the construction mandates
    m.append(("adaptPreVerifyPct", "%.0f"
              % (100.0 * _las_prever_packed / _las_adapt_packed)))
    # like-for-like: charge the classical side the verification LAS is forced to do
    m.append(("clAdaptMatchedUs", "%.0f" % (classical["PreVerify"][0] + _cl_adapt)))
    m.append(("clRatioAdaptMatched", "%.0f"
              % (_las_adapt_packed / (classical["PreVerify"][0] + _cl_adapt))))
    # bytes the packed Adapt must decode and re-encode: sigma-hat, Y, r', pk in,
    # sigma out -- at the SAME parameter set as tab:classical (L2).
    m.append(("adaptCodecKB", "%.1f"
              % ((c2["signature (c,z)"] + c2["signature (c,z)"] + c2["Y = t'"]
                  + c2["r'"] + c2["pk = t"]) / 1024.0)))
    # ML-DSA adaptor experiment (measured; evidence/mldsa_hint/latest).
    # A SEPARATE construction from the scheme of record -- these macros must
    # never be mixed with the simplified scheme's figures.
    if mldsa:
        m.append(("mldsaIters", mldsa["iters"]))
        m.append(("mldsaContract", mldsa["contract"]))
        m.append(("mldsaNaiveP", mldsa["naive_p1"]))
        m.append(("mldsaRepairedP", mldsa["repaired_p4"]))
        m.append(("mldsaOvPreSign", mldsa["ov_mldsa"]))
        m.append(("mldsaOvPreSignSimp", mldsa["ov_simp"]))
        m.append(("mldsaAttempts", mldsa["att_mldsa"]))
        m.append(("mldsaAttemptsSimp", mldsa["att_simp"]))
        m.append(("mldsaSigBytes", mldsa["sig_mldsa"]))
        m.append(("mldsaSigBytesSimp", mldsa["sig_simp"]))
        m.append(("mldsaSigRatio", mldsa["sig_ratio"]))
        m.append(("mldsaStmtBytes", mldsa["stmt_mldsa"]))
        m.append(("mldsaStmtRatio", mldsa["stmt_ratio"]))
        m.append(("mldsaPayloadRatio", mldsa["payload_ratio"]))

    # on-chain settlement gas (measured; evidence/onchain/latest/gas_report.log)
    if onchain:
        cap = 16_777_216
        def g(k):
            return "{:,}".format(onchain[k]).replace(",", "\\,")
        m.append(("gasClassical", g("claimClassical")))
        m.append(("gasLasFloor", g("claimLAS")))
        m.append(("gasLasVerified", g("claimLASVerified")))
        m.append(("gasLasVerifiedM", "%.1f" % (onchain["claimLASVerified"] / 1e6)))
        m.append(("gasRatioFloor", "%.1f" % (onchain["claimLAS"] / onchain["claimClassical"])))
        m.append(("gasRatioVerified", "%.0f" % (onchain["claimLASVerified"] / onchain["claimClassical"])))
        m.append(("gasCapOver", "%.1f" % (onchain["claimLASVerified"] / cap)))
        if "claim" in onchain:
            m.append(("gasNaysayClaimM", "%.1f" % (onchain["claim"] / 1e6)))
        if "naysayDigest" in onchain:
            m.append(("gasNaysayDigestM", "%.1f" % (onchain["naysayDigest"] / 1e6)))
        if "claimLASVerifiedOpt" in onchain:
            # same --gas-report table as the rows above, so directly comparable with them
            m.append(("gasLasOpt", g("claimLASVerifiedOpt")))
            m.append(("gasRatioOpt", "%.0f" % (onchain["claimLASVerifiedOpt"]
                                               / onchain["claimClassical"])))

    if pi_params:
        m.append(("piKnowledgeError", pi_params["knowledge_error"]))
        m.append(("piProofBytes", pi_params["proof_bytes"]))

    # The one-transaction result, from a real client's receipt (NOT the harness).
    if onetx:
        cap = 16_777_216
        m.append(("gasOptReceipt", "{:,}".format(onetx["gasUsed"]).replace(",", "\\,")))
        m.append(("gasOptCapPct", "%.1f" % (100.0 * onetx["gasUsed"] / cap)))
        m.append(("gasOptHeadroom",
                  "{:,}".format(cap - onetx["gasUsed"]).replace(",", "\\,")))
        m.append(("gasOptCalldata",
                  "{:,}".format(onetx["calldata"]).replace(",", "\\,")))
    # Rust port (protocol driver mirrors the C driver; Criterion is the
    # statistical harness)
    m.append(("rustOvPreSign", pct(rust_over["PreSign vs Sign"])))
    m.append(("rustOvPreVerify", pct(rust_over["PreVerify vs Verify"])))
    m.append(("rustOvAdapt", pct(rust_over["Adapt vs Verify"])))
    # Rust full-protocol (packed) tier: the robust cross-language signal --
    # positive and same ordering as C (Adapt > PreVerify > PreSign).
    m.append(("rustPackedOvPreSign", pct(rust_packed_over["PreSign"])))
    m.append(("rustPackedOvPreVerify", pct(rust_packed_over["PreVerify"])))
    m.append(("rustPackedOvAdapt", pct(rust_packed_over["Adapt"])))
    m.append(("rustAttBase", rust_rej["att_base"]))
    m.append(("rustAttLas", rust_rej["att_las"]))
    m.append(("rustAccBase", rust_rej["acc_base"]))
    m.append(("rustAccLas", rust_rej["acc_las"]))
    m.append(("rustCMaxDev", "%.0f" % abs(devs[dev_max_op])))
    m.append(("rustCMaxDevOp", "Extract" if dev_max_op == "Ext" else dev_max_op))
    m.append(("critSamples", crit_samples))
    m.append(("katDigestHead", rust_kat["kat_head"]))
    m.append(("katDigestTail", rust_kat["kat_tail"]))

    body = header(["evidence/latest/tables/*.csv", "evidence/latest/logs/*.log",
                   "rust/fips204-las/*.log"], meta)
    body += "".join("\\newcommand{\\%s}{%s}\n" % (k, v) for k, v in m)
    (out / "benchmacros.tex").write_text(body)


def emit_tab_timing(out, meta, params, timing):
    heads = []
    for lvl in LEVELS:
        p = params[lvl]
        disp = DISPLAY[lvl].replace("Simplified ", "Simplified\\\\ ")
        disp = disp.replace("LAS-2020/845 reference", "LAS-2020/845\\\\ reference")
        heads.append("\\shortstack[r]{%s\\\\ $(%s,%s,%s)$}"
                     % (disp, p["n"], p["ell"], p["kappa"]))
    rows_basic = ["KeyGen", "Sign", "Verify"]
    rows_las = ["PreSign", "PreVerify", "Adapt", "Ext"]

    def row(op):
        label = "Extract" if op == "Ext" else op
        return ("    %s & " % label
                + " & ".join(mean_sd(timing[lvl][op]) for lvl in LEVELS)
                + " \\\\\n")
    b = header(["evidence/latest/tables/primary_timing.csv"], meta)
    b += "\\begin{tabular}{@{}lrrrr@{}}\n  \\toprule\n"
    b += "  Operation\n    & " + "\n    & ".join(heads) + " \\\\\n  \\midrule\n"
    b += ("  \\multicolumn{5}{@{}l}{\\textit{Basic signature (reused unchanged "
          "by LAS)}} \\\\\n")
    b += "".join(row(op) for op in rows_basic)
    b += "  \\midrule\n"
    b += "  \\multicolumn{5}{@{}l}{\\textit{LAS adaptor operations}} \\\\\n"
    b += "".join(row(op) for op in rows_las)
    b += "  \\bottomrule\n\\end{tabular}\n"
    (out / "tab_timing.tex").write_text(b)


def emit_tab_overhead_target(out, meta, timing, over, packed_t, packed_over):
    """Adaptor overhead at the target setting, at BOTH measurement boundaries:
    the core tier (structures in/out; isolates the adaptor arithmetic) and the
    full-protocol tier (packed bytes in/out; incl. the decode + encode a
    wire/on-chain consumer pays).  The tier rule: whenever both tiers
    are measured, both are presented."""
    t = timing[TARGET]
    o = over[TARGET]
    p = packed_t
    b = header(["evidence/latest/tables/primary_timing.csv",
                "evidence/latest/tables/adaptor_overhead.csv",
                "evidence/latest/logs/fair_%s.log (TIER-2 packed section)"
                % TARGET.lower()], meta)
    b += "\\begin{tabular}{@{}lrrr@{}}\n  \\toprule\n"
    b += ("  Operation (basic / adaptor) & Basic ($\\mu$s) & "
          "LAS adaptor ($\\mu$s) & Overhead \\\\\n  \\midrule\n")
    b += ("  \\multicolumn{4}{@{}l}{\\textit{Core tier (structures in/out "
          "--- isolates the adaptor arithmetic)}} \\\\\n")
    b += ("  KeyGen             & %s & %s (shared) & --- \\\\\n"
          % (mean_sd(t["KeyGen"]), mean_sd(t["KeyGen"])))
    b += ("  Sign / PreSign     & %s & %s & $+%s\\%%$ \\\\\n"
          % (mean_sd(t["Sign"]), mean_sd(t["PreSign"]), pct(o["PreSign vs Sign"])))
    b += ("  Verify / PreVerify & %s & %s & $+%s\\%%$ \\\\\n"
          % (mean_sd(t["Verify"]), mean_sd(t["PreVerify"]),
             pct(o["PreVerify vs Verify"])))
    b += ("  Verify / Adapt     & %s & %s & $+%s\\%%$ \\\\\n"
          % (mean_sd(t["Verify"]), mean_sd(t["Adapt"]), pct(o["Adapt vs Verify"])))
    b += ("  Extract            & --- & %s & (LAS only) \\\\\n"
          % mean_sd(t["Ext"]))
    b += "  \\midrule\n"
    b += ("  \\multicolumn{4}{@{}l}{\\textit{Packed tier (wire bytes "
          "in/out --- incl.\\ decode + encode)}} \\\\\n")
    b += ("  KeyGen             & %s & %s (shared) & --- \\\\\n"
          % (mean_sd(p["KeyGen"]), mean_sd(p["KeyGen"])))
    b += ("  Sign / PreSign     & %s & %s & $+%s\\%%$ \\\\\n"
          % (mean_sd(p["Sign"]), mean_sd(p["PreSign"]),
             pct(packed_over["PreSign"])))
    b += ("  Verify / PreVerify & %s & %s & $+%s\\%%$ \\\\\n"
          % (mean_sd(p["Verify"]), mean_sd(p["PreVerify"]),
             pct(packed_over["PreVerify"])))
    b += ("  Verify / Adapt     & %s & %s & $+%s\\%%$ \\\\\n"
          % (mean_sd(p["Verify"]), mean_sd(p["Adapt"]),
             pct(packed_over["Adapt"])))
    b += ("  Extract            & --- & %s & (LAS only) \\\\\n"
          % mean_sd(p["Ext"]))
    b += "  \\bottomrule\n\\end{tabular}\n"
    (out / "tab_overhead_target.tex").write_text(b)


def emit_tab_components(out, meta, params, comm):
    lvls = ["L2", "L3", "L5"]

    def b3(key):
        return " & ".join(str(comm[l][key]) for l in lvls)

    def zpct():
        return " & ".join("%.1f\\%%" % (100.0 * comm[l]["z"]
                                        / comm[l]["signature (c,z)"])
                          for l in lvls)
    heads = " & ".join(
        "\\shortstack[r]{Simplified\\\\ Dilithium-%s\\\\ $(%s,%s)$}"
        % (tag, params[l]["n"], params[l]["ell"])
        for tag, l in (("II", "L2"), ("III", "L3"), ("V", "L5")))
    b = header(["evidence/latest/tables/communication_components.csv"], meta)
    b += "\\begin{tabular}{@{}lrrr@{}}\n  \\toprule\n"
    b += "  Component\n    & " + heads + " \\\\\n  \\midrule\n"
    b += "  $c$ (challenge)   & %s \\\\\n" % b3("c")
    b += "  $z$ (response)    & %s \\\\\n" % b3("z")
    b += "  Signature $(c,z)$ & %s \\\\\n" % b3("signature (c,z)")
    b += "  \\midrule\n"
    b += "  $z$ as \\%% of signature & %s \\\\\n" % zpct()
    b += "  \\midrule\n"
    b += "  Public key $pk$          & %s \\\\\n" % b3("pk = t")
    b += "  Secret key $sk$          & %s \\\\\n" % b3("sk = r")
    b += "  Statement $Y$ (LAS only) & %s \\\\\n" % b3("Y = t'")
    b += "  Witness $y$ (LAS only)   & %s \\\\\n" % b3("r'")
    b += "  Pre-signature (LAS only) & %s \\\\\n" % b3("pre-signature (c,z_hat)")
    b += "  \\bottomrule\n\\end{tabular}\n"
    (out / "tab_components.tex").write_text(b)


def emit_tab_complete_target(out, meta, comm):
    c = comm[TARGET]
    rows = [
        ("Public key", "$pk=t$", c["pk = t"], "yes (shared)"),
        ("Secret key", "$sk=r$", c["sk = r"], "yes (shared)"),
        ("Challenge", "$c$", c["c"], "yes"),
        ("Response", "$z\\,/\\,\\hat z$", c["z"], "yes"),
        ("Signature", "$(c,z)$", c["signature (c,z)"], "yes (basic signature)"),
        ("Statement", "$Y=t'$", c["Y = t'"], "LAS only (public)"),
        ("Witness", "$y=r'$", c["r'"], "LAS only (private)"),
        ("Pre-signature", "$(c,\\hat z)$", c["pre-signature (c,z_hat)"],
         "LAS only"),
        ("Adapted signature", "$(c,z)$", c["final adapted sig (c,z)"],
         "LAS (verifies as basic sig)"),
    ]
    b = header(["evidence/latest/tables/communication_components.csv"], meta)
    b += "\\begin{tabular}{@{}llrl@{}}\n  \\toprule\n"
    b += "  Object & Notation & Bytes & In the basic signature? \\\\\n  \\midrule\n"
    for name, note, byts, role in rows:
        b += "  %s & %s & %d & %s \\\\\n" % (name, note, byts, role)
    b += "  \\bottomrule\n\\end{tabular}\n"
    (out / "tab_complete_target.tex").write_text(b)


def parse_onchain_gas(path):
    """Claim-path and Naysayer gas from a captured `forge test --gas-report` log.

    Returns the MAX column per function: the worst case a settlement can cost,
    which is what the EIP-7825 per-transaction cap must be judged against.
    Absent log -> None, so the Stage-1 pipeline still runs without evidence/onchain.
    """
    if not path.exists():
        return None
    want = ["claimClassical", "claimLAS", "claimLASVerified", "claimLASVerifiedOpt",
            "claim", "naysayDigest", "naysayNorm", "naysayWprime"]
    got = {}
    for line in path.read_text(errors="replace").splitlines():
        if not line.lstrip().startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 6 or cells[0] not in want:
            continue
        nums = [c for c in cells[1:] if re.fullmatch(r"[0-9]+", c)]
        if len(nums) >= 4:
            got[cells[0]] = int(nums[3])
    return got or None


def parse_pi_params(path):
    """Knowledge error and proof size from the COMMITTED LaZer parameter header.

    ref/relation_zk_params.h is emitted by the SageMath codegen and checked in, so
    these are the parameter set's own reported figures -- not a measurement of a
    run, and not to be conflated with the on-the-wire proof bytes observed in the
    Stage-2 study, which are smaller.  The header states the size in KiB; that unit
    is carried through to the report rather than silently rewritten as kB.
    """
    if not path.exists():
        return None
    txt = path.read_text(errors="replace")
    ke = re.search(r"knowledge error <= 2\^\(-([0-9.]+)\)", txt)
    sz = re.search(r"Proof size\s*\n//\s*~\s*([0-9.]+)\s*KiB", txt)
    if not ke or not sz:
        return None
    # The header labels the figure KiB, and that label checks out: the value times
    # 1024 is an exact integer (times 1000 it is not), so the tool divided bytes by
    # 1024.  Report BYTES anyway -- every other size in the report is in bytes, which
    # also makes this directly comparable with the on-the-wire proof of tab:stage2-comm.
    kib = float(sz.group(1))
    b = kib * 1024.0
    if abs(b - round(b)) > 1e-6:
        die("ref/relation_zk_params.h reports %s KiB, which is not a whole number of "
            "bytes -- the unit in that header may not be KiB after all" % sz.group(1))
    return {"knowledge_error": ("%g" % float(ke.group(1))),
            "proof_bytes": "{:,}".format(int(round(b))).replace(",", "\\,")}


def parse_onchain_onetx(d):
    """The ONE-TRANSACTION result, taken from a real client's own receipt.

    evidence/onchain_onetx/latest/ holds what anvil (pinned `osaka`, EIP-7825
    enforcement on) reported for a claim sent at an explicit --gas-limit: the
    receipt (status, gasUsed) and the mined transaction (the gasLimit actually
    on it).  This is the AUTHORITATIVE figure for "fits in one transaction" --
    a client's accounting, with no test-harness inspector in the measured frame.
    The forge --gas-report row for the same function is a different measurement
    and is reported separately; the two must never be conflated.

    Absent directory -> None, so the pipeline still runs without this evidence.
    """
    import json
    rec, tx = d / "claim_receipt.json", d / "claim_tx.json"
    if not rec.exists() or not tx.exists():
        return None

    def num(v):
        v = str(v)
        return int(v, 16) if v.startswith("0x") else int(v)

    r = json.loads(rec.read_text())
    t = json.loads(tx.read_text())
    status = num(r["status"])
    if status != 1:
        die("evidence/onchain_onetx/latest: the claim transaction did not "
            "succeed (receipt status %d) -- refusing to emit a macro for it" % status)
    gas_used = num(r["gasUsed"])
    gas_limit = num(t.get("gas", t.get("gasLimit")))
    cap = 16_777_216
    if gas_limit > cap:
        die("evidence/onchain_onetx/latest: the mined gasLimit (%d) exceeds the "
            "EIP-7825 cap (%d)" % (gas_limit, cap))
    if gas_used >= cap:
        die("evidence/onchain_onetx/latest: gasUsed (%d) is not under the "
            "EIP-7825 cap (%d)" % (gas_used, cap))
    return {"gasUsed": gas_used, "gasLimit": gas_limit,
            "calldata": (len(t["input"]) - 2) // 2}


def emit_tab_classical(out, meta, timing, comm, classical, packed_l2):
    """Classical vs LAS at Simplified Dilithium-II.  The classical library
    exposes exactly ONE measurement boundary (its native API: the 162-B
    pre-signature codec runs INSIDE the timed calls; public-key and
    final-ECDSA-signature wire codecs stay OUTSIDE -- verified against the
    secp256k1-zkp sources), so the classical column is that single tier and
    LAS is shown at both of its tiers, per the tier rule.

    The overhead column is TIER-MATCHED rather than tied to one LAS tier,
    because that single classical boundary is core-like for KeyGen/Sign/Verify
    and packed-like for the four adaptor operations (see CORE_LIKE below)."""
    l2 = timing["L2"]
    cc = comm["L2"]
    b = header(["evidence/latest/logs/classical.log",
                "evidence/latest/tables/primary_timing.csv",
                "evidence/latest/logs/fair_l2.log (TIER-2 packed section)",
                "evidence/latest/tables/communication_components.csv"], meta)
    # The classical library exposes ONE hybrid boundary, and which LAS tier that
    # corresponds to differs PER OPERATION: KeyGen/Sign/Verify exchange in-memory
    # structs with their wire codecs OUTSIDE the timed call (core-like), while
    # PreSign/PreVerify/Adapt/Extract pack or parse the pre-signature INSIDE it
    # (packed-like).  Comparing every row against a single LAS tier would count
    # the codec on one side only for four of the seven operations.
    CORE_LIKE = {"KeyGen", "Sign", "Verify"}

    def ov(las, cl):
        """LAS-over-classical factor, LAS taken at the tier that matches the
        classical library's boundary for that operation.  A value below 1 would
        mean LAS is absolutely faster; the sub-1 branch is kept so such a result
        prints honestly rather than rounding away to '1.0'."""
        r = las / cl
        return ("%.0f" % r) if r >= 10 else ("%.2f" % r if r < 1 else "%.1f" % r)

    # A fifth column makes this table the widest in the report, so the headers
    # wrap onto more lines (no wording is dropped -- the caption still defines
    # each boundary) and the inter-column gaps are tightened from the 6pt default.
    sep = "@{\\hspace{5pt}}"
    b += "\\begin{tabular}{@{}l%sr%sr%sr%sr@{}}\n  \\toprule\n" % ((sep,) * 4)
    b += ("  & \\shortstack[r]{ECDSA adaptor\\\\ (classical,\\\\ hybrid\\\\ native API)} & "
          "\\shortstack[r]{LAS adaptor\\\\ (post-\\\\ quantum,\\\\ core tier)} & "
          "\\shortstack[r]{LAS adaptor\\\\ (post-\\\\ quantum,\\\\ packed tier)} & "
          "\\shortstack[r]{overhead\\\\ LAS $\\div$\\\\ classical\\\\ (tier-matched)} "
          "\\\\\n  \\midrule\n")
    b += "  \\multicolumn{5}{@{}l}{\\textit{Computation ($\\mu$s/op)}} \\\\\n"
    for op in ["KeyGen", "Sign", "Verify", "PreSign", "PreVerify", "Adapt", "Ext"]:
        label = "Extract" if op == "Ext" else op
        las = l2[op][0] if op in CORE_LIKE else packed_l2[op][0]
        b += ("  %s & %s & %s & %s & %s$\\times$ \\\\\n"
              % (label, mean_sd(classical[op]), mean_sd(l2[op]),
                 mean_sd(packed_l2[op]), ov(las, classical[op][0])))
    b += "  \\midrule\n"
    b += "  \\multicolumn{5}{@{}l}{\\textit{Communication (bytes)}} \\\\\n"
    for label, ck, lk in (("Public key / statement", "pk", "pk = t"),
                          ("Secret key / witness  ", "sk", "sk = r"),
                          ("Signature             ", "sig", "signature (c,z)"),
                          ("Pre-signature         ", "presig",
                           "pre-signature (c,z_hat)")):
        b += ("  %s & %d & \\multicolumn{2}{c}{%d} & %s$\\times$ \\\\\n"
              % (label, classical[ck], cc[lk], ov(cc[lk], classical[ck])))
    b += "  \\bottomrule\n\\end{tabular}\n"
    (out / "tab_classical.tex").write_text(b)


def parse_gate_lines(path):
    """Core-tier run-validity gate lines (same format in the C and Rust logs):
    'rejection gate [Algorithm N <op>]: <calls> calls, measured <x>
    attempts/call (acceptance <y>%) ...' -> {op: (calls, attempts, acc_pct)}.
    The '(packed tier)' twins are deliberately not matched."""
    t = path.read_text(errors="replace")
    out = {}
    for m in re.finditer(
            r"rejection gate \[Algorithm [12] (Sign|PreSign)\]: (\d+) calls, "
            r"measured ([0-9.]+) attempts/call \(acceptance ([0-9.]+)%\)", t):
        out[m.group(1)] = (int(m.group(2)), float(m.group(3)), float(m.group(4)))
    for op in ("Sign", "PreSign"):
        if op not in out:
            die("core-tier rejection-gate line for %s missing from %s"
                % (op, path))
    return out


def rejection_model(p):
    """Closed-form geometric attempt model at one parameter set: per-coefficient
    acceptance window [-(bound), bound] out of 2*gamma+1 mask values, all
    (n+ell)*d coefficients independent.  Returns per operation:
    (acceptance, expected attempts, geometric SD, median, 95th percentile)."""
    import math
    n, ell = int(p["n"]), int(p["ell"])
    kappa, gamma, d = int(p["kappa"]), int(p["gamma"]), int(p["N"])

    def model(bound):
        acc = ((2.0 * bound + 1.0) / (2.0 * gamma + 1.0)) ** ((n + ell) * d)
        exp = 1.0 / acc
        sd = math.sqrt(1.0 - acc) / acc
        p50 = math.ceil(math.log(0.5) / math.log(1.0 - acc))
        p95 = math.ceil(math.log(0.05) / math.log(1.0 - acc))
        return acc, exp, sd, p50, p95

    return {"Sign": model(gamma - kappa), "PreSign": model(gamma - kappa - 1)}


def emit_tab_rejstats(out, meta, params, rej, gates_c, gates_rust, gates_crit):
    """Rejection-sampling statistics at the target setting: the closed-form
    geometric model's mean-attempts / acceptance prediction against every
    measured sample (C distribution sample, C timed-run gate, Rust driver gate,
    Criterion gate).  Only Mean and Acceptance are reported here because they are
    the two quantities every source records; the per-call dispersion (geometric
    SD, percentiles, and the sampled maxima) is shown in the companion
    distribution figure (\\cref{fig:rejdist}) rather than duplicated as mostly
    empty table columns."""
    model = rejection_model(params[TARGET])
    csv_row = {"Sign": rej[TARGET]["Base Sign"],
               "PreSign": rej[TARGET]["LAS PreSign"]}
    b = header(["evidence/latest/tables/rejection_sampling.csv",
                "evidence/latest/logs/fair_l3.log (rejection gate lines)",
                "rust/fips204-las/bench_levels_rust.log (rejection gate lines)",
                "rust/fips204-las/bench_las_criterion.log (rejection gate lines)"],
               meta)
    b += "\\begin{tabular}{@{}llrr@{}}\n  \\toprule\n"
    b += "  Operation & Source (calls) & Mean & Accept. \\\\\n  \\midrule\n"
    for op in ("Sign", "PreSign"):
        acc, exp = model[op][0:2]
        c_calls, c_att, c_acc = gates_c[op]
        r_calls, r_att, r_acc = gates_rust[op]
        k_calls, k_att, k_acc = gates_crit[op]
        r = csv_row[op]
        b += ("  %s & geometric model, \\cref{eq:rejacc} & %.3f & %.1f\\%% \\\\\n"
              % (op, exp, 100.0 * acc))
        b += ("     & C driver, distribution sample (2000) & %.3f & %.1f\\%% \\\\\n"
              % (float(r["avg_attempts"]), float(r["acceptance_pct"])))
        b += ("     & C driver, timed-run gate (%d) & %.3f & %.1f\\%% \\\\\n"
              % (c_calls, c_att, c_acc))
        b += ("     & Rust driver, timed-run gate (%d) & %.3f & %.1f\\%% \\\\\n"
              % (r_calls, r_att, r_acc))
        b += ("     & Rust Criterion, gate (%d) & %.3f & %.1f\\%% \\\\\n"
              % (k_calls, k_att, k_acc))
        if op == "Sign":
            b += "  \\midrule\n"
    b += "  \\bottomrule\n\\end{tabular}\n"
    (out / "tab_rejstats.tex").write_text(b)


def emit_tab_rust(out, meta, timing, rust_timing, crit):
    b = header(["evidence/latest/tables/primary_timing.csv",
                "rust/fips204-las/bench_levels_rust.log",
                "rust/fips204-las/bench_las_criterion.log"], meta)
    b += "\\begin{tabular}{@{}lrrrr@{}}\n  \\toprule\n"
    b += ("  Operation & C implementation ($\\mu$s) & Rust port ($\\mu$s) & "
          "Rust\\,/\\,C & \\shortstack[r]{Rust, Criterion\\\\ "
          "($\\mu$s, 95\\% CI)} \\\\\n  \\midrule\n")
    for op in ["KeyGen", "Sign", "Verify", "PreSign", "PreVerify", "Adapt", "Ext"]:
        label = "Extract" if op == "Ext" else op
        ratio = rust_timing[op][0] / timing[TARGET][op][0]
        lo, mid, hi = crit[op]
        b += ("  %s & %s & %s & %.2f & %s\\ [%s, %s] \\\\\n"
              % (label, mean_sd(timing[TARGET][op]), mean_sd(rust_timing[op]),
                 ratio, num(mid), num(lo), num(hi)))
    b += "  \\bottomrule\n\\end{tabular}\n"
    (out / "tab_rust.tex").write_text(b)


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main(argv=None):
    repo = Path(__file__).resolve().parents[1]
    ap = argparse.ArgumentParser(
        description="Generate report/latex/generated/*.tex from captured evidence.")
    ap.add_argument("--evidence-dir", default=str(repo / "evidence/latest"))
    ap.add_argument("--rust-dir", default=str(repo / "rust/fips204-las"))
    ap.add_argument("--output-dir", default=str(repo / "report/latex/generated"))
    a = ap.parse_args(argv)

    ev = Path(a.evidence_dir)
    rust = Path(a.rust_dir)
    out = Path(a.output_dir)
    out.mkdir(parents=True, exist_ok=True)

    params, timing, over, rej, comm = load_c_evidence(ev / "tables")
    meta = parse_metadata(ev / "metadata.txt")
    proto = parse_fair_protocol(ev / "logs" / ("fair_%s.log" % TARGET.lower()))
    packed_over = parse_packed_overhead(ev / "logs" / ("fair_%s.log" % TARGET.lower()))
    packed_t = parse_packed_timing(ev / "logs" / ("fair_%s.log" % TARGET.lower()))
    packed_l2 = parse_packed_timing(ev / "logs" / "fair_l2.log")
    tamper = parse_tamper(ev / "logs" / "serialization_tests.log")
    classical = parse_classical(ev / "logs" / "classical.log")
    onchain = parse_onchain_gas(Path(__file__).resolve().parent.parent
                                / "evidence" / "onchain" / "latest" / "gas_report.log")
    onetx = parse_onchain_onetx(Path(__file__).resolve().parent.parent
                                / "evidence" / "onchain_onetx" / "latest")
    pi_params = parse_pi_params(Path(__file__).resolve().parent.parent
                                / "ref" / "relation_zk_params.h")
    mldsa = parse_mldsa(Path(__file__).resolve().parent.parent
                        / "evidence" / "mldsa_hint" / "latest")
    (rust_params, rust_sizes, rust_timing, rust_over, rust_rej,
     rust_proto) = parse_rust_driver(rust / "bench_levels_rust.log")
    rust_packed_over = parse_rust_packed_overhead(rust / "bench_levels_rust.log")
    rust_kat = parse_rust_sizes(rust / "size_report_rust.log")
    crit, crit_samples = parse_criterion(rust / "bench_las_criterion.log")

    # cross-implementation validity gates
    pt = params[TARGET]
    if (rust_params["n"], rust_params["ell"], rust_params["kappa"]) != \
       (pt["n"], pt["ell"], pt["kappa"]):
        die("Rust parameter set (n=%s, ell=%s, kappa=%s) does not match the C "
            "target setting (n=%s, ell=%s, kappa=%s)"
            % (rust_params["n"], rust_params["ell"], rust_params["kappa"],
               pt["n"], pt["ell"], pt["kappa"]))
    ct = comm[TARGET]
    if (rust_sizes["pk"], rust_sizes["sk"], rust_sizes["sig"]) != \
       (ct["pk = t"], ct["sk = r"], ct["signature (c,z)"]):
        die("Rust packed sizes %s do not equal the C target-setting sizes "
            "(pk %d, sk %d, sig %d)" % (rust_sizes, ct["pk = t"], ct["sk = r"],
                                        ct["signature (c,z)"]))
    if (proto["reps"], proto["iters_sign"], proto["iters_verify"]) != \
       (rust_proto["reps"], rust_proto["iters_sign"], rust_proto["iters_verify"]):
        die("the C and Rust protocol drivers ran different repetition schemes: "
            "C %s vs Rust %s" % (proto, rust_proto))

    emit_macros(out, meta, proto, params, timing, over, rej, comm, classical,
                rust_params, rust_sizes, rust_timing, rust_over, rust_rej,
                rust_proto, rust_kat, crit, crit_samples, tamper, packed_over,
                rust_packed_over, onchain, packed_l2, mldsa, onetx, pi_params)
    emit_tab_timing(out, meta, params, timing)
    emit_tab_overhead_target(out, meta, timing, over, packed_t, packed_over)
    emit_tab_components(out, meta, params, comm)
    emit_tab_complete_target(out, meta, comm)
    emit_tab_classical(out, meta, timing, comm, classical, packed_l2)
    emit_tab_rust(out, meta, timing, rust_timing, crit)
    gates_c = parse_gate_lines(ev / "logs" / ("fair_%s.log" % TARGET.lower()))
    gates_rust = parse_gate_lines(rust / "bench_levels_rust.log")
    gates_crit = parse_gate_lines(rust / "bench_las_criterion.log")
    emit_tab_rejstats(out, meta, params, rej, gates_c, gates_rust, gates_crit)

    o = over[TARGET]
    print("Evidence run : %s (git %s)" % (meta["run_id"], meta["git_short"]))
    print("Output dir   : %s" % out)
    print("Wrote        : benchmacros.tex, tab_timing.tex, tab_overhead_target.tex,")
    print("               tab_components.tex, tab_complete_target.tex,")
    print("               tab_classical.tex, tab_rust.tex, tab_rejstats.tex")
    print("Headline     : PreSign +%s%% | PreVerify +%s%% | Adapt +%s%% (target "
          "setting)" % (pct(o["PreSign vs Sign"]), pct(o["PreVerify vs Verify"]),
                        pct(o["Adapt vs Verify"])))


if __name__ == "__main__":
    main()
