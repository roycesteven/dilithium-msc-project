//! Build script.
//!
//! Configuration 1 needs nothing here: the `secp256k1-zkp` crate vendors and
//! builds `libsecp256k1-zkp` through its own `-sys` crate.
//!
//! Configuration 3's LaZer linking is likewise not handled here: enabling this
//! crate's `relation-zk` feature turns on `fips204/relation-zk`, whose build
//! script (`rust/fips204-las/build.rs`) compiles the shared C bridge and emits
//! the link flags, which propagate to this binary.
//!
//! The file exists so that the above is written down where someone would look
//! for it, rather than leaving the absence of a build step unexplained.

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
}
