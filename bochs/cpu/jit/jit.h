/////////////////////////////////////////////////////////////////////////
//
//  Bochs x86-64 JIT (MVP)
//
/////////////////////////////////////////////////////////////////////////

#ifndef BX_JIT_H
#define BX_JIT_H

class BX_CPU_C;
struct bxICacheEntry_c;

#if BX_SUPPORT_JIT

// Return true if the trace was executed by JIT (caller should skip interpreter).
bool bx_jit_run_trace(BX_CPU_C *cpu, bxICacheEntry_c *entry);

void bx_jit_flush_all(void);
void bx_jit_flush_page(bx_phy_address pAddr, Bit32u mask);

void bx_jit_init(void);
void bx_jit_cleanup(void);

#else

BX_CPP_INLINE bool bx_jit_run_trace(BX_CPU_C *cpu, bxICacheEntry_c *entry)
{
  (void) cpu;
  (void) entry;
  return false;
}

BX_CPP_INLINE void bx_jit_flush_all(void) {}
BX_CPP_INLINE void bx_jit_flush_page(bx_phy_address pAddr, Bit32u mask)
{
  (void) pAddr;
  (void) mask;
}

BX_CPP_INLINE void bx_jit_init(void) {}
BX_CPP_INLINE void bx_jit_cleanup(void) {}

#endif

#endif
