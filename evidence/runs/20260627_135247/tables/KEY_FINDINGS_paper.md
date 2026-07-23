# Key findings (paper-facing; headline = Simplified Dilithium-III setting)

Main comparison: the basic simplified Dilithium-style signature versus the LAS adaptor signature. The LAS-2020/845 reference and the Simplified Dilithium-II/III/V sets are engineering parameter settings for scaling context, not formal NIST-equivalent security levels.

1. **Extra computation is small.** On top of the basic Sign and Verify, LAS adds four operations. At the Simplified Dilithium-III setting PreSign costs +6.7% versus Sign, PreVerify +3.1% versus Verify, and Adapt +8.1% versus Verify (Adapt 302 microseconds, Ext 101 microseconds); Ext extracts the witness s = z - ẑ. Pre-signing and pre-verification mirror basic signing and verification, so the adaptor adds roughly one extra signing pass plus a few verification-scale operations.

2. **The final signature does not grow.** The basic signature, the pre-signature and the adapted signature are byte-identical (6752 bytes), because Adapt computes z = ẑ + r' (it changes the response value, not the serialized structure).

3. **LAS adds one public communication object: the statement.** Beyond the basic signature, LAS publishes the statement Y (4416 bytes, the same size as the public key) that locks the signature, plus the adaptor witness r' (704 bytes) held privately by the signer. The extra communication is the statement Y, not a larger signature.
