/*
 * relation_zk_labrador.c -- LaBRADOR bridge for pi (see relation_zk_labrador.h
 * for the seam contract and why LaBRADOR rather than the FRI-STARK).
 *
 * The THIRD (and last) translation unit in ref/ that includes a vendored proof
 * library, after relation_zk_lazer.c and relation_zk_lazer_batch.c.  It is
 * compiled with -I$(LAZER_DIR)/src/labrados and NO ref include path: labrados
 * owns `poly` and `N` exactly as lazer.h and params.h do.
 *
 * ⚠️ HEADER TRAP -- do not "simplify" this include.
 *   LaZer ships src/labradosNN_py.h declaring the internal ring degree as
 *   N = 64, but the labrados SUBMODULE the Makefile actually builds defines
 *   N = 256.  The struct layouts disagree, so building against the shipped
 *   _py.h header silently corrupts memory.  We include the submodule's OWN
 *   header, which is the one the library is compiled from, and must be built
 *   with the SAME -DLOGQ and -DNDEBUG as the library (labrados structs have
 *   #ifndef NDEBUG members).
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "labrados_python.h"     /* submodule header: proofsystem.h + dachshund.h */

#include "relation_zk_labrador.h"

/* The bridge is generated for the D3 engineering set; refuse a mismatched
 * build rather than encoding a statement of the wrong shape. */
#if PI_LAB_DEG != N
#error "relation_zk_labrador: PI_LAB_DEG must equal labrados' ring degree N"
#endif

static double now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

int relation_labrador_run(const int64_t *phi_w, const int64_t *phi_g,
                          const int64_t *b, const int64_t *w, const int64_t *g,
                          uint64_t w_normsq, uint64_t g_normsq, int zk,
                          int *encoding_ok, int *verified,
                          double *params_ms, double *prove_ms, double *verify_ms) {
  /* Two witness vectors, in the order LaBRADOR's parameter generation expects
   * (approx | bin | exact): the binary witness first, the l2-bounded quotient
   * second.  Lengths are in units of degree-N polynomials, so deg = 1. */
  size_t n[2]         = { PI_LAB_WCOLS, PI_LAB_GCOLS };
  uint64_t normsq[2]  = { w_normsq, g_normsq };
  uint64_t normsq_r[2] = { 0, 0 };
  normtype normty[2]  = { BIN, L2EXACT };

  witness wt;
  statement st;
  dch_pack_params pp;
  dch_pack_proof pi;
  double t0;
  int rc = 0;
  size_t m;

  if(encoding_ok) *encoding_ok = 0;
  if(verified)    *verified    = 0;

  py_init_witness(wt, 2, n);
  if(py_set_witness_vector(wt, 0, PI_LAB_WCOLS, 1, w)) { rc = 1; goto out_wt; }
  if(py_set_witness_vector(wt, 1, PI_LAB_GCOLS, 1, g)) { rc = 2; goto out_wt; }

  /* PI_LAB_ROWS full ring constraints; no Zq or integer constraints. */
  py_init_statement(st, 2, n, normsq, normsq_r, normty, PI_LAB_ROWS, 0, 0);

  for(m = 0; m < PI_LAB_ROWS; ++m) {
    const size_t idx[2] = { 0, 1 };
    const size_t len[2] = { PI_LAB_WCOLS, PI_LAB_GCOLS };
    /* append_constraint consumes phi as: vector 0's len[0] polynomials, then
     * vector 1's len[1], each PI_LAB_DEG consecutive coefficients.  The two
     * blocks must therefore be adjacent, so they are copied into one buffer. */
    int64_t phi[(PI_LAB_WCOLS + PI_LAB_GCOLS) * PI_LAB_DEG];

    memcpy(phi,
           phi_w + m * PI_LAB_WCOLS * PI_LAB_DEG,
           sizeof(int64_t) * PI_LAB_WCOLS * PI_LAB_DEG);
    memcpy(phi + PI_LAB_WCOLS * PI_LAB_DEG,
           phi_g + m * PI_LAB_GCOLS * PI_LAB_DEG,
           sizeof(int64_t) * PI_LAB_GCOLS * PI_LAB_DEG);

    if(py_append_constraint(st, 2, idx, len, 1, phi,
                            (int64_t *)(b + m * PI_LAB_DEG), 1)) {
      rc = 3;
      goto out_st;
    }
  }

  /* Encoding gate: LaBRADOR's own check that the statement holds for this
   * witness, run BEFORE any proof.  A failure here means the encoding is wrong,
   * not that the relation is false. */
  /* NOTE the convention: labrados' verify() returns 1 on SUCCESS (it starts at
   * ret = 1 and zeroes it per failed check), the opposite of the 0-on-success
   * used by py_set_witness_vector / py_append_constraint / py_gen_params in the
   * same header. Getting this backwards makes an honest instance look like a
   * broken encoding. */
  if(py_simple_verify(st, wt) == 1) {
    if(encoding_ok) *encoding_ok = 1;
  } else {
    rc = 4;
    goto out_st;
  }

  t0 = now_ms();
  if(py_gen_params(pp, st, zk, 0)) { rc = 5; goto out_st; }
  if(params_ms) *params_ms = now_ms() - t0;

  t0 = now_ms();
  py_prove(pi, st, wt, pp);
  if(prove_ms) *prove_ms = now_ms() - t0;

  t0 = now_ms();
  if(py_verify(st, pp, pi) == 1) {   /* 1 = accept, same convention as above */
    if(verified) *verified = 1;
  }
  if(verify_ms) *verify_ms = now_ms() - t0;

  py_free_proof(pi);
  py_free_params(pp);
out_st:
  py_free_statement(st);
out_wt:
  py_free_witness(wt);
  return rc;
}
