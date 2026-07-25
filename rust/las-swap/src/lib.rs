//! Stage 2 — a post-quantum atomic swap on UTXO chains, via LAS adaptor
//! signatures (eprint 2020/845 §4.1, Fig. 1), with a classical ECDSA-adaptor
//! baseline to measure it against.
//!
//! # Layout
//!
//! * [`utxo`] — the UTXO ledger the paper's §4 assumes, with the signature
//!   algorithm as a parameter.
//! * [`backend`] — the [`AdaptorScheme`](backend::AdaptorScheme) and
//!   [`ZkpBackend`](backend::ZkpBackend) traits, and the three-configuration
//!   table.
//! * [`las_backend`] — LAS, plus the LaZer and Groth16 role-A provers.
//! * [`ecdsa_backend`] — the classical ECDSA adaptor over `secp256k1-zkp`.
//! * [`protocol`] — Fig. 1 itself, verbatim.
//! * [`metrics`] — per-phase timing and per-message communication accounting.
//!
//! # The three configurations
//!
//! | # | Signature | Role-A proof `pi` |
//! |---|---|---|
//! | 1 | classical adaptor (ECDSA) | not required — ECDSA adaptors have no knowledge gap |
//! | 2 | LAS (post-quantum) | Groth16 over `∃r : A r = t ∧ ‖r‖∞ ≤ 1` |
//! | 3 | LAS (post-quantum) | LaZer over `∃r : A r = t ∧ ‖r‖∞ ≤ 1` |
//!
//! Read [`backend`]'s module documentation before interpreting any number from
//! this crate: it sets out which comparisons are controlled (2 → 3) and which
//! are not (1 → 2/3), and why configuration 1 carries no `pi`.
//!
//! # Building
//!
//! The default build compiles, but every configuration needs its backend:
//!
//! ```text
//! cargo build --release --features secp256k1                 # configuration 1
//! cargo build --release --features relation-zk               # configuration 3
//! cargo build --release --features secp256k1,relation-zk     # 1 and 3
//! ```
//!
//! `relation-zk` additionally needs the vendored LaZer library built once (see
//! the repo README, "π + atomic swap"). Configuration 2 needs a Groth16 prover,
//! which is not vendored — see [`las_backend::Groth16`].
//!
//! This crate is deliberately **not** part of the KAT-locked `fips204` package;
//! it consumes it as a path dependency, so the LAS port's pinned digest is
//! untouched by anything here.

pub mod backend;
pub mod ecdsa_backend;
#[cfg(feature = "groth16")]
pub mod groth16_circuit;
pub mod las_backend;
pub mod metrics;
pub mod protocol;
pub mod utxo;
