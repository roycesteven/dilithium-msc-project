# Key findings (Stage-1 LAS benchmark; headline = L3-like setting)

1. **Extra computation.** Compared with the ordinary simplified Dilithium-style signature, LAS adds PreSign (+6.7% over Sign), PreVerify (+2.1% over Verify) and Adapt (+7.9% over Verify), plus Ext, which extracts the witness s = z - z_hat in about 98 microseconds; the adaptor therefore costs roughly one extra signing pass plus a few verification-scale operations.

2. **The final signature does not grow.** The ordinary signature, the pre-signature and the adapted signature have the same 6752 bytes, because Adapt sets z = z_hat + r' (it changes the response value, not the serialized structure).

3. **Extra communication.** The one extra object LAS puts on the wire is the public statement Y = t' (4416 bytes, the same size as the public key, 4416 bytes); its witness r' (704 bytes) is the signer's private companion and is never published. The signature itself is unchanged. paper-derived / L2-like / L3-like / L5-like are engineering benchmark settings only, not formal NIST / ML-DSA security levels.
