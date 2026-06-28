/////////////////////////////////////////////////////////////////////////
//
//  JIT trace cache
//
/////////////////////////////////////////////////////////////////////////

#include "bochs.h"

#if BX_SUPPORT_JIT

#include "jit/jit.h"
#include "jit/jit_internal.h"

static bx_jit_block bx_jit_cache[BX_JIT_CACHE_SIZE];

static Bit32u bx_jit_hash(bx_phy_address pAddr, Bit32u fetchModeMask)
{
  return (Bit32u)((pAddr >> 4) ^ (pAddr >> 12) ^ fetchModeMask) & (BX_JIT_CACHE_SIZE - 1);
}

bx_jit_block *bx_jit_cache_lookup(bx_phy_address pAddr, Bit32u fetchModeMask)
{
  Bit32u idx = bx_jit_hash(pAddr, fetchModeMask);
  for (Bit32u n = 0; n < 8; n++) {
    bx_jit_block *b = &bx_jit_cache[(idx + n) & (BX_JIT_CACHE_SIZE - 1)];
    if (b->compiled && b->pAddr == pAddr && b->fetchModeMask == fetchModeMask)
      return b;
  }
  return NULL;
}

bx_jit_block *bx_jit_cache_get_slot(bx_phy_address pAddr, Bit32u fetchModeMask, Bit32u traceMask)
{
  Bit32u idx = bx_jit_hash(pAddr, fetchModeMask);
  bx_jit_block *free_slot = NULL;

  for (Bit32u n = 0; n < 8; n++) {
    bx_jit_block *b = &bx_jit_cache[(idx + n) & (BX_JIT_CACHE_SIZE - 1)];
    if (b->compiled && b->pAddr == pAddr && b->fetchModeMask == fetchModeMask)
      return b;
    if (! b->compiled && ! free_slot)
      free_slot = b;
  }

  if (! free_slot)
    free_slot = &bx_jit_cache[idx];

  free_slot->pAddr = pAddr;
  free_slot->fetchModeMask = fetchModeMask;
  free_slot->traceMask = traceMask;
  free_slot->func = NULL;
  free_slot->compiled = false;
  free_slot->jitable = false;
  return free_slot;
}

void bx_jit_flush_all(void)
{
  for (Bit32u i = 0; i < BX_JIT_CACHE_SIZE; i++)
    bx_jit_cache[i].compiled = false;
  bx_jit_codebuf.reset();
}

void bx_jit_flush_page(bx_phy_address pAddr, Bit32u mask)
{
  (void) mask;
  Bit32u page_hash = (Bit32u)(pAddr >> 12);

  for (Bit32u i = 0; i < BX_JIT_CACHE_SIZE; i++) {
    bx_jit_block *b = &bx_jit_cache[i];
    if (b->compiled && (b->pAddr >> 12) == page_hash)
      b->compiled = false;
  }
}

void bx_jit_init(void)
{
  for (Bit32u i = 0; i < BX_JIT_CACHE_SIZE; i++) {
    bx_jit_cache[i].pAddr = BX_ICACHE_INVALID_PHY_ADDRESS;
    bx_jit_cache[i].compiled = false;
  }
  bx_jit_codebuf.init(4 * 1024 * 1024);
}

void bx_jit_cleanup(void)
{
  bx_jit_flush_all();
  bx_jit_codebuf.cleanup();
}

#endif
