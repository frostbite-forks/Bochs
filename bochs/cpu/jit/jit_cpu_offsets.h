/////////////////////////////////////////////////////////////////////////
//
//  CPU structure field offsets for JIT code generation
//
/////////////////////////////////////////////////////////////////////////

#ifndef BX_JIT_CPU_OFFSETS_H
#define BX_JIT_CPU_OFFSETS_H

#if BX_SUPPORT_JIT

struct bx_jit_cpu_layout {
  static unsigned gen_reg_erx(unsigned index);
  static unsigned gen_reg_rrx(unsigned index);
  static unsigned eip_offset(void);
  static unsigned icount_offset(void);
  static unsigned async_event_offset(void);
  static unsigned prev_rip_offset(void);
  static unsigned oszapc_result_offset(void);
  static unsigned oszapc_auxbits_offset(void);
  static unsigned cpu_mode_offset(void);
};

#endif

#endif
