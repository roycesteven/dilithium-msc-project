//! LAS additive (dilithium-msc-project): build script for the OPT-IN
//! `relation-zk` feature only — the default build does nothing here.
//!
//! With `--features relation-zk` it compiles the SAME C bridge the C build
//! uses (`ref/relation_zk_lazer.c` — the single TU that talks to the vendored
//! LaZer library) and links it plus LaZer's static libraries, so the Rust and
//! C ports run the IDENTICAL proof system with the IDENTICAL parameter set
//! (`ref/relation_zk_params.h`).  No `cc` crate: one file, invoked directly.
//!
//! Overridable paths (same knobs as ref/Makefile):
//!   LAZER_DIR        (default <repo>/third_party/lazer)
//!   LAZER_DEP_PREFIX (default $HOME/micromamba/envs/lazer-build — MPFR/GMP)

use std::env;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    println!("cargo:rerun-if-env-changed=LAZER_DIR");
    println!("cargo:rerun-if-env-changed=LAZER_DEP_PREFIX");
    if env::var_os("CARGO_FEATURE_RELATION_ZK").is_none() {
        return;
    }

    let crate_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let repo = crate_dir.join("../..").canonicalize().unwrap();
    let ref_dir = repo.join("ref");
    let lazer_dir = env::var("LAZER_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|_| repo.join("third_party/lazer"));
    let dep_prefix = env::var("LAZER_DEP_PREFIX")
        .map(PathBuf::from)
        .unwrap_or_else(|_| PathBuf::from(env::var("HOME").unwrap()).join("micromamba/envs/lazer-build"));
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());

    let bridge_c = ref_dir.join("relation_zk_lazer.c");
    println!("cargo:rerun-if-changed={}", bridge_c.display());
    println!("cargo:rerun-if-changed={}", ref_dir.join("relation_zk_lazer.h").display());
    println!("cargo:rerun-if-changed={}", ref_dir.join("relation_zk_params.h").display());

    let cc = env::var("CC").unwrap_or_else(|_| "cc".into());
    let obj = out.join("relation_zk_lazer.o");
    let status = Command::new(&cc)
        .args(["-Wall", "-Wextra", "-Wshadow", "-O3"])
        .arg(format!("-I{}", lazer_dir.display()))
        .arg(format!("-I{}", ref_dir.display()))
        .arg("-c")
        .arg(&bridge_c)
        .arg("-o")
        .arg(&obj)
        .status()
        .expect("relation-zk: failed to run the C compiler");
    assert!(status.success(), "relation-zk: compiling ref/relation_zk_lazer.c failed \
        (is third_party/lazer cloned and built? see README \"pi + atomic swap\")");

    let lib = out.join("librelation_zk_bridge.a");
    let status = Command::new("ar")
        .arg("rcs")
        .arg(&lib)
        .arg(&obj)
        .status()
        .expect("relation-zk: failed to run ar");
    assert!(status.success(), "relation-zk: ar failed");

    // Link order matters: bridge -> lazer -> hexl -> mpfr/gmp -> stdc++.
    println!("cargo:rustc-link-search=native={}", out.display());
    println!("cargo:rustc-link-lib=static=relation_zk_bridge");
    println!("cargo:rustc-link-search=native={}", lazer_dir.display());
    println!("cargo:rustc-link-lib=static=lazer");
    let hexl = lazer_dir.join("third_party/hexl-development/build/hexl");
    println!("cargo:rustc-link-search=native={}", hexl.join("lib64").display());
    println!("cargo:rustc-link-search=native={}", hexl.join("lib").display());
    println!("cargo:rustc-link-lib=static=hexl");
    let dep_lib = dep_prefix.join("lib");
    println!("cargo:rustc-link-search=native={}", dep_lib.display());
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", dep_lib.display());
    println!("cargo:rustc-link-lib=dylib=mpfr");
    println!("cargo:rustc-link-lib=dylib=gmp");
    println!("cargo:rustc-link-lib=dylib=stdc++");
    println!("cargo:rustc-link-lib=dylib=m");
}
