#include <string.h>
#include "chain.h"
#include "las.h"

void chain_init(chain *ch, const char *name, const las_pp *pp) {
  size_t i;
  for(i = 0; i < CHAIN_NAMELEN - 1 && name[i]; ++i) ch->name[i] = name[i];
  ch->name[i] = 0;
  ch->height = 0;
  ch->nacc = 0;
  ch->nc = 0;
  ch->pp = pp;
}

int chain_account_add(chain *ch, const char *name, const las_pk *pk, long balance) {
  size_t i;
  chain_account *a;
  if(ch->nacc >= CHAIN_MAXACC) return -1;
  a = &ch->acc[ch->nacc];
  for(i = 0; i < CHAIN_NAMELEN - 1 && name[i]; ++i) a->name[i] = name[i];
  a->name[i] = 0;
  a->pk = pk;
  a->balance = balance;
  return ch->nacc++;
}

void chain_advance(chain *ch, uint64_t blocks) {
  ch->height += blocks;
}

long chain_balance(const chain *ch, int acc) {
  if(acc < 0 || acc >= ch->nacc) return -1;
  return ch->acc[acc].balance;
}

int chain_fund_swap(chain *ch, int from, int to, long amount,
                    const las_pk *Y, const las_sig *presig,
                    const uint8_t *claimmsg, size_t claimlen, uint64_t timeout) {
  swap_contract *k;
  if(ch->nc >= CHAIN_MAXC) return -1;
  if(from < 0 || from >= ch->nacc || to < 0 || to >= ch->nacc) return -1;
  if(amount <= 0 || ch->acc[from].balance < amount) return -1;
  if(claimlen > CHAIN_MSGLEN) return -1;

  /* The chain accepts the funding only if the funder's conditional
   * authorisation is a well-formed pre-signature bound to Y. */
  if(las_preverify(presig, claimmsg, claimlen, Y, ch->acc[from].pk, ch->pp) != 0)
    return -1;

  ch->acc[from].balance -= amount;          /* escrow */
  k = &ch->c[ch->nc];
  k->from = from; k->to = to; k->amount = amount;
  k->Y = *Y;
  k->presig = *presig;
  memcpy(k->claimmsg, claimmsg, claimlen);
  k->claimlen = claimlen;
  k->timeout = timeout;
  k->state = SWAP_OPEN;
  k->adapted_set = 0;
  return ch->nc++;
}

int chain_claim_swap(chain *ch, int cid, const las_sig *adapted) {
  swap_contract *k;
  if(cid < 0 || cid >= ch->nc) return -1;
  k = &ch->c[cid];
  if(k->state != SWAP_OPEN) return -1;

  /* The adapted signature must be a valid ordinary signature by the funder over
   * the pre-authorised claim message. */
  if(las_verify(adapted, k->claimmsg, k->claimlen, ch->acc[k->from].pk, ch->pp) != 0)
    return -1;

  ch->acc[k->to].balance += k->amount;      /* pay beneficiary */
  k->adapted = *adapted;
  k->adapted_set = 1;
  k->state = SWAP_CLAIMED;                   /* witness now revealed on-chain */
  return 0;
}

int chain_extract_witness(const chain *ch, int cid, las_sk *witness_out) {
  const swap_contract *k;
  if(cid < 0 || cid >= ch->nc) return -1;
  k = &ch->c[cid];
  if(k->state != SWAP_CLAIMED || !k->adapted_set) return -1;
  return las_ext(witness_out, &k->adapted, &k->presig, &k->Y, ch->pp);
}

int chain_refund_swap(chain *ch, int cid) {
  swap_contract *k;
  if(cid < 0 || cid >= ch->nc) return -1;
  k = &ch->c[cid];
  if(k->state != SWAP_OPEN) return -1;
  if(ch->height < k->timeout) return -1;     /* time lock not yet expired */
  ch->acc[k->from].balance += k->amount;     /* return escrow to funder */
  k->state = SWAP_REFUNDED;
  return 0;
}
