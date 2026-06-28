/////////////////////////////////////////////////////////////////////////
//
//  CPU field offsets for JIT
//
/////////////////////////////////////////////////////////////////////////

#define NEED_CPU_REG_SHORTCUTS 1
#include "jit_include.h"
#include "jit_cpu_offsets.h"

#if BX_SUPPORT_JIT

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

unsigned bx_jit_cpu_layout::eip_offset(void)
{
  return gen_reg_erx(BX_32BIT_REG_EIP);
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

#endif
