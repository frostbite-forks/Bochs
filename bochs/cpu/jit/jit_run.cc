/////////////////////////////////////////////////////////////////////////
//
//  JIT dispatch from cpu_loop
//
/////////////////////////////////////////////////////////////////////////

#define NEED_CPU_REG_SHORTCUTS 1
#include "bochs.h"
#include "cpu.h"
#define LOG_THIS BX_CPU_THIS_PTR

#if BX_SUPPORT_JIT

#include "jit.h"
#include "jit_internal.h"

bool bx_jit_run_trace(BX_CPU_C *cpu, bxICacheEntry_c *entry)
{
  if (! bx_jit_can_compile_cpu(cpu)) return false;

  bx_phy_address pAddr = entry->pAddr;
  Bit32u fetchModeMask = BX_CPU_THIS_PTR fetchModeMask;

  bx_jit_block *block = bx_jit_cache_lookup(pAddr, fetchModeMask);

  if (! block || ! block->compiled) {
    if (! bx_jit_trace_jitable(cpu, entry)) {
      if (block) block->jitable = false;
      return false;
    }

    block = bx_jit_cache_get_slot(pAddr, fetchModeMask, entry->traceMask);

    if (! bx_jit_compile_trace(cpu, entry, block)) {
      block->compiled = false;
      block->jitable = false;
      return false;
    }

#if BX_JIT_VERIFY
    BX_INFO(("JIT compiled trace at " FMT_PHY_ADDRX " len=%u",
             pAddr, entry->tlen));
#endif
  }

  if (! block->func) return false;

#if BX_JIT_VERIFY
  // Shadow mode: also execute via interpreter and compare final GPR state.
  bx_gen_reg_t saved_gen[BX_GENERAL_REGISTERS+4];
  bx_lazyflags_entry saved_lf = BX_CPU_THIS_PTR oszapc;
  bx_address saved_rip = RIP;
  Bit64u saved_icount = BX_CPU_THIS_PTR icount;

  memcpy(saved_gen, BX_CPU_THIS_PTR gen_reg, sizeof(saved_gen));

  Bit32u jit_result = block->func(cpu);

  bx_gen_reg_t after_jit[BX_GENERAL_REGISTERS+4];
  bx_lazyflags_entry after_lf = BX_CPU_THIS_PTR oszapc;
  bx_address after_rip = RIP;
  Bit64u after_icount = BX_CPU_THIS_PTR icount;

  memcpy(after_jit, BX_CPU_THIS_PTR gen_reg, sizeof(after_jit));

  memcpy(BX_CPU_THIS_PTR gen_reg, saved_gen, sizeof(saved_gen));
  BX_CPU_THIS_PTR oszapc = saved_lf;
  RIP = saved_rip;
  BX_CPU_THIS_PTR icount = saved_icount;
  BX_CPU_THIS_PTR async_event = 0;

  bxInstruction_c *i = entry->i;
  bxInstruction_c *last = i + entry->tlen;
  for (; i < last; i++) {
    if (BX_CPU_THIS_PTR jit_invoke_one(i))
      break;
  }

  if (memcmp(after_jit, BX_CPU_THIS_PTR gen_reg, sizeof(after_jit)) != 0 ||
      after_lf.result != BX_CPU_THIS_PTR oszapc.result ||
      after_lf.auxbits != BX_CPU_THIS_PTR oszapc.auxbits ||
      after_rip != RIP ||
      after_icount != BX_CPU_THIS_PTR icount) {
    BX_PANIC(("JIT verify mismatch at " FMT_PHY_ADDRX, pAddr));
  }

  (void) jit_result;
  return true;
#else
  Bit32u result = block->func(cpu);

  if (result)
    BX_CPU_THIS_PTR async_event &= ~BX_ASYNC_EVENT_STOP_TRACE;

  return true;
#endif
}

#endif
