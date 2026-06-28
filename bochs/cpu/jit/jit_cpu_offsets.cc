/////////////////////////////////////////////////////////////////////////
//
//  CPU field offsets for JIT
//
/////////////////////////////////////////////////////////////////////////

#define NEED_CPU_REG_SHORTCUTS 1
#include "jit_include.h"
#include "jit_cpu_offsets.h"
#include "tlb.h"

#if BX_SUPPORT_JIT

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

unsigned bx_jit_cpu_layout::gen_reg_erx(unsigned index)
{
  return (unsigned)(offsetof(BX_CPU_C, gen_reg) + index * sizeof(bx_gen_reg_t) +
         offsetof(bx_gen_reg_t, dword.erx));
}

unsigned bx_jit_cpu_layout::gen_reg_rrx(unsigned index)
{
  return (unsigned)(offsetof(BX_CPU_C, gen_reg) + index * sizeof(bx_gen_reg_t) +
         offsetof(bx_gen_reg_t, rrx));
}

unsigned bx_jit_cpu_layout::rip_offset(void)
{
  return gen_reg_rrx(BX_64BIT_REG_RIP);
}

unsigned bx_jit_cpu_layout::eip_offset(void)
{
  return rip_offset();
}

unsigned bx_jit_cpu_layout::icount_offset(void)
{
  return (unsigned)offsetof(BX_CPU_C, icount);
}

unsigned bx_jit_cpu_layout::async_event_offset(void)
{
  return (unsigned)offsetof(BX_CPU_C, async_event);
}

unsigned bx_jit_cpu_layout::prev_rip_offset(void)
{
  return (unsigned)offsetof(BX_CPU_C, prev_rip);
}

unsigned bx_jit_cpu_layout::oszapc_result_offset(void)
{
  return (unsigned)offsetof(BX_CPU_C, oszapc) +
         (unsigned)offsetof(bx_lazyflags_entry, result);
}

unsigned bx_jit_cpu_layout::oszapc_auxbits_offset(void)
{
  return (unsigned)offsetof(BX_CPU_C, oszapc) +
         (unsigned)offsetof(bx_lazyflags_entry, auxbits);
}

unsigned bx_jit_cpu_layout::cpu_mode_offset(void)
{
  return (unsigned)offsetof(BX_CPU_C, cpu_mode);
}

unsigned bx_jit_cpu_layout::user_pl_offset(void)
{
  return (unsigned)offsetof(BX_CPU_C, user_pl);
}

unsigned bx_jit_cpu_layout::dtlb_base_offset(void)
{
  return (unsigned)offsetof(BX_CPU_C, DTLB) +
         (unsigned)offsetof(TLB<2048>, entry);
}

unsigned bx_jit_cpu_layout::dtlb_entry_size(void)
{
  return (unsigned)sizeof(bx_TLB_entry);
}

unsigned bx_jit_cpu_layout::tlb_lpf_offset(void)
{
  return (unsigned)offsetof(bx_TLB_entry, lpf);
}

unsigned bx_jit_cpu_layout::tlb_hostPageAddr_offset(void)
{
  return (unsigned)offsetof(bx_TLB_entry, hostPageAddr);
}

unsigned bx_jit_cpu_layout::tlb_accessBits_offset(void)
{
  return (unsigned)offsetof(bx_TLB_entry, accessBits);
}

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#endif
