#!/usr/bin/env python3
"""Turn a captured Stage-2 bench_swap log into LaTeX macros.

The report must never contain hand-typed measurements: every number a chapter
states has to come from a real run, through a macro generated here. This is the
Stage-2 twin of ``scripts/gen_report_data.py`` (which serves Stage 1).

Usage::

    python3 scripts/gen_stage2_data.py \
        --log evidence/stage2/latest/bench_swap.log \
        --env evidence/stage2/latest/environment.txt \
        --out report/latex/generated/stage2macros.tex

It parses, rather than recomputes: if a figure is not in the log, it does not
reach the report.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path

# Phase label in the log -> macro suffix.
PHASES = {
    "KeyGen x2 + Gen": "KeyMat",
    "Prove (pi)": "Prove",
    "PreSign (u1, tx1)": "PreSignA",
    "ProofVerify (pi)": "ProofVerify",
    "PreVerify (u2 gate)": "PreVerify",
    "PreSign (u2, tx2)": "PreSignB",
    "Adapt (u1 -> sigma2)": "AdaptA",
    "Settle chain 2": "SettleB",
    "Ext (u2)": "Ext",
    "Adapt (u2 -> sigma1)": "AdaptB",
    "Settle chain 1": "SettleA",
    "end-to-end": "Total",
}

# Message name in the log -> macro suffix.
MESSAGES = {
    "Y (statement)": "MsgY",
    "pi": "MsgPi",
    "sigma_hat_1": "MsgPreSigA",
    "tx1": "MsgTxA",
    "sigma_hat_2": "MsgPreSigB",
    "tx2": "MsgTxB",
    "tx2 + sigma_2": "MsgChainB",
    "tx1 + sigma_1": "MsgChainA",
}

ROMAN = {1: "One", 2: "Two", 3: "Three"}


def fmt_range(value: str) -> str:
    """Render ``a..b`` as a typeset en-dash range; leave a scalar alone.

    The log writes a range when a message's size varied between runs. It must
    stay visible as a range in the report -- collapsing it to a mean would hide
    exactly the variability the harness went to the trouble of detecting.
    """
    if ".." in value:
        lo, hi = value.split("..", 1)
        return f"{lo}--{hi}"
    return value


def tex_escape(text: str) -> str:
    # `~` is a non-breaking space in TeX, not a tilde: "~128-bit" would silently
    # render as " 128-bit". Map it to a maths approx sign, which is what the log
    # means by it.
    return (
        text.replace("\\", r"\textbackslash{}")
        .replace("_", r"\_")
        .replace("%", r"\%")
        .replace("&", r"\&")
        .replace("#", r"\#")
        .replace("~", r"$\sim$")
    )


def parse(log: str) -> dict:
    """Split the log into per-configuration blocks and pull the numbers out."""
    out: dict = {"configs": {}}

    if m := re.search(r"^Master seed\s*:\s*(\S+)", log, re.M):
        out["seed"] = m.group(1)
    if m := re.search(r"^Swaps per config\s*:\s*(\d+)", log, re.M):
        out["runs"] = m.group(1)
    if m := re.search(r"^LAS parameter setup:\s*(\S+)", log, re.M):
        out["las_setup"] = m.group(1)

    blocks = re.split(r"^-{20,}$", log, flags=re.M)
    for block in blocks:
        m = re.search(r"^Configuration (\d+):", block, re.M)
        if not m:
            continue
        num = int(m.group(1))
        cfg: dict = {"phases": {}, "messages": {}}

        if mm := re.search(r"^\s*signature\s*:\s*(.+?)\s*\[(.+?)\]\s*$", block, re.M):
            cfg["sig_name"] = mm.group(1).strip()
            cfg["sig_params"] = mm.group(2).strip()
        if mm := re.search(r"^\s*role-A pi\s*:\s*(.+)$", block, re.M):
            cfg["pi_name"] = mm.group(1).strip()
        if mm := re.search(r"^\s*binding\s*:\s*(\w+)", block, re.M):
            cfg["binding"] = mm.group(1)
        if mm := re.search(r"^\s*fully PQ\s*:\s*(\w+)", block, re.M):
            cfg["fully_pq"] = mm.group(1)
        if mm := re.search(r"setup:\s*(.+?)\s*\(([^()]+)\)\s*$", block, re.M):
            cfg["setup_kind"] = mm.group(1).strip()
            cfg["setup_time"] = mm.group(2).strip()

        if mm := re.search(
            r"Rejection gate:\s*([\d.]+) attempts/PreSign over (\d+) calls\s*\n"
            r"\s*expected\s*([\d.]+), tolerance \+/-\s*([\d.]+)",
            block,
        ):
            cfg["rej_measured"], cfg["rej_calls"] = mm.group(1), mm.group(2)
            cfg["rej_expected"], cfg["rej_tol"] = mm.group(3), mm.group(4)

        for label, key in PHASES.items():
            pat = re.escape(label) + r"\s+([\d.]+) \+/-\s+([\d.]+) us"
            if mm := re.search(pat, block):
                cfg["phases"][key] = (mm.group(1), mm.group(2))

        if mm := re.search(r"role-A proof share\s+([\d.]+) us\s+\(([\d.]+)%", block):
            cfg["proof_share"], cfg["proof_pct"] = mm.group(1), mm.group(2)

        for label, key in MESSAGES.items():
            pat = r"^\s*" + re.escape(label) + r"\s+([\d.]+(?:\.\.[\d.]+)?) B"
            if mm := re.search(pat, block, re.M):
                cfg["messages"][key] = mm.group(1)

        for label, key in (("off-chain", "OffChain"), ("on-chain", "OnChain")):
            if mm := re.search(r"^\s*subtotal\s+(\S+) B\s+" + label, block, re.M):
                cfg[key] = mm.group(1)
        if mm := re.search(r"^\s*TOTAL\s+(\S+) B", block, re.M):
            cfg["Total"] = mm.group(1)

        sizes = re.search(
            r"public key\s+(\d+) B\s+secret key\s+(\d+) B.*?"
            r"statement\s+(\d+) B\s+witness\s+(\d+) B.*?"
            r"signature\s+(\d+) B\s+pre-signature\s+(\d+) B\s+\(overhead (\d+) B\)",
            block,
            re.S,
        )
        if sizes:
            for i, key in enumerate(
                ["Pk", "Sk", "Statement", "Witness", "Sig", "PreSig", "PreSigOverhead"]
            ):
                cfg[key] = sizes.group(i + 1)

        out["configs"][num] = cfg

    if mm := re.search(
        r"time\s+([\d.]+) ->\s+([\d.]+) us\s+\(([-+][\d.]+)%\)", log
    ):
        out["cmp_time_from"], out["cmp_time_to"], out["cmp_time_pct"] = mm.groups()
    if mm := re.search(r"bytes\s+(\S+) ->\s+(\S+)\s+\(([-+][\d.]+)%\)", log):
        out["cmp_bytes_from"], out["cmp_bytes_to"], out["cmp_bytes_pct"] = mm.groups()
    if mm := re.search(r"proof\s+(\S+) ->\s+(\S+) B", log):
        out["cmp_proof_from"], out["cmp_proof_to"] = mm.groups()

    return out


def emit(data: dict, env: dict) -> str:
    lines = [
        "% AUTO-GENERATED by scripts/gen_stage2_data.py -- DO NOT EDIT.",
        "% Regenerate from a captured bench_swap log; never hand-type a measurement.",
        f"% Stage-2 evidence run: {env.get('run_id', 'unknown')}"
        f"  (git {env.get('git', 'unknown')})",
        "",
    ]

    def cmd(name: str, value: str) -> None:
        lines.append(rf"\newcommand{{\{name}}}{{{value}}}")

    cmd("stageTwoRunId", tex_escape(env.get("run_id", "unknown")))
    cmd("stageTwoGit", env.get("git", "unknown"))
    cmd("stageTwoCpu", tex_escape(env.get("cpu", "unknown")))
    cmd("stageTwoHost", tex_escape(env.get("host", "unknown")))
    cmd("stageTwoRustc", tex_escape(env.get("rustc", "unknown")))
    cmd("stageTwoRuns", data.get("runs", "?"))
    cmd("stageTwoSeed", data.get("seed", "?")[:16] + r"\ldots")
    cmd("stageTwoLasSetup", tex_escape(data.get("las_setup", "?")))
    lines.append("")

    for num, cfg in sorted(data["configs"].items()):
        r = ROMAN[num]
        for key, val in (
            ("SigName", cfg.get("sig_name", "")),
            ("SigParams", cfg.get("sig_params", "")),
            ("PiName", cfg.get("pi_name", "")),
            ("Binding", cfg.get("binding", "")),
            ("FullyPQ", cfg.get("fully_pq", "")),
            ("SetupKind", cfg.get("setup_kind", "")),
            ("SetupTime", cfg.get("setup_time", "")),
        ):
            if val:
                cmd(f"cfg{r}{key}", tex_escape(val))

        for key, (mean, sd) in cfg["phases"].items():
            cmd(f"cfg{r}{key}Mean", mean)
            cmd(f"cfg{r}{key}SD", sd)

        for key in ("proof_share", "proof_pct"):
            if key in cfg:
                cmd(f"cfg{r}{'ProofShare' if key == 'proof_share' else 'ProofPct'}", cfg[key])

        for key, val in cfg["messages"].items():
            cmd(f"cfg{r}{key}", fmt_range(tex_escape(val)))

        for key in (
            "OffChain", "OnChain", "Total", "Pk", "Sk", "Statement",
            "Witness", "Sig", "PreSig", "PreSigOverhead",
        ):
            if key in cfg:
                cmd(f"cfg{r}Bytes{key}", fmt_range(tex_escape(cfg[key])))

        for key, macro in (
            ("rej_measured", "RejMeasured"), ("rej_expected", "RejExpected"),
            ("rej_tol", "RejTol"), ("rej_calls", "RejCalls"),
        ):
            if key in cfg:
                cmd(f"cfg{r}{macro}", cfg[key])
        lines.append("")

    for key, macro in (
        ("cmp_time_from", "cmpTimeFrom"), ("cmp_time_to", "cmpTimeTo"),
        ("cmp_time_pct", "cmpTimePct"), ("cmp_bytes_from", "cmpBytesFrom"),
        ("cmp_bytes_to", "cmpBytesTo"), ("cmp_bytes_pct", "cmpBytesPct"),
        ("cmp_proof_from", "cmpProofFrom"), ("cmp_proof_to", "cmpProofTo"),
    ):
        if key in data:
            cmd(macro, fmt_range(tex_escape(data[key])))

    # Unsigned magnitudes, so prose can write "faster by X%" without the macro
    # supplying a second minus sign ("faster by -53.0%").
    for key, macro in (("cmp_time_pct", "cmpTimePctAbs"), ("cmp_bytes_pct", "cmpBytesPctAbs")):
        if key in data:
            cmd(macro, data[key].lstrip("+-"))

    # ---- derived quantities -------------------------------------------------
    # Computed here rather than typed into a chapter, so they cannot drift from
    # the run. A range like "80001..80050" is reduced to its midpoint for ratio
    # purposes only; the range itself is always what the tables print.
    def mid(value: str) -> float:
        if ".." in value:
            lo, hi = value.split("..")
            return (float(lo) + float(hi)) / 2
        return float(value)

    cfgs = data["configs"]
    lines.append("")
    lines.append("% derived (computed by the generator, not measured directly)")

    # Protocol time excluding the role-A proof: end-to-end minus the proof share.
    non_proof = {}
    for num, cfg in cfgs.items():
        total = cfg.get("phases", {}).get("Total", (None,))[0]
        if total is None:
            continue
        share = float(cfg.get("proof_share", 0.0))
        non_proof[num] = float(total) - share
        cmd(f"cfg{ROMAN[num]}NonProofUs", f"{non_proof[num]:.1f}")

    if 1 in non_proof and 2 in non_proof:
        cmd("stageTwoCryptoRatio", f"{non_proof[2] / non_proof[1]:.1f}")
    for num in (2, 3):
        if num in cfgs and 1 in cfgs:
            a, b = cfgs[1].get("Total"), cfgs[num].get("Total")
            if a and b:
                cmd(f"stageTwoBytesRatio{ROMAN[num]}", f"{mid(b) / mid(a):.1f}")
    if 1 in cfgs and 2 in cfgs:
        a, b = cfgs[1].get("OnChain"), cfgs[2].get("OnChain")
        if a and b:
            cmd("stageTwoOnChainRatio", f"{mid(b) / mid(a):.1f}")
    if "cmp_proof_from" in data and "cmp_proof_to" in data:
        cmd(
            "stageTwoProofSizeRatio",
            f"{mid(data['cmp_proof_to']) / mid(data['cmp_proof_from']):.0f}",
        )

    return "\n".join(lines) + "\n"


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", required=True, type=Path)
    ap.add_argument("--env", type=Path)
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    env: dict = {}
    if args.env and args.env.is_file():
        for line in args.env.read_text().splitlines():
            if "=" in line:
                k, v = line.split("=", 1)
                env[k.strip()] = v.strip()

    data = parse(args.log.read_text())
    if not data["configs"]:
        raise SystemExit(f"no configuration blocks parsed from {args.log}")

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(emit(data, env))
    print(f"wrote {args.out} from {len(data['configs'])} configuration(s)")


if __name__ == "__main__":
    main()
