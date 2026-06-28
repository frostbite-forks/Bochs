/////////////////////////////////////////////////////////////////////////
//
//  Minimal x86-64 code emitter for Bochs JIT
//
/////////////////////////////////////////////////////////////////////////

#include "jit_include.h"

#if BX_SUPPORT_JIT

#if defined(_WIN32) || defined(__CYGWIN__)
#define BX_JIT_CPU_ARG_REG 1  // RCX
#else
#define BX_JIT_CPU_ARG_REG 7  // RDI
#endif

// host reg numbers: 0=rax 1=rcx 2=rdx 3=rbx 4=rsp 5=rbp 6=rsi 7=rdi
#define BX_HOST_RAX 0
#define BX_HOST_RCX 1
#define BX_HOST_RDX 2
#define BX_HOST_RBX 3
#define BX_HOST_RSI 6
#define BX_HOST_RDI 7

static Bit8u rex_w_bit(int reg, int rm)
{
  Bit8u rex = 0x40;
  if (reg >= 8) rex |= 0x04;
  if (rm >= 8) rex |= 0x01;
  return rex | 0x08; // REX.W
}

static Bit8u rex_r_bit(int reg, int rm)
{
  Bit8u rex = 0x40;
  if (reg >= 8) rex |= 0x04;
  if (rm >= 8) rex |= 0x01;
  return rex;
}

bx_jit_x64_emitter::bx_jit_x64_emitter(bx_jit_code_buffer *b)
{
  buf = b;
  start = b->alloc(4096);
  if (! start) {
    ptr = start = NULL;
    return;
  }
  ptr = start;
}

void bx_jit_x64_emitter::emit8(Bit8u b)
{
  *ptr++ = b;
}

void bx_jit_x64_emitter::emit32(Bit32u v)
{
  ptr[0] = (Bit8u) v;
  ptr[1] = (Bit8u)(v >> 8);
  ptr[2] = (Bit8u)(v >> 16);
  ptr[3] = (Bit8u)(v >> 24);
  ptr += 4;
}

void bx_jit_x64_emitter::push_reg(int r)
{
  if (r >= 8) emit8(0x41);
  emit8(0x50 | (r & 7));
}

void bx_jit_x64_emitter::pop_reg(int r)
{
  if (r >= 8) emit8(0x41);
  emit8(0x58 | (r & 7));
}

void bx_jit_x64_emitter::mov_reg64_imm64(int dst, Bit64u imm)
{
  emit8(rex_w_bit(dst, 0));
  emit8(0xB8 | (dst & 7));
  emit32((Bit32u) imm);
  emit32((Bit32u)(imm >> 32));
}

