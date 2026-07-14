#ifndef RELATION_H
#define RELATION_H

/*
 * relation.{c,h} -- the HARD-RELATION layer of LAS (eprint 2020/845): the
 * statement-witness generator Gen for the relation
 *   R_A = { (Y, y) = (t', r') : t' = A r', ||r'||inf <= 1 }   (Table 1),
 * together with the protocol types it OWNS, `statement` and `witness`
 * (physically defined in las_types.h so the codec below both schemes can see
 * them; this file is their owner).  C twin of rust/fips204-las/src/relation.rs.
 *
 * Paper model (Section 3, p.7): "The statement-witness generation Gen for R_A
 * runs exactly as KeyGen."  So relation_gen_seed is the SAME mathematics as
 * basesig.c's base_keygen_seed (= the old las_keypair_seed) -- r' <- S_1^(n+ell);
 * t' = A r' -- but it constructs the DISTINCT relation types (statement, witness),
 * never (public_key, secret_key): a statement is pk-shaped yet the API keeps the
 * two non-interchangeable (no casts, no aliases).
 *
 * This layer sits between setup and the two schemes
 * (setup.h -> relation.h / serialize.h -> basesig.c / las.c): las.c consumes
 * statements and witnesses; basesig.c never sees them.  Extracted witnesses
 * (Ext's s = z - z^, las_ext) live in the EXTENDED relation R'_A
 * (||.||inf <= 2(gamma-kappa), the knowledge gap, p.9) -- same `witness` type,
 * not assumed ternary.
 *
 * NO INVENTED HELPERS: the static helpers at the bottom are VERBATIM twins of
 * the las.c / basesig.c helpers of the same name (prefix relation_ instead of
 * las_ / b_), each a one-to-one twin of a NAMED upstream poly.c/polyvec.c
 * function.  Duplication instead of sharing keeps the layering acyclic, exactly
 * like basesig.c and las.c keep their own local copies for independent linkability.
 */

#include <stdint.h>
#include "setup.h"      /* SHARED system layer: construction parameters + public_params */
#include "las_types.h"  /* the six protocol object types (statement/witness owned here) */

/* Gen -> (Y, r_prime) in R_A: random path (fresh seed, then the deterministic
 * body).  Same sampling/arithmetic as base_keygen (basesig.c) but a DISTINCT
 * algorithm constructing (statement, witness), NOT a key pair.  The witness is
 * the paper's r' (never `y` -- the S_gamma mask -- and never `s` -- Ext's
 * extracted witness).  Returns 0 (success). */
int relation_gen(statement *Y, witness *r_prime, const public_params *pp);

/* Deterministic Gen from an explicit 32-byte seed (reproducible KATs).
 * Same deterministic sampling and arithmetic as base_keygen_seed (basesig.c):
 * r' <- S_1^(n+ell); Y = t' = A r' -- the ONE private generation core the paper
 * prescribes ("Gen runs exactly as KeyGen", Section 3), constructing the
 * DISTINCT (statement, witness) pair.  Returns 0 (success). */
int relation_gen_seed(statement *Y, witness *r_prime, const public_params *pp,
                      const uint8_t seed[LAS_SEEDBYTES]);

#endif
