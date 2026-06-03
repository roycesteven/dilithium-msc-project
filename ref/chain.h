#ifndef CHAIN_H
#define CHAIN_H

/*
 * A minimal "blockchain" abstraction to demonstrate LAS in a realistic
 * scriptless-script setting: account balances, a block height (for timeouts),
 * and adaptor-locked outputs ("swap contracts") that replace HTLCs.
 *
 * A swap contract is the scriptless analogue of a Hash-Time-Locked Contract:
 *   - the "hash lock" is an adaptor statement Y (claiming requires the witness y);
 *   - the "time lock" is a timeout block height (the funder may refund after it).
 * The chain stores only PUBLIC data (public keys, statements, (pre-)signatures);
 * the secret keys and the Adapt/PreSign steps live in the parties' wallets
 * (the demo driver), exactly as on a real chain.
 */

#include <stdint.h>
#include <stddef.h>
#include "las.h"

#define CHAIN_MAXACC   8
#define CHAIN_MAXC     8
#define CHAIN_NAMELEN  16
#define CHAIN_MSGLEN   64

typedef enum { SWAP_OPEN = 0, SWAP_CLAIMED = 1, SWAP_REFUNDED = 2 } swap_state;

typedef struct {
  int from, to;                    /* account indices: funder -> beneficiary   */
  long amount;
  las_pk  Y;                       /* adaptor statement (the scriptless lock)   */
  las_sig presig;                  /* funder's pre-signature on claimmsg, bound to Y */
  uint8_t claimmsg[CHAIN_MSGLEN];  /* the claim "transaction" pre-authorised    */
  size_t  claimlen;
  uint64_t timeout;                /* refundable once height >= timeout         */
  swap_state state;
  las_sig adapted;                 /* published adapted signature (set on claim) */
  int adapted_set;
} swap_contract;

typedef struct {
  char name[CHAIN_NAMELEN];
  long balance;
  const las_pk *pk;                /* identity (caller-owned)                   */
} chain_account;

typedef struct {
  char name[CHAIN_NAMELEN];
  uint64_t height;                 /* current block height                      */
  chain_account acc[CHAIN_MAXACC];
  int nacc;
  swap_contract c[CHAIN_MAXC];
  int nc;
  const las_pp *pp;                /* shared public parameters (matrix A)       */
} chain;

void chain_init(chain *ch, const char *name, const las_pp *pp);
int  chain_account_add(chain *ch, const char *name, const las_pk *pk, long balance);
void chain_advance(chain *ch, uint64_t blocks);
long chain_balance(const chain *ch, int acc);

/* Funder `from` locks `amount` and submits a pre-signature `presig` over
 * `claimmsg` bound to `Y`. The chain PRE-VERIFIES the pre-signature before
 * escrowing the funds. Returns contract id, or -1 on failure. */
int  chain_fund_swap(chain *ch, int from, int to, long amount,
                     const las_pk *Y, const las_sig *presig,
                     const uint8_t *claimmsg, size_t claimlen, uint64_t timeout);

/* AMHL variant: identical to chain_fund_swap but pre-verifies with the K-hop
 * bound g-k-K (las_preverify_k), as required when the route has K hops and the
 * cumulative witness opening a hop may have infinity-norm up to K. */
int  chain_fund_swap_k(chain *ch, int from, int to, long amount,
                       const las_pk *Y, const las_sig *presig,
                       const uint8_t *claimmsg, size_t claimlen, uint64_t timeout,
                       unsigned int nhops);

/* Beneficiary submits an `adapted` signature. The chain VERIFIES it against the
 * pre-authorised claim message and the funder's public key; on success it pays
 * the beneficiary, records the adapted signature on-chain, and marks it CLAIMED
 * (which reveals the witness to anyone watching). Returns 0 on success. */
int  chain_claim_swap(chain *ch, int cid, const las_sig *adapted);

/* Recover the witness from a claimed contract (anyone watching can do this). */
int  chain_extract_witness(const chain *ch, int cid, las_sk *witness_out);

/* Funder reclaims escrow after the timeout if the contract is still open. */
int  chain_refund_swap(chain *ch, int cid);

#endif