void bx_jit_x64_emitter::mov_reg64_reg64(int dst, int src)
{
  emit8(rex_w_bit(dst, src));
  emit8(0x89);
  emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void bx_jit_x64_emitter::mov_reg64_mem64(int dst, int base, int disp32)
{
  emit8(rex_w_bit(dst, base));
  emit8(0x8B);
  emit8(0x80 | ((dst & 7) << 3) | (base & 7));
  emit32(disp32);
}

void bx_jit_x64_emitter::mov_mem64_reg64(int base, int disp32, int src)
{
  emit8(rex_w_bit(src, base));
  emit8(0x89);
  emit8(0x80 | ((src & 7) << 3) | (base & 7));
  emit32(disp32);
}

void bx_jit_x64_emitter::mov_reg32_mem32(int dst, int base, int disp32)
{
  if (dst >= 8 || base >= 8) emit8(rex_r_bit(dst, base));
  emit8(0x8B);
  emit8(0x80 | ((dst & 7) << 3) | (base & 7));
  emit32(disp32);
}

void bx_jit_x64_emitter::mov_mem32_reg32(int base, int disp32, int src)
{
  if (src >= 8 || base >= 8) emit8(rex_r_bit(src, base));
  emit8(0x89);
  emit8(0x80 | ((src & 7) << 3) | (base & 7));
  emit32(disp32);
}

void bx_jit_x64_emitter::mov_reg32_imm32(int dst, Bit32u imm)
{
  if (dst >= 8) emit8(0x41);
  emit8(0xB8 | (dst & 7));
  emit32(imm);
}

void bx_jit_x64_emitter::add_reg32_imm8(int dst, Bit8u imm)
{
  if (dst >= 8) emit8(0x41);
  emit8(0x83);
  emit8(0xC0 | (dst & 7));
  emit8(imm);
}

void bx_jit_x64_emitter::add_reg64_reg64(int dst, int src)
{
  emit8(rex_w_bit(dst, src));
  emit8(0x01);
  emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void bx_jit_x64_emitter::sub_reg64_reg64(int dst, int src)
{
  emit8(rex_w_bit(dst, src));
  emit8(0x29);
  emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void bx_jit_x64_emitter::and_reg64_reg64(int dst, int src)
{
  emit8(rex_w_bit(dst, src));
  emit8(0x21);
  emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void bx_jit_x64_emitter::or_reg64_reg64(int dst, int src)
{
  emit8(rex_w_bit(dst, src));
  emit8(0x09);
  emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void bx_jit_x64_emitter::xor_reg64_reg64(int dst, int src)
{
  emit8(rex_w_bit(dst, src));
  emit8(0x31);
  emit8(0xC0 | ((src & 7) << 3) | (dst & 7));
}

void bx_jit_x64_emitter::not_reg32_mem32(int base, int disp32)
{
  if (base >= 8) emit8(0x41);
  emit8(0xF7);
  emit8(0x90 | (base & 7));
  emit32(disp32);
}

void bx_jit_x64_emitter::neg_reg32_mem32(int base, int disp32)
{
  if (base >= 8) emit8(0x41);
  emit8(0xF7);
  emit8(0xB0 | (base & 7));
  emit32(disp32);
}

void bx_jit_x64_emitter::inc_mem32(int base, int disp32)
{
  if (base >= 8) emit8(0x41);
  emit8(0xFF);
  emit8(0x80 | (base & 7));
  emit32(disp32);
}

void bx_jit_x64_emitter::dec_mem32(int base, int disp32)
{
  if (base >= 8) emit8(0x41);
  emit8(0xFF);
  emit8(0x88 | (base & 7));
  emit32(disp32);
}

void bx_jit_x64_emitter::test_reg64_reg64(int a, int b)
{
  emit8(rex_w_bit(a, b));
  emit8(0x85);
  emit8(0xC0 | ((b & 7) << 3) | (a & 7));
}

void bx_jit_x64_emitter::cmp_reg32_mem32(int reg, int base, int disp32)
{
  if (reg >= 8 || base >= 8) emit8(rex_r_bit(reg, base));
  emit8(0x3B);
  emit8(0x80 | ((reg & 7) << 3) | (base & 7));
  emit32(disp32);
}

void bx_jit_x64_emitter::call_reg64(int r)
{
  if (r >= 8) emit8(0x41);
  emit8(0xFF);
  emit8(0xD0 | (r & 7));
}

void bx_jit_x64_emitter::call_rel32(void *target)
{
  emit8(0xE8);
  Bit32u rel = (Bit32u)((Bit8u *)target - (ptr + 4));
  emit32(rel);
}

void bx_jit_x64_emitter::ret(void)
{
  emit8(0xC3);
}

void bx_jit_x64_emitter::jcc8(int cond, Bit8s rel8)
{
  emit8(0x70 | (cond & 0x0F));
  emit8((Bit8u) rel8);
}

void bx_jit_x64_emitter::jmp_rel32(void *target)
{
  emit8(0xE9);
  Bit32u rel = (Bit32u)((Bit8u *)target - (ptr + 4));
  emit32(rel);
}

void bx_jit_x64_emitter::load_cpu_arg(int reg)
{
  mov_reg64_reg64(reg, BX_JIT_CPU_ARG_REG);
}

#endif
