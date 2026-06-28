/////////////////////////////////////////////////////////////////////////
//
//  Bochs x86-64 JIT internals
//
/////////////////////////////////////////////////////////////////////////

#ifndef BX_JIT_INTERNAL_H
#define BX_JIT_INTERNAL_H

#if BX_SUPPORT_JIT

class BX_CPU_C;
struct bxICacheEntry_c;

#include "jit_cpu_offsets.h"

#define BX_JIT_CACHE_SIZE 65536

enum {
  BX_JIT_OP_NONE = 0,
  BX_JIT_OP_ALU_RM,
  BX_JIT_OP_LOGIC_RM,
  BX_JIT_OP_MOV_RM,
  BX_JIT_OP_IMM_RM,
  BX_JIT_OP_UNARY_RM,
  BX_JIT_OP_BRANCH,
  BX_JIT_OP_NOP,
};

// Compiled trace entry point. Returns 0 on success, non-zero if cpu->async_event is set.
typedef Bit32u (*bx_jit_trace_func_t)(BX_CPU_C *cpu);

struct bx_jit_block {
  bx_phy_address pAddr;
  Bit32u fetchModeMask;
  Bit32u traceMask;
  bx_jit_trace_func_t func;
  bool compiled;
  bool jitable;
};

class bx_jit_code_buffer {
  Bit8u *base;
  Bit8u *ptr;
  Bit8u *end;
  bool   owned;

public:
  bx_jit_code_buffer();
 ~bx_jit_code_buffer();

  bool init(Bit32u size);
  void cleanup();

  Bit8u *alloc(Bit32u size);
  void  seal_page(void *start, Bit32u size);
  void  reset(void);
};

class bx_jit_x64_emitter {
  bx_jit_code_buffer *buf;
  Bit8u *start;
  Bit8u *ptr;

public:
  bx_jit_x64_emitter(bx_jit_code_buffer *b);

  Bit8u *get_ptr(void) const { return ptr; }
  Bit8u *get_start(void) const { return start; }
  Bit32u size(void) const { return (Bit32u)(ptr - start); }

  void emit8(Bit8u b);
  void emit32(Bit32u v);

  void push_reg(int r);
  void pop_reg(int r);

  void mov_reg64_imm64(int dst, Bit64u imm);
  void mov_reg64_reg64(int dst, int src);
  void mov_reg64_mem64(int dst, int base, int disp32);
  void mov_mem64_reg64(int base, int disp32, int src);
  void mov_reg32_mem32(int dst, int base, int disp32);
  void mov_mem32_reg32(int base, int disp32, int src);
  void mov_reg32_imm32(int dst, Bit32u imm);
  void add_reg32_imm8(int dst, Bit8u imm);
  void sub_reg32_imm8(int dst, Bit8u imm);
  void add_reg64_imm8(int dst, Bit8u imm);
  void sub_reg64_imm8(int dst, Bit8u imm);
  void add_mem32_imm8(int base, int disp32, Bit8u imm);
  void add_mem64_imm8(int base, int disp32, Bit8u imm);
  void add_reg64_reg64(int dst, int src);
  void sub_reg64_reg64(int dst, int src);
  void and_reg64_reg64(int dst, int src);
  void or_reg64_reg64(int dst, int src);
  void xor_reg64_reg64(int dst, int src);
  void not_reg32_mem32(int base, int disp32);
  void neg_reg32_mem32(int base, int disp32);
  void inc_mem32(int base, int disp32);
  void dec_mem32(int base, int disp32);

  void test_reg64_reg64(int a, int b);
  void cmp_reg32_mem32(int reg, int base, int disp32);

  void call_reg64(int r);
  void call_rel32(void *target);
  void ret(void);

  // cond: 0x84=o, 0x85=no, 0x82=b/c, 0x83/0x86/0x87 nb/ae/be/a, 0x88/e s/ns, 0x84/0x85 z/nz
  void jcc8(int cond, Bit8s rel8);
  void jmp_rel32(void *target);

  void load_cpu_arg(int reg); // mov reg, [cpu ptr passed in ABI arg0]
};

bool bx_jit_can_compile_cpu(BX_CPU_C *cpu);
bool bx_jit_trace_jitable(BX_CPU_C *cpu, bxICacheEntry_c *entry);

bx_jit_block *bx_jit_cache_lookup(bx_phy_address pAddr, Bit32u fetchModeMask);
bx_jit_block *bx_jit_cache_get_slot(bx_phy_address pAddr, Bit32u fetchModeMask, Bit32u traceMask);

bool bx_jit_compile_trace(BX_CPU_C *cpu, bxICacheEntry_c *entry, bx_jit_block *block);

extern Bit8u bx_jit_get_opcode_class(unsigned ia_opcode);
extern bool bx_jit_opcode_supported(unsigned ia_opcode, bool mod_c0);
extern bool bx_jit_opcode_use_helper(unsigned ia_opcode, bool mod_c0);
extern bool bx_jit_opcode_64bit(unsigned ia_opcode);
extern Bit32u bx_jit_insn_opsize(unsigned ia_opcode);

extern bx_jit_code_buffer bx_jit_codebuf;

#endif // BX_SUPPORT_JIT

#endif
