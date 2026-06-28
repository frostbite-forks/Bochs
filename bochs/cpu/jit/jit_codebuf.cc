/////////////////////////////////////////////////////////////////////////
//
//  JIT executable code buffer (W^X)
//
/////////////////////////////////////////////////////////////////////////

#include "jit_include.h"

#if BX_SUPPORT_JIT

bx_jit_code_buffer bx_jit_codebuf;

#if defined(WIN32) || defined(__CYGWIN__)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

bx_jit_code_buffer::bx_jit_code_buffer()
{
  base = ptr = end = NULL;
  owned = false;
}

bx_jit_code_buffer::~bx_jit_code_buffer()
{
  cleanup();
}

bool bx_jit_code_buffer::init(Bit32u size)
{
  cleanup();

#if defined(WIN32) || defined(__CYGWIN__)
  base = (Bit8u *) VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (! base) return false;
#else
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) page_size = 4096;
  size = (size + (Bit32u)page_size - 1) & ~((Bit32u)page_size - 1);
  base = (Bit8u *) mmap(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
#if defined(MAP_FAILED)
  if (base == MAP_FAILED) {
    base = NULL;
    return false;
  }
#endif
#endif

  ptr = base;
  end = base + size;
  owned = true;
  return true;
}

void bx_jit_code_buffer::cleanup()
{
  if (! owned || ! base) {
    base = ptr = end = NULL;
    owned = false;
    return;
  }

#if defined(WIN32) || defined(__CYGWIN__)
  VirtualFree(base, 0, MEM_RELEASE);
#else
  munmap(base, (size_t)(end - base));
#endif

  base = ptr = end = NULL;
  owned = false;
}

Bit8u *bx_jit_code_buffer::alloc(Bit32u size)
{
  Bit8u *p = ptr;
  if (ptr + size > end) return NULL;
  ptr += size;
  return p;
}

void bx_jit_code_buffer::seal_page(void *start, Bit32u size)
{
#if defined(WIN32) || defined(__CYGWIN__)
  DWORD old;
  VirtualProtect(start, size, PAGE_EXECUTE_READ, &old);
  FlushInstructionCache(GetCurrentProcess(), start, size);
#else
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) page_size = 4096;
  Bit8u *s = (Bit8u *)start;
  Bit8u *e = s + size;
  Bit8u *page = (Bit8u *)((Bit64u)s & ~((Bit64u)page_size - 1));
  while (page < e) {
    mprotect(page, (size_t)page_size, PROT_READ | PROT_EXEC);
    page += page_size;
  }
#endif
}

void bx_jit_code_buffer::reset(void)
{
  ptr = base;
}

#endif
