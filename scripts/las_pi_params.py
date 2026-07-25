# LAS atomic-swap relation proof (pi) -- eprint 2020/845 Section 4.1 / Fig. 1.
#
# Proves knowledge of a BINARY witness b = (r_plus || r_minus) in Rp^22 with
#
#     [A | -A] * b = t'   over  Rp = Z_p[X]/(X^256 + 1),  p = 8380417,
#
# which is equivalent to knowledge of a TERNARY r' = r_plus - r_minus with
# A r' = t' and ||r'||_inf <= 1 -- the exact statement pi must prove so the
# paper's M-SIS uniqueness argument (Section 4.1) applies.
#
# Statement dimensions: A = [I_6 | A'] in Rp^{6 x 11} (Simplified Dilithium-III
# engineering set n=6, ell=5), so [A | -A] is 6 x 22.
#
# LaZer proves per-partition either binary coefficients or an l2 bound; the
# optional wlinf is a parameter hint only (NOT proven).  We therefore use the
# binary-decomposition encoding with ONE all-binary partition.

vname = "las_pi_params"          # C variable name in the generated header

deg   = 256                      # ring degree d (= LAS_D)
mod   = 8380417                  # Dilithium prime q (= Q)
dim   = (6, 23)                  # [A | -A | 0] in Rp^(6,23)

# The witness is (r_plus || r_minus || e): 22 binary polys plus ONE dummy poly e
# bound to the all-zero 23rd matrix column.  The dummy exists because the
# lin-codegen pipeline requires at least one l2-bounded partition (an all-binary
# witness makes alpha = 0 and crashes; every shipped spec mixes binary with l2).
# The zero column means e contributes nothing to the relation, so soundness of
# the binary-decomposition statement is unaffected; the honest prover sets e = 0.
wpart = [ list(range(0, 22)), [22] ]
wl2   = [ 0,  16 ]               # dummy partition: l2(e) <= 16 (honest e = 0)
wbin  = [ 1,   0 ]               # real partition: coefficients proven BINARY
wrej  = [ 0,   0 ]               # witness is fresh per swap: no rejection sampling
wlinf = 1                        # a-priori linf hint (tightens parameters only)
