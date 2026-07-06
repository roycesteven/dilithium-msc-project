# Key findings (Stage-1 LAS benchmark; headline = Simplified Dilithium-III setting)

1. **Extra computation.** Compared with the basic simplified Dilithium-style signature, LAS adds PreSign (+2.8% over Sign), PreVerify (+2.9% over Verify) and Adapt (+6.2% over Verify), plus Ext, which extracts the witness s = z - ẑ in about 39 microseconds; the adaptor therefore costs roughly one extra signing pass plus a few verification-scale operations.

2. **The final signature does not grow.** The basic signature, the pre-signature and the adapted signature have the same 6752 bytes, because Adapt sets z = ẑ + r' (it changes the response value, not the serialized structure).

3. **Extra communication.** The one extra object LAS puts on the wire is the public statement Y = t' (4416 bytes, the same size as the public key, 4416 bytes); its witness r' (704 bytes) is the signer's private companion and is never published. The signature itself is unchanged. The LAS-2020/845 reference and the Simplified Dilithium-II/III/V settings are engineering benchmark settings only, not formal NIST / ML-DSA security levels.
