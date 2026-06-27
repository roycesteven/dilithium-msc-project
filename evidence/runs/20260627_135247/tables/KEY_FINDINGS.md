# Key findings (auto-generated; headline = Simplified Dilithium-III (target); machine: n/a)

**Question answered: compared with the basic simplified Dilithium-style signature, how much extra computation and communication does the LAS exotic adaptor signature add?**

1. **Extra computation.** The basic signature uses Sign and Verify; the LAS adaptor adds four operations. At the Simplified Dilithium-III (target) setting: PreSign takes 1355 microseconds versus Sign at 1270 (+6.7%); PreVerify takes 288 versus Verify at 280 (+3.1%); Adapt takes 302 microseconds (+8.1% versus Verify); and Ext takes 101 microseconds. Pre-signing and pre-verification stay close to basic signing and verification, so the adaptor machinery costs roughly one extra signing pass plus a few verification-scale operations.

2. **Extra communication: the signature does not grow.** The basic signature, the pre-signature and the adapted signature are all byte-identical (6752 bytes), because Adapt computes z = ẑ + r' (it changes the response value, not the serialized structure). Inside the signature the response z is 99.1% of the bytes and the challenge c is only 64 bytes.

3. **Extra communication: LAS adds one public object, the statement.** Beyond the basic signature, LAS publishes the statement Y (4416 bytes, the same size as the public key) that locks the signature, plus the adaptor witness r' (704 bytes) held privately by the signer. So moving from the basic signature to the LAS adaptor signature costs essentially one extra public-key-sized object on the wire, not a larger signature.

_The LAS-2020/845 reference and the Simplified Dilithium-II/III/V sets are engineering parameter settings used for scaling context, not formal NIST security levels. Sources: per-operation timings -> primary_timing.csv / per_operation_timing_report.*; component sizes -> communication_components.csv / communication_components_clean_report.*; parameters -> parameter_sets.csv / parameter_sets_report.*._
