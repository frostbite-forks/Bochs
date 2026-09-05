/////////////////////////////////////////////////////////////////////////
// $Id$
/////////////////////////////////////////////////////////////////////////
//
//  Copyright (C) 2025-2026  The Bochs Project
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
/////////////////////////////////////////////////////////////////////////

#define BX_PLUGGABLE

#include "iodev.h"
#include "vgacore.h"
#include "pci.h"
#define BX_USE_BINARY_FWD_ROP
#define BX_USE_TERNARY_ROP
#include "bitblt.h"
#include "ddc.h"
#include "pxextract.h"
#include "nvriva.h"
#include "virt_timer.h"

#include "bx_debug/debug.h"

#include <math.h>

#if BX_SUPPORT_NVRIVA

#define LOG_THIS theSvga->

#if BX_USE_NVRIVA_SMF
#define VGA_READ(addr,len)       bx_vgacore_c::read_handler(theSvga,addr,len)
#define VGA_WRITE(addr,val,len)  bx_vgacore_c::write_handler(theSvga,addr,val,len)
#define SVGA_READ(addr,len)      svga_read_handler(theSvga,addr,len)
#define SVGA_WRITE(addr,val,len) svga_write_handler(theSvga,addr,val,len)
#else
#define VGA_READ(addr,len)       bx_vgacore_c::read(addr,len)
#define VGA_WRITE(addr,val,len)  bx_vgacore_c::write(addr,val,len,0)
#define SVGA_READ(addr,len)      svga_read(addr,len)
#define SVGA_WRITE(addr,val,len) svga_write(addr,val,len)
#endif // BX_USE_NVRIVA_SMF

#define RIVA_PNPMMIO_SIZE        0x1000000

#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))

static bx_nvriva_c *theSvga = NULL;

void nvriva_init_options(void)
{
  static const char* nvriva_model_list[] = {
    "tnt2m64",
    NULL
  };

  bx_param_enum_c *model = SIM->get_param_enum(BXPN_VGA_EXT_MODEL);
  model->set_enabled(1);
  model->set_label("RIVA model");
  model->set_description("Selects the RIVA TNT2 model to emulate");
  model->set_choices(nvriva_model_list, 0, 0);
}

void nvriva_cleanup_options(void)
{
  bx_param_enum_c *model = SIM->get_param_enum(BXPN_VGA_EXT_MODEL);
  model->set_choices(NULL, 0, 0);
  model->set_label("Model");
  model->set_enabled(0);
}

Bit32s nvriva_options_parser(const char *context, int num_params, char *params[])
{
  if (!strcmp(params[0], "nvriva")) {
    if (num_params > 1 && !strncmp(params[1], "model=", 6)) {
      SIM->get_param_enum(BXPN_VGA_EXT_MODEL)->set_by_name(&params[1][6]);
    } else if (num_params > 1) {
      BX_ERROR(("%s: unknown parameter for nvriva ignored.", context));
    }
  } else {
    BX_PANIC(("%s: unknown directive '%s'", context, params[0]));
  }
  return 0;
}

Bit32s nvriva_options_save(FILE *fp)
{
  return fprintf(fp, "nvriva: model=%s\n", SIM->get_param_enum(BXPN_VGA_EXT_MODEL)->get_selected());
}

PLUGIN_ENTRY_FOR_MODULE(nvriva)
{
  if (mode == PLUGIN_INIT) {
    theSvga = new bx_nvriva_c();
    bx_devices.pluginVgaDevice = theSvga;
    BX_REGISTER_DEVICE_DEVMODEL(plugin, type, theSvga, BX_PLUGIN_NVRIVA);
    nvriva_init_options();
    SIM->register_addon_option("nvriva", nvriva_options_parser, nvriva_options_save);
  } else if (mode == PLUGIN_FINI) {
    nvriva_cleanup_options();
    SIM->unregister_addon_option("nvriva");
    bx_list_c *menu = (bx_list_c*)SIM->get_param("display");
    menu->remove("nvriva");
    delete theSvga;
  } else if (mode == PLUGIN_PROBE) {
    return (int)PLUGTYPE_VGA;
  } else if (mode == PLUGIN_FLAGS) {
    return PLUGFLAG_PCI;
  }
  return 0;
}

#undef LOG_THIS
#define LOG_THIS BX_NVRIVA_THIS

bx_nvriva_c::bx_nvriva_c() : bx_vgacore_c()
{
}

bx_nvriva_c::~bx_nvriva_c()
{
  SIM->get_bochs_root()->remove("nvriva");
  BX_DEBUG(("Exit"));
}

bool bx_nvriva_c::init_vga_extension(void)
{
  if (!SIM->is_agp_device(BX_PLUGIN_NVRIVA)) {
    BX_PANIC(("RIVA TNT2 Model 64 should be plugged into AGP slot"));
  }

  BX_NVRIVA_THIS pci_enabled = true;

  BX_NVRIVA_THIS put("NVRIVA");
  BX_NVRIVA_THIS init_iohandlers(svga_read_handler, svga_write_handler, "nvriva");
  DEV_register_ioread_handler(this, svga_read_handler, 0x03B4, "nvriva", 2);
  DEV_register_ioread_handler(this, svga_read_handler, 0x03D0, "nvriva", 7);
  DEV_register_ioread_handler(this, svga_read_handler, 0x03D2, "nvriva", 2);
  DEV_register_ioread_handler(this, svga_read_handler, 0x03C3, "nvriva", 3);
  DEV_register_iowrite_handler(this, svga_write_handler, 0x03D0, "nvriva", 7);
  DEV_register_iowrite_handler(this, svga_write_handler, 0x03D2, "nvriva", 2);
  BX_NVRIVA_THIS svga_init_members();
  BX_NVRIVA_THIS svga_init_pcihandlers();
  BX_NVRIVA_THIS bitblt_init();
  BX_NVRIVA_THIS init_method_handlers();
  BX_NVRIVA_THIS s.CRTC.max_reg = RIVA_CRTC_MAX;
  BX_NVRIVA_THIS s.max_xres = 1600;
  BX_NVRIVA_THIS s.max_yres = 1200;
  BX_INFO(("RIVA TNT2 Model 64 initialized"));
#if BX_DEBUGGER
  bx_dbg_register_debug_info("nvriva", this);
#endif
  return 1;
}

#define SETUP_BITBLT(num, name, flags) \
  do { \
    BX_NVRIVA_THIS rop_handler[num] = bitblt_rop_fwd_##name; \
    BX_NVRIVA_THIS rop_flags[num] = flags; \
  } while (0);

void bx_nvriva_c::bitblt_init()
{
  for (int i = 0; i < 0x100; i++) {
    SETUP_BITBLT(i, nop, BX_ROP_PATTERN);
  }
  SETUP_BITBLT(0x00, 0, 0);
  SETUP_BITBLT(0x05, notsrc_and_notdst, BX_ROP_PATTERN);
  SETUP_BITBLT(0x0a, notsrc_and_dst, BX_ROP_PATTERN);
  SETUP_BITBLT(0x0f, notsrc, BX_ROP_PATTERN);
  SETUP_BITBLT(0x11, notsrc_and_notdst, 0);
  SETUP_BITBLT(0x22, notsrc_and_dst, 0);
  SETUP_BITBLT(0x33, notsrc, 0);
  SETUP_BITBLT(0x44, src_and_notdst, 0);
  SETUP_BITBLT(0x50, src_and_notdst, 0);
  SETUP_BITBLT(0x55, notdst, 0);
  SETUP_BITBLT(0x5a, src_xor_dst, BX_ROP_PATTERN);
  SETUP_BITBLT(0x5f, notsrc_or_notdst, BX_ROP_PATTERN);
  SETUP_BITBLT(0x66, src_xor_dst, 0);
  SETUP_BITBLT(0x77, notsrc_or_notdst, 0);
  SETUP_BITBLT(0x88, src_and_dst, 0);
  SETUP_BITBLT(0x99, src_notxor_dst, 0);
  SETUP_BITBLT(0xaa, nop, 0);
  SETUP_BITBLT(0xad, src_and_dst, BX_ROP_PATTERN);
  SETUP_BITBLT(0xaf, notsrc_or_dst, BX_ROP_PATTERN);
  SETUP_BITBLT(0xbb, notsrc_or_dst, 0);
  SETUP_BITBLT(0xcc, src, 0);
  SETUP_BITBLT(0xdd, src_and_notdst, 0);
  SETUP_BITBLT(0xee, src_or_dst, 0);
  SETUP_BITBLT(0xf0, src, BX_ROP_PATTERN);
  SETUP_BITBLT(0xf5, src_or_notdst, BX_ROP_PATTERN);
  SETUP_BITBLT(0xfa, src_or_dst, BX_ROP_PATTERN);
  SETUP_BITBLT(0xff, 1, 0);
}

void bx_nvriva_c::svga_init_members()
{
  BX_NVRIVA_THIS crtc.index = RIVA_CRTC_MAX + 1;
  for (int i = 0; i <= RIVA_CRTC_MAX; i++)
    BX_NVRIVA_THIS crtc.reg[i] = 0x00;

  BX_NVRIVA_THIS mc_soft_intr = false;
  BX_NVRIVA_THIS mc_intr_en = 0;
  BX_NVRIVA_THIS mc_enable = 0;
  BX_NVRIVA_THIS bus_intr = 0;
  BX_NVRIVA_THIS bus_intr_en = 0;
  BX_NVRIVA_THIS fifo_wait = false;
  BX_NVRIVA_THIS fifo_wait_soft = false;
  BX_NVRIVA_THIS fifo_wait_notify = false;
  BX_NVRIVA_THIS fifo_wait_flip = false;
  BX_NVRIVA_THIS fifo_wait_acquire = false;
  BX_NVRIVA_THIS fifo_intr = 0;
  BX_NVRIVA_THIS fifo_intr_en = 0;
  BX_NVRIVA_THIS fifo_ramht = 0;
  BX_NVRIVA_THIS fifo_ramfc = 0;
  BX_NVRIVA_THIS fifo_ramro = 0;
  BX_NVRIVA_THIS fifo_mode = 0;
  BX_NVRIVA_THIS fifo_cache1_push0 = 0;
  BX_NVRIVA_THIS fifo_cache1_push1 = 0;
  BX_NVRIVA_THIS fifo_cache1_put = 0;
  BX_NVRIVA_THIS fifo_cache1_dma_push = 0;
  BX_NVRIVA_THIS fifo_cache1_dma_instance = 0;
  BX_NVRIVA_THIS fifo_cache1_dma_put = 0;
  BX_NVRIVA_THIS fifo_cache1_dma_get = 0;
  BX_NVRIVA_THIS fifo_cache1_ref_cnt = 0;
  BX_NVRIVA_THIS fifo_cache1_pull0 = 0;
  BX_NVRIVA_THIS fifo_cache1_get = 0;
  for (int i = 0; i < RIVA_CACHE1_SIZE; i++) {
    BX_NVRIVA_THIS fifo_cache1_method[i] = 0;
    BX_NVRIVA_THIS fifo_cache1_data[i] = 0;
  }
  BX_NVRIVA_THIS rma_addr = 0;
  BX_NVRIVA_THIS timer_intr = 0;
  BX_NVRIVA_THIS timer_intr_en = 0;
  BX_NVRIVA_THIS timer_num = 0;
  BX_NVRIVA_THIS timer_den = 0;
  BX_NVRIVA_THIS timer_inittime1 = 0;
  BX_NVRIVA_THIS timer_inittime2 = 0;
  BX_NVRIVA_THIS timer_alarm = 0;
  BX_NVRIVA_THIS graph_intr = 0;
  BX_NVRIVA_THIS graph_nsource = 0;
  BX_NVRIVA_THIS graph_intr_en = 0;
  BX_NVRIVA_THIS graph_ctx_switch1 = 0;
  BX_NVRIVA_THIS graph_ctx_switch2 = 0;
  BX_NVRIVA_THIS graph_ctx_switch4 = 0;
  BX_NVRIVA_THIS graph_ctxctl_cur = 0;
  BX_NVRIVA_THIS graph_status = 0;
  BX_NVRIVA_THIS graph_trapped_addr = 0;
  BX_NVRIVA_THIS graph_trapped_data = 0;
  BX_NVRIVA_THIS graph_flip_read = 0;
  BX_NVRIVA_THIS graph_flip_write = 0;
  BX_NVRIVA_THIS graph_flip_modulo = 0;
  BX_NVRIVA_THIS graph_notify = 0;
  BX_NVRIVA_THIS graph_fifo = 0;
  BX_NVRIVA_THIS graph_bpixel = 0;
  BX_NVRIVA_THIS graph_channel_ctx_table = 0;
  BX_NVRIVA_THIS graph_offset0 = 0;
  BX_NVRIVA_THIS graph_pitch0 = 0;
  BX_NVRIVA_THIS crtc_intr = 0;
  BX_NVRIVA_THIS crtc_intr_en = 0;
  BX_NVRIVA_THIS crtc_start = 0;
  BX_NVRIVA_THIS crtc_config = 0;
  BX_NVRIVA_THIS crtc_raster_pos = 0;
  BX_NVRIVA_THIS crtc_cursor_offset = 0;
  BX_NVRIVA_THIS crtc_cursor_config = 0;
  BX_NVRIVA_THIS crtc_gpio_ext = 0;
  BX_NVRIVA_THIS ramdac_cu_start_pos = 0;
  BX_NVRIVA_THIS ramdac_nvpll = 0x0001DA0D;
  BX_NVRIVA_THIS ramdac_mpll = 0x0001E30D;
  BX_NVRIVA_THIS ramdac_vpll = 0x0003BE0C;
  BX_NVRIVA_THIS ramdac_pll_select = 0x00000700;
  BX_NVRIVA_THIS ramdac_general_control = 0;
  BX_NVRIVA_THIS pfb_boot_0 = 0x00000028;
  BX_NVRIVA_THIS pfb_config_0 = 0;
  BX_NVRIVA_THIS pfb_config_1 = 0;

  memset(BX_NVRIVA_THIS chs, 0x00, sizeof(BX_NVRIVA_THIS chs));
  for (int i = 0; i < RIVA_CHANNEL_COUNT; i++) {
    BX_NVRIVA_THIS chs[i].swzs_color_bytes = 1;
    BX_NVRIVA_THIS chs[i].s2d_color_bytes = 1;
    BX_NVRIVA_THIS chs[i].d3d_surface_color_bytes = 1;
    BX_NVRIVA_THIS chs[i].d3d_surface_depth_bytes = 1;
  }

  for (int i = 0; i < 4 * 1024 * 1024; i++)
    BX_NVRIVA_THIS unk_regs[i] = 0;

  BX_NVRIVA_THIS svga_unlock_special = 0;
  BX_NVRIVA_THIS svga_needs_update_tile = 1;
  BX_NVRIVA_THIS svga_needs_update_dispentire = 1;
  BX_NVRIVA_THIS svga_needs_update_mode = 0;
  BX_NVRIVA_THIS svga_double_width = 0;

  BX_NVRIVA_THIS svga_xres = 640;
  BX_NVRIVA_THIS svga_yres = 480;
  BX_NVRIVA_THIS svga_bpp = 8;
  BX_NVRIVA_THIS svga_pitch = 640;
  BX_NVRIVA_THIS bank_base[0] = 0;
  BX_NVRIVA_THIS bank_base[1] = 0;

  BX_NVRIVA_THIS hw_cursor.x = 0;
  BX_NVRIVA_THIS hw_cursor.y = 0;
  BX_NVRIVA_THIS hw_cursor.size = 32;
  BX_NVRIVA_THIS hw_cursor.offset = 0;
  BX_NVRIVA_THIS hw_cursor.bpp32 = false;
  BX_NVRIVA_THIS hw_cursor.enabled = false;

  BX_NVRIVA_THIS s.memsize = 32 * 1024 * 1024;
  BX_NVRIVA_THIS straps0_primary_original = 0x8000D5C7;
  BX_NVRIVA_THIS straps0_primary = BX_NVRIVA_THIS straps0_primary_original;
  BX_NVRIVA_THIS memsize_mask = BX_NVRIVA_THIS s.memsize - 1;
  BX_NVRIVA_THIS ramin_base = BX_NVRIVA_THIS s.memsize - (64 * 1024);
  BX_NVRIVA_THIS ramin_size = 64 * 1024;

  if (BX_NVRIVA_THIS s.memory == NULL)
    BX_NVRIVA_THIS s.memory = new Bit8u[BX_NVRIVA_THIS s.memsize];
  memset(BX_NVRIVA_THIS s.memory, 0x00, BX_NVRIVA_THIS s.memsize);
  BX_NVRIVA_THIS disp_ptr = BX_NVRIVA_THIS s.memory;
  BX_NVRIVA_THIS disp_offset = 0;
  BX_NVRIVA_THIS disp_end_offset = 0;

  BX_NVRIVA_THIS s.vclk[0] = 25180000;
  BX_NVRIVA_THIS s.vclk[1] = 28325000;
  BX_NVRIVA_THIS s.vclk[2] = 41165000;
  BX_NVRIVA_THIS s.vclk[3] = 36082000;
}

void bx_nvriva_c::reset(unsigned type)
{
  BX_NVRIVA_THIS bx_vgacore_c::reset(type);
  BX_NVRIVA_THIS svga_init_members();
  BX_NVRIVA_THIS ddc.init();
  BX_NVRIVA_THIS pci_conf[0x50] = 0x00;
}

void bx_nvriva_c::register_state(void)
{
  bx_list_c *list = new bx_list_c(SIM->get_bochs_root(), "nvriva", "RIVA TNT2 M64 State");
  BX_NVRIVA_THIS vgacore_register_state(list);
  bx_list_c *crtc = new bx_list_c(list, "crtc");
  new bx_shadow_num_c(crtc, "index", &BX_NVRIVA_THIS crtc.index, BASE_HEX);
  new bx_shadow_data_c(crtc, "reg", BX_NVRIVA_THIS crtc.reg, RIVA_CRTC_MAX + 1, 1);
  BXRS_PARAM_BOOL(list, svga_unlock_special, BX_NVRIVA_THIS svga_unlock_special);
  BXRS_PARAM_BOOL(list, svga_double_width, BX_NVRIVA_THIS svga_double_width);
  new bx_shadow_num_c(list, "svga_xres", &BX_NVRIVA_THIS svga_xres);
  new bx_shadow_num_c(list, "svga_yres", &BX_NVRIVA_THIS svga_yres);
  new bx_shadow_num_c(list, "svga_pitch", &BX_NVRIVA_THIS svga_pitch);
  new bx_shadow_num_c(list, "svga_bpp", &BX_NVRIVA_THIS svga_bpp);
  new bx_shadow_num_c(list, "svga_dispbpp", &BX_NVRIVA_THIS svga_dispbpp);
  new bx_shadow_num_c(list, "bank_base0", &BX_NVRIVA_THIS bank_base[0], BASE_HEX);
  new bx_shadow_num_c(list, "bank_base1", &BX_NVRIVA_THIS bank_base[1], BASE_HEX);
  bx_list_c *cursor = new bx_list_c(list, "hw_cursor");
  BXRS_PARAM_BOOL(cursor, enabled, BX_NVRIVA_THIS hw_cursor.enabled);
  BXRS_PARAM_BOOL(cursor, bpp32, BX_NVRIVA_THIS hw_cursor.bpp32);
  BXRS_PARAM_BOOL(cursor, vram, BX_NVRIVA_THIS hw_cursor.vram);
  new bx_shadow_num_c(cursor, "x", &BX_NVRIVA_THIS hw_cursor.x, BASE_HEX);
  new bx_shadow_num_c(cursor, "y", &BX_NVRIVA_THIS hw_cursor.y, BASE_HEX);
  new bx_shadow_num_c(cursor, "size", &BX_NVRIVA_THIS hw_cursor.size, BASE_HEX);
  new bx_shadow_num_c(cursor, "offset", &BX_NVRIVA_THIS hw_cursor.offset, BASE_HEX);
  BXRS_PARAM_BOOL(list, mc_soft_intr, BX_NVRIVA_THIS mc_soft_intr);
  new bx_shadow_num_c(list, "mc_intr_en", &BX_NVRIVA_THIS mc_intr_en, BASE_HEX);
  new bx_shadow_num_c(list, "mc_enable", &BX_NVRIVA_THIS mc_enable, BASE_HEX);
  new bx_shadow_num_c(list, "bus_intr", &BX_NVRIVA_THIS bus_intr, BASE_HEX);
  new bx_shadow_num_c(list, "bus_intr_en", &BX_NVRIVA_THIS bus_intr_en, BASE_HEX);
  BXRS_PARAM_BOOL(list, fifo_wait, BX_NVRIVA_THIS fifo_wait);
  BXRS_PARAM_BOOL(list, fifo_wait_soft, BX_NVRIVA_THIS fifo_wait_soft);
  BXRS_PARAM_BOOL(list, fifo_wait_notify, BX_NVRIVA_THIS fifo_wait_notify);
  BXRS_PARAM_BOOL(list, fifo_wait_flip, BX_NVRIVA_THIS fifo_wait_flip);
  BXRS_PARAM_BOOL(list, fifo_wait_acquire, BX_NVRIVA_THIS fifo_wait_acquire);
  new bx_shadow_num_c(list, "fifo_intr", &BX_NVRIVA_THIS fifo_intr, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_intr_en", &BX_NVRIVA_THIS fifo_intr_en, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_ramht", &BX_NVRIVA_THIS fifo_ramht, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_ramfc", &BX_NVRIVA_THIS fifo_ramfc, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_ramro", &BX_NVRIVA_THIS fifo_ramro, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_mode", &BX_NVRIVA_THIS fifo_mode, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_cache1_push0", &BX_NVRIVA_THIS fifo_cache1_push0, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_cache1_push1", &BX_NVRIVA_THIS fifo_cache1_push1, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_cache1_put", &BX_NVRIVA_THIS fifo_cache1_put, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_cache1_dma_push", &BX_NVRIVA_THIS fifo_cache1_dma_push, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_cache1_dma_instance", &BX_NVRIVA_THIS fifo_cache1_dma_instance, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_cache1_dma_put", &BX_NVRIVA_THIS fifo_cache1_dma_put, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_cache1_dma_get", &BX_NVRIVA_THIS fifo_cache1_dma_get, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_cache1_ref_cnt", &BX_NVRIVA_THIS fifo_cache1_ref_cnt, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_cache1_pull0", &BX_NVRIVA_THIS fifo_cache1_pull0, BASE_HEX);
  new bx_shadow_num_c(list, "fifo_cache1_get", &BX_NVRIVA_THIS fifo_cache1_get, BASE_HEX);
  new bx_shadow_data_c(list, "fifo_cache1_method", (Bit8u*)BX_NVRIVA_THIS fifo_cache1_method, RIVA_CACHE1_SIZE << 2, 1);
  new bx_shadow_data_c(list, "fifo_cache1_data", (Bit8u*)BX_NVRIVA_THIS fifo_cache1_data, RIVA_CACHE1_SIZE << 2, 1);
  new bx_shadow_num_c(list, "rma_addr", &BX_NVRIVA_THIS rma_addr, BASE_HEX);
  new bx_shadow_num_c(list, "timer_intr", &BX_NVRIVA_THIS timer_intr, BASE_HEX);
  new bx_shadow_num_c(list, "timer_intr_en", &BX_NVRIVA_THIS timer_intr_en, BASE_HEX);
  new bx_shadow_num_c(list, "timer_num", &BX_NVRIVA_THIS timer_num, BASE_HEX);
  new bx_shadow_num_c(list, "timer_den", &BX_NVRIVA_THIS timer_den, BASE_HEX);
  new bx_shadow_num_c(list, "timer_inittime1", &BX_NVRIVA_THIS timer_inittime1, BASE_HEX);
  new bx_shadow_num_c(list, "timer_inittime2", &BX_NVRIVA_THIS timer_inittime2, BASE_HEX);
  new bx_shadow_num_c(list, "timer_alarm", &BX_NVRIVA_THIS timer_alarm, BASE_HEX);
  new bx_shadow_num_c(list, "straps0_primary", &BX_NVRIVA_THIS straps0_primary, BASE_HEX);
  new bx_shadow_num_c(list, "straps0_primary_original", &BX_NVRIVA_THIS straps0_primary_original, BASE_HEX);
  new bx_shadow_num_c(list, "graph_intr", &BX_NVRIVA_THIS graph_intr, BASE_HEX);
  new bx_shadow_num_c(list, "graph_nsource", &BX_NVRIVA_THIS graph_nsource, BASE_HEX);
  new bx_shadow_num_c(list, "graph_intr_en", &BX_NVRIVA_THIS graph_intr_en, BASE_HEX);
  new bx_shadow_num_c(list, "graph_ctx_switch1", &BX_NVRIVA_THIS graph_ctx_switch1, BASE_HEX);
  new bx_shadow_num_c(list, "graph_ctx_switch2", &BX_NVRIVA_THIS graph_ctx_switch2, BASE_HEX);
  new bx_shadow_num_c(list, "graph_ctx_switch4", &BX_NVRIVA_THIS graph_ctx_switch4, BASE_HEX);
  new bx_shadow_num_c(list, "graph_ctxctl_cur", &BX_NVRIVA_THIS graph_ctxctl_cur, BASE_HEX);
  new bx_shadow_num_c(list, "graph_status", &BX_NVRIVA_THIS graph_status, BASE_HEX);
  new bx_shadow_num_c(list, "graph_trapped_addr", &BX_NVRIVA_THIS graph_trapped_addr, BASE_HEX);
  new bx_shadow_num_c(list, "graph_trapped_data", &BX_NVRIVA_THIS graph_trapped_data, BASE_HEX);
  new bx_shadow_num_c(list, "graph_flip_read", &BX_NVRIVA_THIS graph_flip_read, BASE_HEX);
  new bx_shadow_num_c(list, "graph_flip_write", &BX_NVRIVA_THIS graph_flip_write, BASE_HEX);
  new bx_shadow_num_c(list, "graph_flip_modulo", &BX_NVRIVA_THIS graph_flip_modulo, BASE_HEX);
  new bx_shadow_num_c(list, "graph_notify", &BX_NVRIVA_THIS graph_notify, BASE_HEX);
  new bx_shadow_num_c(list, "graph_fifo", &BX_NVRIVA_THIS graph_fifo, BASE_HEX);
  new bx_shadow_num_c(list, "graph_bpixel", &BX_NVRIVA_THIS graph_bpixel, BASE_HEX);
  new bx_shadow_num_c(list, "graph_channel_ctx_table", &BX_NVRIVA_THIS graph_channel_ctx_table, BASE_HEX);
  new bx_shadow_num_c(list, "graph_offset0", &BX_NVRIVA_THIS graph_offset0, BASE_HEX);
  new bx_shadow_num_c(list, "graph_pitch0", &BX_NVRIVA_THIS graph_pitch0, BASE_HEX);
  new bx_shadow_num_c(list, "crtc_intr", &BX_NVRIVA_THIS crtc_intr, BASE_HEX);
  new bx_shadow_num_c(list, "crtc_intr_en", &BX_NVRIVA_THIS crtc_intr_en, BASE_HEX);
  new bx_shadow_num_c(list, "crtc_start", &BX_NVRIVA_THIS crtc_start, BASE_HEX);
  new bx_shadow_num_c(list, "crtc_config", &BX_NVRIVA_THIS crtc_config, BASE_HEX);
  new bx_shadow_num_c(list, "crtc_raster_pos", &BX_NVRIVA_THIS crtc_raster_pos, BASE_HEX);
  new bx_shadow_num_c(list, "crtc_cursor_offset", &BX_NVRIVA_THIS crtc_cursor_offset, BASE_HEX);
  new bx_shadow_num_c(list, "crtc_cursor_config", &BX_NVRIVA_THIS crtc_cursor_config, BASE_HEX);
  new bx_shadow_num_c(list, "crtc_gpio_ext", &BX_NVRIVA_THIS crtc_gpio_ext, BASE_HEX);
  new bx_shadow_num_c(list, "ramdac_cu_start_pos", &BX_NVRIVA_THIS ramdac_cu_start_pos, BASE_HEX);
  new bx_shadow_num_c(list, "ramdac_nvpll", &BX_NVRIVA_THIS ramdac_nvpll, BASE_HEX);
  new bx_shadow_num_c(list, "ramdac_mpll", &BX_NVRIVA_THIS ramdac_mpll, BASE_HEX);
  new bx_shadow_num_c(list, "ramdac_vpll", &BX_NVRIVA_THIS ramdac_vpll, BASE_HEX);
  new bx_shadow_num_c(list, "ramdac_pll_select", &BX_NVRIVA_THIS ramdac_pll_select, BASE_HEX);
  new bx_shadow_num_c(list, "ramdac_general_control", &BX_NVRIVA_THIS ramdac_general_control, BASE_HEX);
  new bx_shadow_num_c(list, "pfb_boot_0", &BX_NVRIVA_THIS pfb_boot_0, BASE_HEX);
  new bx_shadow_num_c(list, "pfb_config_0", &BX_NVRIVA_THIS pfb_config_0, BASE_HEX);
  new bx_shadow_num_c(list, "pfb_config_1", &BX_NVRIVA_THIS pfb_config_1, BASE_HEX);
  new bx_shadow_data_c(list, "chs", (Bit8u*)BX_NVRIVA_THIS chs, sizeof(chs));
  register_pci_state(list);
}

void bx_nvriva_c::after_restore_state(void)
{
  bx_pci_device_c::after_restore_pci_state();
  if (BX_NVRIVA_THIS crtc.reg[0x28] == 0x00) {
    BX_NVRIVA_THIS bx_vgacore_c::after_restore_state();
  } else {
    BX_NVRIVA_THIS svga_needs_update_mode = 1;
    BX_NVRIVA_THIS update();
  }
}

void bx_nvriva_c::redraw_area(unsigned x0, unsigned y0,
                               unsigned width, unsigned height)
{
  redraw_area_d((Bit32s)x0, (Bit32s)y0, width, height);
}

void bx_nvriva_c::redraw_area_nd(Bit32u offset, Bit32u width, Bit32u height)
{
  if (BX_NVRIVA_THIS svga_pitch != 0) {
    Bit32u redraw_x = offset % BX_NVRIVA_THIS svga_pitch / (BX_NVRIVA_THIS svga_bpp >> 3);
    Bit32u redraw_y = offset / BX_NVRIVA_THIS svga_pitch;
    BX_NVRIVA_THIS redraw_area_nd(redraw_x, redraw_y, width, height);
  }
}

void bx_nvriva_c::redraw_area_nd(Bit32s x0, Bit32s y0, Bit32u width, Bit32u height)
{
  if (s.y_doublescan) {
    y0 <<= 1;
    height <<= 1;
  }
  if (BX_NVRIVA_THIS svga_double_width) {
    x0 <<= 1;
    width <<= 1;
  }
  BX_NVRIVA_THIS redraw_area_d(x0, y0, width, height);
}

void bx_nvriva_c::redraw_area_d(Bit32s x0, Bit32s y0, Bit32u width, Bit32u height)
{
  if (x0 + (Bit32s)width <= 0 || y0 + (Bit32s)height <= 0)
    return;

  if (!BX_NVRIVA_THIS crtc.reg[0x28]) {
    BX_NVRIVA_THIS bx_vgacore_c::redraw_area(x0, y0, width, height);
    return;
  }

  if (BX_NVRIVA_THIS svga_needs_update_mode)
    return;

  BX_NVRIVA_THIS svga_needs_update_tile = 1;

  unsigned xti, yti, xt0, xt1, yt0, yt1;
  xt0 = x0 <= 0 ? 0 : x0 / X_TILESIZE;
  yt0 = y0 <= 0 ? 0 : y0 / Y_TILESIZE;
  if (x0 < (Bit32s)BX_NVRIVA_THIS svga_xres) {
    xt1 = (x0 + width - 1) / X_TILESIZE;
  } else {
    xt1 = (BX_NVRIVA_THIS svga_xres - 1) / X_TILESIZE;
  }
  if (y0 < (Bit32s)BX_NVRIVA_THIS svga_yres) {
    yt1 = (y0 + height - 1) / Y_TILESIZE;
  } else {
    yt1 = (BX_NVRIVA_THIS svga_yres - 1) / Y_TILESIZE;
  }
  if ((x0 + width) > BX_NVRIVA_THIS svga_xres) {
    BX_NVRIVA_THIS redraw_area_d(0, y0 + 1, x0 + width - BX_NVRIVA_THIS svga_xres, height);
  }
  for (yti=yt0; yti<=yt1; yti++) {
    for (xti=xt0; xti<=xt1; xti++) {
      SET_TILE_UPDATED(BX_NVRIVA_THIS, xti, yti, 1);
    }
  }
}

void bx_nvriva_c::svga_init_pcihandlers(void)
{
  BX_NVRIVA_THIS devfunc = 0x00;
  DEV_register_pci_handlers2(BX_NVRIVA_THIS_PTR,
      &BX_NVRIVA_THIS devfunc, BX_PLUGIN_NVRIVA, "RIVA TNT2 M64 AGP", true);
  BX_NVRIVA_THIS init_pci_conf(0x10DE, 0x002D, 0x15, 0x030000, 0x00, BX_PCI_INTA);

  BX_NVRIVA_THIS init_bar_mem(0, RIVA_PNPMMIO_SIZE, nvriva_mem_read_handler, nvriva_mem_write_handler);
  BX_NVRIVA_THIS pci_conf[0x14] = 0x08;
  BX_NVRIVA_THIS init_bar_mem(1, BX_NVRIVA_THIS s.memsize, nvriva_mem_read_handler, nvriva_mem_write_handler);
  BX_NVRIVA_THIS load_pci_rom(SIM->get_param_string(BXPN_VGA_ROM_PATH)->getptr(),
                               nvriva_mem_read_handler);

  BX_NVRIVA_THIS pci_conf[0x2c] = 0x7D;
  BX_NVRIVA_THIS pci_conf[0x2d] = 0x10;
  BX_NVRIVA_THIS pci_conf[0x2e] = 0x37;
  BX_NVRIVA_THIS pci_conf[0x2f] = 0x21;
  BX_NVRIVA_THIS pci_conf[0x40] = BX_NVRIVA_THIS pci_conf[0x2c];
  BX_NVRIVA_THIS pci_conf[0x41] = BX_NVRIVA_THIS pci_conf[0x2d];
  BX_NVRIVA_THIS pci_conf[0x42] = BX_NVRIVA_THIS pci_conf[0x2e];
  BX_NVRIVA_THIS pci_conf[0x43] = BX_NVRIVA_THIS pci_conf[0x2f];

  BX_NVRIVA_THIS pci_conf[0x06] = 0xB0;
  BX_NVRIVA_THIS pci_conf[0x07] = 0x02;
  BX_NVRIVA_THIS pci_conf[0x0d] = 0x40;
  BX_NVRIVA_THIS pci_conf[0x3e] = 0x05;
  BX_NVRIVA_THIS pci_conf[0x3f] = 0x01;
  BX_NVRIVA_THIS pci_conf[0x34] = 0x60;
  BX_NVRIVA_THIS pci_conf[0x44] = 0x02;
  BX_NVRIVA_THIS pci_conf[0x45] = 0x00;
  BX_NVRIVA_THIS pci_conf[0x46] = 0x20;
  BX_NVRIVA_THIS pci_conf[0x47] = 0x00;
  BX_NVRIVA_THIS pci_conf[0x48] = 0x07;
  BX_NVRIVA_THIS pci_conf[0x49] = 0x00;
  BX_NVRIVA_THIS pci_conf[0x4a] = 0x00;
  BX_NVRIVA_THIS pci_conf[0x4b] = 0x1F;
  BX_NVRIVA_THIS pci_conf[0x50] = 0x01;
  BX_NVRIVA_THIS pci_conf[0x54] = 0x01;
  BX_NVRIVA_THIS pci_conf[0x55] = 0x00;
  BX_NVRIVA_THIS pci_conf[0x56] = 0x00;
  BX_NVRIVA_THIS pci_conf[0x57] = 0x00;
  BX_NVRIVA_THIS pci_conf[0x60] = 0x01;
  BX_NVRIVA_THIS pci_conf[0x61] = 0x44;
  BX_NVRIVA_THIS pci_conf[0x62] = 0x01;
  BX_NVRIVA_THIS pci_conf[0x63] = 0x00;
}

// Memory handlers, display update, VGA I/O, CRTC, cursor follow



#undef LOG_THIS
#define LOG_THIS class_ptr->

bool bx_nvriva_c::nvriva_mem_read_handler(bx_phy_address addr, unsigned len,
                                          void *data, void *param)
{
  bx_nvriva_c* class_ptr = (bx_nvriva_c*)param;
  if (addr >= class_ptr->pci_bar[0].addr &&
      addr < (class_ptr->pci_bar[0].addr + RIVA_PNPMMIO_SIZE)) {
    Bit32u offset = addr & (RIVA_PNPMMIO_SIZE - 1);
    if (len == 1) {
      *(Bit8u*)data = class_ptr->register_read8(offset);
    } else if (len == 2) {
      Bit16u value = (Bit16u)class_ptr->register_read32(offset);
      *((Bit16u*)data) = value;
    } else if (len == 4) {
      Bit32u value = class_ptr->register_read32(offset);
      *((Bit32u*)data) = value;
    }
    return true;
  }

  Bit8u *data_ptr;
#ifdef BX_LITTLE_ENDIAN
  data_ptr = (Bit8u *) data;
#else
  data_ptr = (Bit8u *) data + (len - 1);
#endif
  for (unsigned i = 0; i < len; i++) {
    *data_ptr = class_ptr->mem_read(addr);
    addr++;
#ifdef BX_LITTLE_ENDIAN
    data_ptr++;
#else
    data_ptr--;
#endif
  }
  return true;
}

bool bx_nvriva_c::nvriva_mem_write_handler(bx_phy_address addr, unsigned len,
                                           void *data, void *param)
{
  bx_nvriva_c* class_ptr = (bx_nvriva_c*)param;
  if (addr >= class_ptr->pci_bar[0].addr &&
      addr < (class_ptr->pci_bar[0].addr + RIVA_PNPMMIO_SIZE)) {
    Bit32u offset = addr & (RIVA_PNPMMIO_SIZE - 1);
    if (len == 1) {
      class_ptr->register_write8(offset, *(Bit8u*)data);
    } else if (len == 4) {
      Bit32u value = *((Bit32u*)data);
      class_ptr->register_write32(offset, value);
    } else if (len == 8) {
      Bit64u value = *((Bit64u*)data);
      class_ptr->register_write32(offset, (Bit32u)value);
      class_ptr->register_write32(offset + 4, value >> 32);
    }
    return true;
  }

  Bit8u *data_ptr;
#ifdef BX_LITTLE_ENDIAN
  data_ptr = (Bit8u *) data;
#else
  data_ptr = (Bit8u *) data + (len - 1);
#endif
  for (unsigned i = 0; i < len; i++) {
    class_ptr->mem_write(addr, *data_ptr);
    addr++;
#ifdef BX_LITTLE_ENDIAN
    data_ptr++;
#else
    data_ptr--;
#endif
  }
  return true;
}

#undef LOG_THIS
#define LOG_THIS BX_NVRIVA_THIS

Bit8u bx_nvriva_c::mem_read(bx_phy_address addr)
{
  if (BX_NVRIVA_THIS pci_bar[PCI_ROM_BAR].size > 0) {
    Bit32u mask = (BX_NVRIVA_THIS pci_bar[PCI_ROM_BAR].size - 1);
    if (((Bit32u)addr & ~mask) == BX_NVRIVA_THIS pci_bar[PCI_ROM_BAR].addr) {
      if (BX_NVRIVA_THIS pci_conf[0x30] & 0x01) {
        if (BX_NVRIVA_THIS pci_conf[0x50] == 0x00)
          return BX_NVRIVA_THIS pci_rom[addr & mask];
        else
          return BX_NVRIVA_THIS s.memory[(addr & mask) + BX_NVRIVA_THIS ramin_base];
      } else {
        return 0xff;
      }
    }
  }

  if ((addr >= BX_NVRIVA_THIS pci_bar[1].addr) &&
      (addr < (BX_NVRIVA_THIS pci_bar[1].addr + BX_NVRIVA_THIS s.memsize))) {
    Bit32u offset = addr & BX_NVRIVA_THIS memsize_mask;
    return BX_NVRIVA_THIS s.memory[offset];
  }

  if (!BX_NVRIVA_THIS crtc.reg[0x28])
    return BX_NVRIVA_THIS bx_vgacore_c::mem_read(addr);

  if (addr >= 0xA0000 && addr <= 0xAFFFF) {
    Bit32u offset = addr & 0xffff;
    offset += BX_NVRIVA_THIS bank_base[0];
    offset &= BX_NVRIVA_THIS memsize_mask;
    return BX_NVRIVA_THIS s.memory[offset];
  }

  return 0xff;
}

void bx_nvriva_c::mem_write(bx_phy_address addr, Bit8u value)
{
  if ((addr >= BX_NVRIVA_THIS pci_bar[1].addr) &&
      (addr < (BX_NVRIVA_THIS pci_bar[1].addr + BX_NVRIVA_THIS s.memsize))) {
    Bit32u offset = addr & BX_NVRIVA_THIS memsize_mask;
    BX_NVRIVA_THIS s.memory[offset] = value;
    if (offset < BX_NVRIVA_THIS disp_end_offset &&
        offset >= BX_NVRIVA_THIS disp_offset) {
      BX_NVRIVA_THIS svga_needs_update_tile = 1;
      offset -= BX_NVRIVA_THIS disp_offset;
      Bit32u x = (offset % BX_NVRIVA_THIS svga_pitch) / (BX_NVRIVA_THIS svga_bpp / 8);
      Bit32u y = offset / BX_NVRIVA_THIS svga_pitch;
      if (BX_NVRIVA_THIS s.y_doublescan)
        y <<= 1;
      if (BX_NVRIVA_THIS svga_double_width)
        x <<= 1;
      SET_TILE_UPDATED(BX_NVRIVA_THIS, x / X_TILESIZE, y / Y_TILESIZE, 1);
    }
    return;
  }

  if (!BX_NVRIVA_THIS crtc.reg[0x28]) {
    BX_NVRIVA_THIS bx_vgacore_c::mem_write(addr, value);
    return;
  }

  if (addr >= 0xA0000 && addr <= 0xAFFFF) {
    Bit32u offset = addr & 0xffff;
    if (BX_NVRIVA_THIS crtc.reg[0x1c] & 0x80) {
      BX_NVRIVA_THIS s.memory[offset + BX_NVRIVA_THIS ramin_base] = value;
      return;
    }
    offset += BX_NVRIVA_THIS bank_base[0];
    offset &= BX_NVRIVA_THIS memsize_mask;
    BX_NVRIVA_THIS s.memory[offset] = value;
    if (BX_NVRIVA_THIS svga_pitch != 0) {
      BX_NVRIVA_THIS svga_needs_update_tile = 1;
      Bit32u x = (offset % BX_NVRIVA_THIS svga_pitch) / (BX_NVRIVA_THIS svga_bpp / 8);
      Bit32u y = offset / BX_NVRIVA_THIS svga_pitch;
      if (BX_NVRIVA_THIS s.y_doublescan)
        y <<= 1;
      if (BX_NVRIVA_THIS svga_double_width)
        x <<= 1;
      SET_TILE_UPDATED(BX_NVRIVA_THIS, x / X_TILESIZE, y / Y_TILESIZE, 1);
    }
  }
}

void bx_nvriva_c::get_text_snapshot(Bit8u **text_snapshot,
                                    unsigned *txHeight, unsigned *txWidth)
{
  BX_NVRIVA_THIS bx_vgacore_c::get_text_snapshot(text_snapshot, txHeight, txWidth);
}

Bit32u bx_nvriva_c::svga_read_handler(void *this_ptr, Bit32u address, unsigned io_len)
{
#if !BX_USE_NVRIVA_SMF
  bx_nvriva_c *class_ptr = (bx_nvriva_c *) this_ptr;
  return class_ptr->svga_read(address, io_len);
}
Bit32u bx_nvriva_c::svga_read(Bit32u address, unsigned io_len)
{
#else
  UNUSED(this_ptr);
#endif

  if (address == 0x03C3 && io_len == 2)
    return VGA_READ(address, 1) | VGA_READ(address + 1, 1) << 8;

  if (address == 0x03d0 || address == 0x03d2) {
    if (io_len == 1)
      return 0;
    Bit8u crtc38 = BX_NVRIVA_THIS crtc.reg[0x38];
    bool rma_enable = crtc38 & 1;
    if (!rma_enable)
      return 0;
    int rma_index = crtc38 >> 1;
    if (rma_index == 1) {
      if (address == 0x03d0)
        return BX_NVRIVA_THIS rma_addr;
      else
        return BX_NVRIVA_THIS rma_addr >> 16;
    } else if (rma_index == 2) {
      Bit32u offset = BX_NVRIVA_THIS rma_addr;
      bool vram = false;
      if (offset & 0x80000000) {
        vram = true;
        offset &= ~0x80000000;
      }
      if ((!vram && offset < RIVA_PNPMMIO_SIZE) ||
           (vram && offset < BX_NVRIVA_THIS s.memsize)) {
        Bit32u value = vram ? vram_read32(offset) : register_read32(offset);
        if (address == 0x03d0)
          return value;
        else
          return value >> 16;
      }
      return 0xFFFFFFFF;
    }
    return 0;
  }

  if ((io_len == 2) && ((address & 1) == 0)) {
    Bit32u value  = (Bit32u)SVGA_READ(address, 1);
           value |= (Bit32u)SVGA_READ(address + 1, 1) << 8;
    return value;
  }

  if (io_len != 1)
    return 0;

  switch (address) {
    case 0x03b4:
    case 0x03d4:
      return BX_NVRIVA_THIS crtc.index;
    case 0x03b5:
    case 0x03d5:
      if (BX_NVRIVA_THIS crtc.index > VGA_CRTC_MAX)
        return BX_NVRIVA_THIS svga_read_crtc(address, BX_NVRIVA_THIS crtc.index);
      else
        break;
    case 0x03c2:
      return 0x10;
    default:
      break;
  }

  return VGA_READ(address, io_len);
}

void bx_nvriva_c::svga_write_handler(void *this_ptr, Bit32u address, Bit32u value, unsigned io_len)
{
#if !BX_USE_NVRIVA_SMF
  bx_nvriva_c *class_ptr = (bx_nvriva_c *) this_ptr;
  class_ptr->svga_write(address, value, io_len);
}
void bx_nvriva_c::svga_write(Bit32u address, Bit32u value, unsigned io_len)
{
#else
  UNUSED(this_ptr);
#endif

  if (address == 0x03d0 || address == 0x03d2) {
    if (io_len == 1)
      return;
    Bit8u crtc38 = BX_NVRIVA_THIS crtc.reg[0x38];
    bool rma_enable = crtc38 & 1;
    if (!rma_enable)
      return;
    int rma_index = crtc38 >> 1;
    if (rma_index == 1) {
      if (address == 0x03d0) {
        if (io_len == 2) {
          BX_NVRIVA_THIS rma_addr &= 0xFFFF0000;
          BX_NVRIVA_THIS rma_addr |= value;
        } else {
          BX_NVRIVA_THIS rma_addr = value;
        }
      } else {
        BX_NVRIVA_THIS rma_addr &= 0x0000FFFF;
        BX_NVRIVA_THIS rma_addr |= value << 16;
      }
    } else if (rma_index == 3) {
      Bit32u offset = BX_NVRIVA_THIS rma_addr & ~3;
      bool vram = false;
      if (BX_NVRIVA_THIS rma_addr & 0x80000000) {
        vram = true;
        offset &= ~0x80000000;
      }
      if ((!vram && offset < RIVA_PNPMMIO_SIZE) ||
           (vram && offset < BX_NVRIVA_THIS s.memsize)) {
        if (address == 0x03d0) {
          if (io_len == 2) {
            Bit32u value32 = vram ? vram_read32(offset) : register_read32(offset);
            value32 &= 0xFFFF0000;
            value32 |= value;
            if (vram) vram_write32(offset, value32);
            else register_write32(offset, value32);
          } else {
            if (vram) vram_write32(offset, value);
            else register_write32(offset, value);
          }
        } else {
          Bit32u value32 = vram ? vram_read32(offset) : register_read32(offset);
          value32 &= 0x0000FFFF;
          value32 |= value << 16;
          if (vram) vram_write32(offset, value32);
          else register_write32(offset, value32);
        }
      }
    }
    return;
  }

  if ((io_len == 2) && ((address & 1) == 0)) {
    SVGA_WRITE(address, value & 0xff, 1);
    SVGA_WRITE(address + 1, value >> 8, 1);
    return;
  }

  if (io_len != 1)
    return;

  switch (address) {
    case 0x03b4:
    case 0x03d4:
      BX_NVRIVA_THIS crtc.index = value;
      break;
    case 0x03b5:
    case 0x03d5:
      if (BX_NVRIVA_THIS crtc.index == 0x01 ||
          BX_NVRIVA_THIS crtc.index == 0x07 ||
          BX_NVRIVA_THIS crtc.index == 0x09 ||
          BX_NVRIVA_THIS crtc.index == 0x0c ||
          BX_NVRIVA_THIS crtc.index == 0x0d ||
          BX_NVRIVA_THIS crtc.index == 0x12 ||
          BX_NVRIVA_THIS crtc.index == 0x13 ||
          BX_NVRIVA_THIS crtc.index == 0x15 ||
          BX_NVRIVA_THIS crtc.index == 0x19 ||
          BX_NVRIVA_THIS crtc.index == 0x25 ||
          BX_NVRIVA_THIS crtc.index == 0x28 ||
          BX_NVRIVA_THIS crtc.index == 0x2D ||
          BX_NVRIVA_THIS crtc.index == 0x41 ||
          BX_NVRIVA_THIS crtc.index == 0x42) {
        BX_NVRIVA_THIS svga_needs_update_mode = 1;
      }
      if (BX_NVRIVA_THIS crtc.index <= VGA_CRTC_MAX) {
        BX_NVRIVA_THIS crtc.reg[BX_NVRIVA_THIS crtc.index] = value;
      } else {
        BX_NVRIVA_THIS svga_write_crtc(address, BX_NVRIVA_THIS crtc.index, value);
        if (BX_NVRIVA_THIS crtc.index == 0x25 ||
            BX_NVRIVA_THIS crtc.index == 0x2D ||
            BX_NVRIVA_THIS crtc.index == 0x41) {
          BX_NVRIVA_THIS calculate_retrace_timing();
        }
        return;
      }
      break;
    default:
      break;
  }

  VGA_WRITE(address, value, io_len);
}

Bit8u bx_nvriva_c::svga_read_crtc(Bit32u address, unsigned index)
{
  if (index <= RIVA_CRTC_MAX) {
    Bit8u value = BX_NVRIVA_THIS crtc.reg[index];
    BX_DEBUG(("crtc: index 0x%02x read 0x%02x", index, value));
    return value;
  }
  return 0xff;
}

void bx_nvriva_c::svga_write_crtc(Bit32u address, unsigned index, Bit8u value)
{
  BX_DEBUG(("crtc: index 0x%02x write 0x%02x", index, (unsigned)value));

  bool update_cursor_addr = false;

  if (index == 0x1c) {
    if (!(BX_NVRIVA_THIS crtc.reg[index] & 0x80) && (value & 0x80) != 0) {
      BX_NVRIVA_THIS crtc_intr_en = 0x00000000;
      update_irq_level();
    }
  } else if (index == 0x1d || index == 0x1e)
    BX_NVRIVA_THIS bank_base[index - 0x1d] = value * 0x8000;
  else if (index == 0x2f || index == 0x30 || index == 0x31)
    update_cursor_addr = true;
  else if (index == 0x37 || index == 0x3f || index == 0x51) {
    bool scl = value & 0x20;
    bool sda = value & 0x10;
    if (index == 0x3f) {
      BX_NVRIVA_THIS ddc.write(scl, sda);
      BX_NVRIVA_THIS crtc.reg[0x3e] = BX_NVRIVA_THIS ddc.read() & 0x0c;
    } else {
      BX_NVRIVA_THIS crtc.reg[index - 1] = sda << 3 | scl << 2;
    }
  }

  if (index <= RIVA_CRTC_MAX)
    BX_NVRIVA_THIS crtc.reg[index] = value;

  if (update_cursor_addr) {
    bool prev_enabled = BX_NVRIVA_THIS hw_cursor.enabled;
    BX_NVRIVA_THIS hw_cursor.enabled =
      (BX_NVRIVA_THIS crtc.reg[0x31] & 0x01) ||
      (BX_NVRIVA_THIS crtc_cursor_config & 0x00000001);
    BX_NVRIVA_THIS hw_cursor.vram = false;
    BX_NVRIVA_THIS hw_cursor.offset =
      (BX_NVRIVA_THIS crtc.reg[0x31] >> 2 << 11) |
      (BX_NVRIVA_THIS crtc.reg[0x30] & 0x7F) << 17 |
       BX_NVRIVA_THIS crtc.reg[0x2f] << 24;
    BX_NVRIVA_THIS hw_cursor.offset += BX_NVRIVA_THIS crtc_cursor_offset;
    if (prev_enabled != BX_NVRIVA_THIS hw_cursor.enabled) {
      BX_NVRIVA_THIS redraw_area_nd(BX_NVRIVA_THIS hw_cursor.x, BX_NVRIVA_THIS hw_cursor.y,
        BX_NVRIVA_THIS hw_cursor.size, BX_NVRIVA_THIS hw_cursor.size);
    }
  }
}

Bit8u riva_alpha_wrap(int value)
{
  return -(value >> 8) ^ value;
}

Bit16u bx_nvriva_c::cursor_read16(Bit32u address)
{
  return ramin_read16(address);
}

Bit32u bx_nvriva_c::cursor_read32(Bit32u address)
{
  return ramin_read32(address);
}

void bx_nvriva_c::draw_hardware_cursor(unsigned xc, unsigned yc, bx_svga_tileinfo_t *info)
{
  Bit16s hwcx = BX_NVRIVA_THIS hw_cursor.x;
  Bit16s hwcy = BX_NVRIVA_THIS hw_cursor.y;
  Bit8u size = BX_NVRIVA_THIS hw_cursor.size;

  if (BX_NVRIVA_THIS svga_double_width) {
    hwcx <<= 1;
    hwcy <<= 1;
    size <<= 1;
  }

  unsigned w, h;
  Bit8u* tile_ptr;
  if (info->snapshot_mode) {
    tile_ptr = bx_gui->get_snapshot_buffer();
    w = BX_NVRIVA_THIS svga_xres;
    h = BX_NVRIVA_THIS svga_yres;
  } else {
    tile_ptr = bx_gui->graphics_tile_get(xc, yc, &w, &h);
  }
  if (BX_NVRIVA_THIS hw_cursor.enabled &&
      (int)xc < hwcx + size &&
      (int)(xc + w) > hwcx &&
      (int)yc < hwcy + size &&
      (int)(yc + h) > hwcy) {
    unsigned cx0 = hwcx > (int)xc ? hwcx : xc;
    unsigned cy0 = hwcy > (int)yc ? hwcy : yc;
    unsigned cx1 = hwcx + size < (int)(xc + w) ? hwcx + size : xc + w;
    unsigned cy1 = hwcy + size < (int)(yc + h) ? hwcy + size : yc + h;

    Bit8u display_color_bytes = BX_NVRIVA_THIS svga_bpp >> 3;
    Bit8u cursor_color_bytes = BX_NVRIVA_THIS hw_cursor.bpp32 ? 4 : 2;
    if (info->bpp == 15) info->bpp = 16;
    tile_ptr += info->pitch * (cy0 - yc) + info->bpp / 8 * (cx0 - xc);
    unsigned pitch = BX_NVRIVA_THIS hw_cursor.size * cursor_color_bytes;
    Bit32u cursor_ofs = BX_NVRIVA_THIS hw_cursor.offset +
      (pitch * (cy0 - hwcy) >> (int)BX_NVRIVA_THIS s.y_doublescan);
    Bit8u* vid_ptr = BX_NVRIVA_THIS disp_ptr + (BX_NVRIVA_THIS svga_pitch * cy0 >>
      (int)BX_NVRIVA_THIS s.y_doublescan);
    for (unsigned cy = cy0; cy < cy1; cy++) {
      Bit8u* tile_ptr2 = tile_ptr;
      Bit32u cursor_ofs2 = cursor_ofs + (cursor_color_bytes * (cx0 - hwcx) >>
        (int)BX_NVRIVA_THIS svga_double_width);
      Bit8u* vid_ptr2 = vid_ptr + (display_color_bytes * cx0 >>
        (int)BX_NVRIVA_THIS svga_double_width);
      for (unsigned cx = cx0; cx < cx1; cx++) {
        Bit8u dr, dg, db;
        if (display_color_bytes == 1) {
          if (info->is_indexed) {
            dr = dg = db = vid_ptr2[0];
          } else {
            Bit8u index = vid_ptr2[0];
            dr = (BX_NVRIVA_THIS s.pel.data[index].red << BX_NVRIVA_THIS s.dac_shift);
            dg = (BX_NVRIVA_THIS s.pel.data[index].green << BX_NVRIVA_THIS s.dac_shift);
            db = (BX_NVRIVA_THIS s.pel.data[index].blue << BX_NVRIVA_THIS s.dac_shift);
          }
        } else if (display_color_bytes == 2) {
          EXTRACT_565_TO_888(vid_ptr2[0] << 0 | vid_ptr2[1] << 8, dr, dg, db);
        } else {
          db = vid_ptr2[0];
          dg = vid_ptr2[1];
          dr = vid_ptr2[2];
        }
        Bit8u b, g, r;
        if (BX_NVRIVA_THIS hw_cursor.bpp32) {
          Bit32u cursor_color = cursor_read32(cursor_ofs2);
          if (cursor_color != 0) {
            Bit8u alpha, cr, cg, cb;
            EXTRACT_8888_TO_8888(cursor_color, alpha, cr, cg, cb);
            Bit8u ica = 0xFF - alpha;
            b = riva_alpha_wrap(db * ica / 0xFF + cb);
            g = riva_alpha_wrap(dg * ica / 0xFF + cg);
            r = riva_alpha_wrap(dr * ica / 0xFF + cr);
          } else {
            b = db; g = dg; r = dr;
          }
        } else {
          Bit8u alpha, cr, cg, cb;
          EXTRACT_1555_TO_8888(cursor_read16(cursor_ofs2), alpha, cr, cg, cb);
          if (alpha) {
            b = cb; g = cg; r = cr;
          } else {
            b = db ^ cb; g = dg ^ cg; r = dr ^ cr;
          }
        }
        Bit32u color;
        if (display_color_bytes == 1) {
          color = b << 0 | g << 8 | r << 16;
        } else {
          color =
            BX_NVRIVA_THIS s.pel.data[b].blue << 0 |
            BX_NVRIVA_THIS s.pel.data[g].green << 8 |
            BX_NVRIVA_THIS s.pel.data[r].red << 16;
        }
        if (!info->is_indexed) {
          color = MAKE_COLOUR(
            color, 24, info->red_shift, info->red_mask,
            color, 16, info->green_shift, info->green_mask,
            color, 8, info->blue_shift, info->blue_mask);
          if (info->is_little_endian) {
            for (int i = 0; i < info->bpp; i += 8)
              *(tile_ptr2++) = (Bit8u)(color >> i);
          } else {
            for (int i = info->bpp - 8; i > -8; i -= 8)
              *(tile_ptr2++) = (Bit8u)(color >> i);
          }
        } else {
          *(tile_ptr2++) = (Bit8u)color;
        }
        if (!BX_NVRIVA_THIS svga_double_width || (cx & 1)) {
          cursor_ofs2 += cursor_color_bytes;
          vid_ptr2 += display_color_bytes;
        }
      }
      tile_ptr += info->pitch;
      if (!BX_NVRIVA_THIS s.y_doublescan || (cy & 1)) {
        cursor_ofs += pitch;
        vid_ptr += BX_NVRIVA_THIS svga_pitch;
      }
    }
  }
}

Bit16u bx_nvriva_c::get_crtc_vtotal()
{
  return BX_NVRIVA_THIS crtc.reg[6] +
    ((BX_NVRIVA_THIS crtc.reg[7] & 0x01) << 8) +
    ((BX_NVRIVA_THIS crtc.reg[7] & 0x20) << 4) +
    ((BX_NVRIVA_THIS crtc.reg[0x25] & 1) << 10) +
    ((BX_NVRIVA_THIS crtc.reg[0x41] & 1) << 11) + 2;
}

void bx_nvriva_c::get_crtc_params(bx_crtc_params_t* crtcp, Bit32u* vclock)
{
  Bit32u m = BX_NVRIVA_THIS ramdac_vpll & 0xFF;
  Bit32u n = (BX_NVRIVA_THIS ramdac_vpll >> 8) & 0xFF;
  Bit32u p = (BX_NVRIVA_THIS ramdac_vpll >> 16) & 0x07;
  if ((BX_NVRIVA_THIS ramdac_pll_select & 0x200) != 0 && m) {
    Bit32u crystalFreq = 13500000;
    if (BX_NVRIVA_THIS straps0_primary & 0x00000040)
      crystalFreq = 14318180;
    *vclock = (Bit32u)((Bit64u)crystalFreq * n / m >> p);
    crtcp->htotal = BX_NVRIVA_THIS crtc.reg[0] +
                    ((BX_NVRIVA_THIS crtc.reg[0x2D] & 1) << 8) + 5;
    crtcp->vtotal = get_crtc_vtotal();
    crtcp->vbstart = BX_NVRIVA_THIS crtc.reg[21] +
                     ((BX_NVRIVA_THIS crtc.reg[7] & 0x08) << 5) +
                     ((BX_NVRIVA_THIS crtc.reg[9] & 0x20) << 4) +
                     ((BX_NVRIVA_THIS crtc.reg[0x25] & 0x08) << 7) +
                     ((BX_NVRIVA_THIS crtc.reg[0x41] & 0x40) << 5);
    crtcp->vrstart = BX_NVRIVA_THIS crtc.reg[16] +
                     ((BX_NVRIVA_THIS crtc.reg[7] & 0x04) << 6) +
                     ((BX_NVRIVA_THIS crtc.reg[7] & 0x80) << 2) +
                     ((BX_NVRIVA_THIS crtc.reg[0x25] & 0x04) << 8) +
                     ((BX_NVRIVA_THIS crtc.reg[0x41] & 0x10) << 7);
  } else {
    bx_vgacore_c::get_crtc_params(crtcp, vclock);
  }
}

void bx_nvriva_c::update(void)
{
  Bit8u crtc28 = BX_NVRIVA_THIS crtc.reg[0x28];

  if (crtc28 & 0x80)
    crtc28 &= 0x7F;

  if (crtc28 == 0x00) {
    BX_NVRIVA_THIS bx_vgacore_c::update();
    return;
  }

  if (BX_NVRIVA_THIS svga_needs_update_mode) {
    Bit32u iTopOffset =
       BX_NVRIVA_THIS crtc.reg[0x0d] |
      (BX_NVRIVA_THIS crtc.reg[0x0c] << 8) |
      (BX_NVRIVA_THIS crtc.reg[0x19] & 0x1F) << 16;
    iTopOffset <<= 2;
    iTopOffset += BX_NVRIVA_THIS crtc_start;

    Bit32u iPitch =
       BX_NVRIVA_THIS crtc.reg[0x13] |
      (BX_NVRIVA_THIS crtc.reg[0x19] >> 5 << 8) |
      (BX_NVRIVA_THIS crtc.reg[0x42] >> 6 & 1) << 11;
    iPitch <<= 3;

    Bit8u iBpp = 0;
    if (crtc28 == 0x01) iBpp = 8;
    else if (crtc28 == 0x02) iBpp = 16;
    else if (crtc28 == 0x03) iBpp = 32;

    Bit32u iWidth =
      (BX_NVRIVA_THIS crtc.reg[1] +
      ((BX_NVRIVA_THIS crtc.reg[0x2D] & 0x02) << 7) + 1) * 8;
    Bit32u iHeight =
      (BX_NVRIVA_THIS crtc.reg[18] |
      ((BX_NVRIVA_THIS crtc.reg[7] & 0x02) << 7) |
      ((BX_NVRIVA_THIS crtc.reg[7] & 0x40) << 3) |
      ((BX_NVRIVA_THIS crtc.reg[0x25] & 0x02) << 9) |
      ((BX_NVRIVA_THIS crtc.reg[0x41] & 0x04) << 9)) + 1;

    if (BX_NVRIVA_THIS s.y_doublescan && iHeight > iWidth) {
      iWidth <<= 1;
      BX_NVRIVA_THIS svga_double_width = true;
    } else {
      BX_NVRIVA_THIS svga_double_width = false;
    }

    if (iWidth != BX_NVRIVA_THIS svga_xres ||
        iHeight != BX_NVRIVA_THIS svga_yres ||
        iBpp != BX_NVRIVA_THIS svga_bpp) {
      BX_INFO(("switched to %u x %u x %u", iWidth, iHeight, iBpp));
    }

    BX_NVRIVA_THIS svga_xres = iWidth;
    BX_NVRIVA_THIS svga_yres = iHeight;
    BX_NVRIVA_THIS svga_bpp = iBpp;
    BX_NVRIVA_THIS svga_dispbpp = iBpp;
    BX_NVRIVA_THIS disp_ptr = BX_NVRIVA_THIS s.memory + iTopOffset;
    BX_NVRIVA_THIS disp_offset = iTopOffset;
    BX_NVRIVA_THIS disp_end_offset = iTopOffset + iPitch * iHeight;
    BX_NVRIVA_THIS svga_pitch = iPitch;
    BX_NVRIVA_THIS s.last_xres = iWidth;
    BX_NVRIVA_THIS s.last_yres = iHeight;
    BX_NVRIVA_THIS s.last_bpp = iBpp;
    BX_NVRIVA_THIS s.last_fh = 0;
  }

  unsigned width, height, pitch;

  if (BX_NVRIVA_THIS svga_dispbpp != 4) {
    width  = BX_NVRIVA_THIS svga_xres;
    height = BX_NVRIVA_THIS svga_yres;
    pitch = BX_NVRIVA_THIS svga_pitch;
    if (BX_NVRIVA_THIS svga_needs_update_mode) {
      bx_gui->dimension_update(width, height, 0, 0, BX_NVRIVA_THIS svga_dispbpp);
      BX_NVRIVA_THIS s.last_bpp = BX_NVRIVA_THIS svga_dispbpp;
      BX_NVRIVA_THIS svga_needs_update_mode = 0;
      BX_NVRIVA_THIS svga_needs_update_dispentire = 1;
    }
  } else {
    BX_NVRIVA_THIS determine_screen_dimensions(&height, &width);
    pitch = BX_NVRIVA_THIS s.line_offset;
    if ((width != BX_NVRIVA_THIS s.last_xres) || (height != BX_NVRIVA_THIS s.last_yres) ||
        (BX_NVRIVA_THIS s.last_bpp > 8)) {
      bx_gui->dimension_update(width, height);
      BX_NVRIVA_THIS s.last_xres = width;
      BX_NVRIVA_THIS s.last_yres = height;
      BX_NVRIVA_THIS s.last_bpp = 8;
    }
  }

  if (BX_NVRIVA_THIS svga_needs_update_dispentire) {
    BX_NVRIVA_THIS redraw_area_d(0, 0, width, height);
    BX_NVRIVA_THIS svga_needs_update_dispentire = 0;
  }

  if (!BX_NVRIVA_THIS svga_needs_update_tile)
    return;

  BX_NVRIVA_THIS svga_needs_update_tile = 0;

  unsigned xc, yc, xti, yti, hp;
  unsigned r, c, w, h, x, y;
  int i;
  Bit8u red, green, blue;
  Bit32u colour, row_addr;
  Bit8u * vid_ptr, * vid_ptr2;
  Bit8u * tile_ptr, * tile_ptr2;
  bx_svga_tileinfo_t info;
  Bit8u dac_size = (BX_NVRIVA_THIS s.dac_shift == 0) ? 8 : 6;

  if (bx_gui->graphics_tile_info_common(&info)) {
    if (info.snapshot_mode) {
      vid_ptr = BX_NVRIVA_THIS disp_ptr;
      tile_ptr = bx_gui->get_snapshot_buffer();
      if (tile_ptr != NULL) {
        for (yc = 0; yc < height; yc++) {
          vid_ptr2  = vid_ptr;
          tile_ptr2 = tile_ptr;
          if (BX_NVRIVA_THIS svga_dispbpp != 4) {
            for (xc = 0; xc < width; xc++) {
              memcpy(tile_ptr2, vid_ptr2, (BX_NVRIVA_THIS svga_bpp >> 3));
              if (!BX_NVRIVA_THIS svga_double_width || (xc & 1))
                vid_ptr2 += (BX_NVRIVA_THIS svga_bpp >> 3);
              tile_ptr2 += ((info.bpp + 1) >> 3);
            }
            if (!BX_NVRIVA_THIS s.y_doublescan || (yc & 1))
              vid_ptr += pitch;
          } else {
            row_addr = BX_NVRIVA_THIS s.CRTC.start_addr + (yc * pitch);
            for (xc = 0; xc < width; xc++)
              *(tile_ptr2++) = BX_NVRIVA_THIS get_vga_pixel(xc, yc, row_addr, 0xffff, 0, BX_NVRIVA_THIS s.memory);
          }
          tile_ptr += info.pitch;
        }
        draw_hardware_cursor(0, 0, &info);
      }
    } else if (info.is_indexed) {
      switch (BX_NVRIVA_THIS svga_dispbpp) {
        case 4:
          for (yc=0, yti = 0; yc<height; yc+=Y_TILESIZE, yti++) {
            for (xc=0, xti = 0; xc<width; xc+=X_TILESIZE, xti++) {
              if (GET_TILE_UPDATED (xti, yti)) {
                tile_ptr = bx_gui->graphics_tile_get(xc, yc, &w, &h);
                for (r=0; r<h; r++) {
                  y = yc + r;
                  if (BX_NVRIVA_THIS s.y_doublescan) y >>= 1;
                  row_addr = BX_NVRIVA_THIS s.CRTC.start_addr + (y * pitch);
                  tile_ptr2 = tile_ptr;
                  for (c=0; c<w; c++) {
                    x = xc + c;
                    *(tile_ptr2++) = BX_NVRIVA_THIS get_vga_pixel(x, y, row_addr, 0xffff, 0, BX_NVRIVA_THIS s.memory);
                  }
                  tile_ptr += info.pitch;
                }
                draw_hardware_cursor(xc, yc, &info);
                bx_gui->graphics_tile_update_in_place(xc, yc, w, h);
                SET_TILE_UPDATED(BX_NVRIVA_THIS, xti, yti, 0);
              }
            }
          }
          break;
        case 8:
          hp = BX_NVRIVA_THIS s.attribute_ctrl.horiz_pel_panning & 0x07;
          for (yc=0, yti = 0; yc<height; yc+=Y_TILESIZE, yti++) {
            for (xc=0, xti = 0; xc<width; xc+=X_TILESIZE, xti++) {
              if (GET_TILE_UPDATED (xti, yti)) {
                if (!BX_NVRIVA_THIS s.y_doublescan)
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + (yc * pitch + xc + hp);
                else if (!BX_NVRIVA_THIS svga_double_width)
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + ((yc >> 1) * pitch + xc + hp);
                else
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + ((yc >> 1) * pitch + ((xc + hp) >> 1));
                tile_ptr = bx_gui->graphics_tile_get(xc, yc, &w, &h);
                for (r=0; r<h; r++) {
                  vid_ptr2  = vid_ptr;
                  tile_ptr2 = tile_ptr;
                  for (c=0; c<w; c++) {
                    colour = *(vid_ptr2);
                    if (!BX_NVRIVA_THIS svga_double_width || (c & 1))
                      vid_ptr2++;
                    *(tile_ptr2++) = colour;
                  }
                  if (!BX_NVRIVA_THIS s.y_doublescan || (r & 1))
                    vid_ptr += pitch;
                  tile_ptr += info.pitch;
                }
                draw_hardware_cursor(xc, yc, &info);
                bx_gui->graphics_tile_update_in_place(xc, yc, w, h);
                SET_TILE_UPDATED(BX_NVRIVA_THIS, xti, yti, 0);
              }
            }
          }
          break;
        default:
          break;
      }
    } else {
      switch (BX_NVRIVA_THIS svga_dispbpp) {
        case 4:
          for (yc=0, yti=0; yc<height; yc+=Y_TILESIZE, yti++) {
            for (xc=0, xti=0; xc<width; xc+=X_TILESIZE, xti++) {
              if (GET_TILE_UPDATED (xti, yti)) {
                tile_ptr = bx_gui->graphics_tile_get(xc, yc, &w, &h);
                for (r=0; r<Y_TILESIZE; r++) {
                  tile_ptr2 = tile_ptr;
                  y = yc + r;
                  if (BX_NVRIVA_THIS s.y_doublescan) y >>= 1;
                  row_addr = BX_NVRIVA_THIS s.CRTC.start_addr + (y * pitch);
                  for (c=0; c<X_TILESIZE; c++) {
                    x = xc + c;
                    colour = BX_NVRIVA_THIS get_vga_pixel(x, y, row_addr, 0xffff, 0, BX_NVRIVA_THIS s.memory);
                    colour = MAKE_COLOUR(
                      BX_NVRIVA_THIS s.pel.data[colour].red, 6, info.red_shift, info.red_mask,
                      BX_NVRIVA_THIS s.pel.data[colour].green, 6, info.green_shift, info.green_mask,
                      BX_NVRIVA_THIS s.pel.data[colour].blue, 6, info.blue_shift, info.blue_mask);
                    if (info.is_little_endian) {
                      for (i=0; i<info.bpp; i+=8)
                        *(tile_ptr2++) = colour >> i;
                    } else {
                      for (i=info.bpp-8; i>-8; i-=8)
                        *(tile_ptr2++) = colour >> i;
                    }
                  }
                  tile_ptr += info.pitch;
                }
                draw_hardware_cursor(xc, yc, &info);
                bx_gui->graphics_tile_update_in_place(xc, yc, w, h);
                SET_TILE_UPDATED(BX_NVRIVA_THIS, xti, yti, 0);
              }
            }
          }
          break;
        case 8:
          hp = BX_NVRIVA_THIS s.attribute_ctrl.horiz_pel_panning & 0x07;
          for (yc=0, yti = 0; yc<height; yc+=Y_TILESIZE, yti++) {
            for (xc=0, xti = 0; xc<width; xc+=X_TILESIZE, xti++) {
              if (GET_TILE_UPDATED (xti, yti)) {
                if (!BX_NVRIVA_THIS s.y_doublescan)
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + (yc * pitch + xc + hp);
                else if (!BX_NVRIVA_THIS svga_double_width)
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + ((yc >> 1) * pitch + xc + hp);
                else
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + ((yc >> 1) * pitch + ((xc + hp) >> 1));
                tile_ptr = bx_gui->graphics_tile_get(xc, yc, &w, &h);
                for (r=0; r<h; r++) {
                  vid_ptr2  = vid_ptr;
                  tile_ptr2 = tile_ptr;
                  for (c=0; c<w; c++) {
                    colour = *(vid_ptr2);
                    if (!BX_NVRIVA_THIS svga_double_width || (c & 1))
                      vid_ptr2++;
                    colour = MAKE_COLOUR(
                      BX_NVRIVA_THIS s.pel.data[colour].red, dac_size, info.red_shift, info.red_mask,
                      BX_NVRIVA_THIS s.pel.data[colour].green, dac_size, info.green_shift, info.green_mask,
                      BX_NVRIVA_THIS s.pel.data[colour].blue, dac_size, info.blue_shift, info.blue_mask);
                    if (info.is_little_endian) {
                      for (i=0; i<info.bpp; i+=8)
                        *(tile_ptr2++) = colour >> i;
                    } else {
                      for (i=info.bpp-8; i>-8; i-=8)
                        *(tile_ptr2++) = colour >> i;
                    }
                  }
                  if (!BX_NVRIVA_THIS s.y_doublescan || (r & 1))
                    vid_ptr += pitch;
                  tile_ptr += info.pitch;
                }
                draw_hardware_cursor(xc, yc, &info);
                bx_gui->graphics_tile_update_in_place(xc, yc, w, h);
                SET_TILE_UPDATED(BX_NVRIVA_THIS, xti, yti, 0);
              }
            }
          }
          break;
        case 15:
          hp = BX_NVRIVA_THIS s.attribute_ctrl.horiz_pel_panning & 0x01;
          for (yc=0, yti = 0; yc<height; yc+=Y_TILESIZE, yti++) {
            for (xc=0, xti = 0; xc<width; xc+=X_TILESIZE, xti++) {
              if (GET_TILE_UPDATED (xti, yti)) {
                if (!BX_NVRIVA_THIS s.y_doublescan)
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + (yc * pitch + ((xc + hp) << 1));
                else if (!BX_NVRIVA_THIS svga_double_width)
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + ((yc >> 1) * pitch + ((xc + hp) << 1));
                else
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + ((yc >> 1) * pitch + xc + (hp << 1));
                tile_ptr = bx_gui->graphics_tile_get(xc, yc, &w, &h);
                for (r=0; r<h; r++) {
                  vid_ptr2 = vid_ptr;
                  tile_ptr2 = tile_ptr;
                  for (c=0; c<w; c++) {
                    colour = *(vid_ptr2) | *(vid_ptr2+1) << 8;
                    if (!BX_NVRIVA_THIS svga_double_width || (c & 1))
                      vid_ptr2 += 2;
                    EXTRACT_x555_TO_888(colour, red, green, blue);
                    if (info.bpp >= 24) {
                      colour =
                        (BX_NVRIVA_THIS s.pel.data[red].red << 16) |
                        (BX_NVRIVA_THIS s.pel.data[green].green << 8) |
                        BX_NVRIVA_THIS s.pel.data[blue].blue;
                    } else {
                      colour = MAKE_COLOUR(
                        BX_NVRIVA_THIS s.pel.data[blue].blue, 8, info.blue_shift, info.blue_mask,
                        BX_NVRIVA_THIS s.pel.data[green].green, 8, info.green_shift, info.green_mask,
                        BX_NVRIVA_THIS s.pel.data[red].red, 8, info.red_shift, info.red_mask);
                    }
                    if (info.is_little_endian) {
                      for (i=0; i<info.bpp; i+=8)
                        *(tile_ptr2++) = colour >> i;
                    } else {
                      for (i=info.bpp-8; i>-8; i-=8)
                        *(tile_ptr2++) = colour >> i;
                    }
                  }
                  if (!BX_NVRIVA_THIS s.y_doublescan || (r & 1))
                    vid_ptr += pitch;
                  tile_ptr += info.pitch;
                }
                draw_hardware_cursor(xc, yc, &info);
                bx_gui->graphics_tile_update_in_place(xc, yc, w, h);
                SET_TILE_UPDATED(BX_NVRIVA_THIS, xti, yti, 0);
              }
            }
          }
          break;
        case 16:
          hp = BX_NVRIVA_THIS s.attribute_ctrl.horiz_pel_panning & 0x01;
          for (yc=0, yti = 0; yc<height; yc+=Y_TILESIZE, yti++) {
            for (xc=0, xti = 0; xc<width; xc+=X_TILESIZE, xti++) {
              if (GET_TILE_UPDATED (xti, yti)) {
                if (!BX_NVRIVA_THIS s.y_doublescan)
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + (yc * pitch + ((xc + hp) << 1));
                else if (!BX_NVRIVA_THIS svga_double_width)
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + ((yc >> 1) * pitch + ((xc + hp) << 1));
                else
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + ((yc >> 1) * pitch + xc + (hp << 1));
                tile_ptr = bx_gui->graphics_tile_get(xc, yc, &w, &h);
                for (r=0; r<h; r++) {
                  vid_ptr2 = vid_ptr;
                  tile_ptr2 = tile_ptr;
                  for (c=0; c<w; c++) {
                    colour = *(vid_ptr2) | *(vid_ptr2+1) << 8;
                    if (!BX_NVRIVA_THIS svga_double_width || (c & 1))
                      vid_ptr2 += 2;
                    EXTRACT_565_TO_888(colour, red, green, blue);
                    if (info.bpp >= 24) {
                      colour =
                        (BX_NVRIVA_THIS s.pel.data[red].red << 16) |
                        (BX_NVRIVA_THIS s.pel.data[green].green << 8) |
                        BX_NVRIVA_THIS s.pel.data[blue].blue;
                    } else {
                      colour = MAKE_COLOUR(
                        BX_NVRIVA_THIS s.pel.data[blue].blue, 8, info.blue_shift, info.blue_mask,
                        BX_NVRIVA_THIS s.pel.data[green].green, 8, info.green_shift, info.green_mask,
                        BX_NVRIVA_THIS s.pel.data[red].red, 8, info.red_shift, info.red_mask);
                    }
                    if (info.is_little_endian) {
                      for (i=0; i<info.bpp; i+=8)
                        *(tile_ptr2++) = colour >> i;
                    } else {
                      for (i=info.bpp-8; i>-8; i-=8)
                        *(tile_ptr2++) = colour >> i;
                    }
                  }
                  if (!BX_NVRIVA_THIS s.y_doublescan || (r & 1))
                    vid_ptr += pitch;
                  tile_ptr += info.pitch;
                }
                draw_hardware_cursor(xc, yc, &info);
                bx_gui->graphics_tile_update_in_place(xc, yc, w, h);
                SET_TILE_UPDATED(BX_NVRIVA_THIS, xti, yti, 0);
              }
            }
          }
          break;
        case 32:
          for (yc=0, yti = 0; yc<height; yc+=Y_TILESIZE, yti++) {
            for (xc=0, xti = 0; xc<width; xc+=X_TILESIZE, xti++) {
              if (GET_TILE_UPDATED (xti, yti)) {
                if (!BX_NVRIVA_THIS s.y_doublescan)
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + (yc * pitch + (xc << 2));
                else if (!BX_NVRIVA_THIS svga_double_width)
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + ((yc >> 1) * pitch + (xc << 2));
                else
                  vid_ptr = BX_NVRIVA_THIS disp_ptr + ((yc >> 1) * pitch + (xc << 1));
                tile_ptr = bx_gui->graphics_tile_get(xc, yc, &w, &h);
                for (r=0; r<h; r++) {
                  vid_ptr2 = vid_ptr;
                  tile_ptr2 = tile_ptr;
                  for (c=0; c<w; c++) {
                    blue = *(vid_ptr2);
                    green = *(vid_ptr2+1);
                    red = *(vid_ptr2+2);
                    if (!BX_NVRIVA_THIS svga_double_width || (c & 1))
                      vid_ptr2 += 4;
                    colour = MAKE_COLOUR(
                      BX_NVRIVA_THIS s.pel.data[red].red, 8, info.red_shift, info.red_mask,
                      BX_NVRIVA_THIS s.pel.data[green].green, 8, info.green_shift, info.green_mask,
                      BX_NVRIVA_THIS s.pel.data[blue].blue, 8, info.blue_shift, info.blue_mask);
                    if (info.is_little_endian) {
                      for (i=0; i<info.bpp; i+=8)
                        *(tile_ptr2++) = colour >> i;
                    } else {
                      for (i=info.bpp-8; i>-8; i-=8)
                        *(tile_ptr2++) = colour >> i;
                    }
                  }
                  if (!BX_NVRIVA_THIS s.y_doublescan || (r & 1))
                    vid_ptr += pitch;
                  tile_ptr += info.pitch;
                }
                draw_hardware_cursor(xc, yc, &info);
                bx_gui->graphics_tile_update_in_place(xc, yc, w, h);
                SET_TILE_UPDATED(BX_NVRIVA_THIS, xti, yti, 0);
              }
            }
          }
          break;
      }
    }
  }
}

void bx_nvriva_c::vertical_timer()
{
  bx_vgacore_c::vertical_timer();
  if (BX_NVRIVA_THIS vtimer_toggle) {
    BX_NVRIVA_THIS crtc_intr |= 0x00000001;
    update_irq_level();
  }
  if (BX_NVRIVA_THIS fifo_wait_acquire) {
    BX_NVRIVA_THIS fifo_wait_acquire = false;
    update_fifo_wait();
    fifo_process();
  }
}

Bit8u bx_nvriva_c::register_read8(Bit32u address)
{
  Bit8u value;
  if (address >= 0x1800 && address < 0x1900) {
    value = BX_NVRIVA_THIS pci_conf[address - 0x1800];
  } else if (address >= 0x300000 && address < 0x310000) {
    if (BX_NVRIVA_THIS pci_conf[0x50] == 0x00)
      value = BX_NVRIVA_THIS pci_rom[address - 0x300000];
    else
      value = 0x00;
  } else if (address >= 0xc0300 && address < 0xc0400) {
    Bit32u offset = address & 0x00000fff;
    if (offset == 0x3c3 || offset == 0x3c4 || offset == 0x3c5 ||
        offset == 0x3cc || offset == 0x3cf)
      value = SVGA_READ(offset, 1);
    else
      value = 0xFF;
  } else if (address >= 0x601300 && address < 0x601400) {
    Bit32u offset = address & 0x00000fff;
    if (offset == 0x3b4 || offset == 0x3b5 ||
        offset == 0x3c0 || offset == 0x3c1 ||
        offset == 0x3c2 || offset == 0x3d4 ||
        offset == 0x3d5 || offset == 0x3d8 ||
        offset == 0x3da)
      value = SVGA_READ(offset, 1);
    else
      value = 0xFF;
  } else if (address >= 0x681300 && address < 0x681400) {
    Bit32u offset = address & 0x00000fff;
    if (offset >= 0x3c6 && offset <= 0x3c9)
      value = SVGA_READ(offset, 1);
    else
      value = 0xFF;
  } else if (address >= 0x700000 && address < 0x800000) {
    value = BX_NVRIVA_THIS s.memory[(address - 0x700000) + BX_NVRIVA_THIS ramin_base];
  } else {
    value = register_read32(address);
  }
  return value;
}

void bx_nvriva_c::register_write8(Bit32u address, Bit8u value)
{
  if (address >= 0xc0300 && address < 0xc0400) {
    Bit32u offset = address & 0x00000fff;
    if (offset == 0x3c2 || offset == 0x3c3 ||
        offset == 0x3c4 || offset == 0x3c5 ||
        offset == 0x3ce || offset == 0x3cf)
      SVGA_WRITE(offset, value, 1);
  } else if (address >= 0x601300 && address < 0x601400) {
    Bit32u offset = address & 0x00000fff;
    if (offset == 0x3b4 || offset == 0x3b5 ||
        offset == 0x3c0 || offset == 0x3c1 ||
        offset == 0x3c2 || offset == 0x3d4 ||
        offset == 0x3d5 || offset == 0x3da)
      SVGA_WRITE(offset, value, 1);
  } else if (address >= 0x681300 && address < 0x681400) {
    Bit32u offset = address & 0x00000fff;
    if (offset >= 0x3c6 && offset <= 0x3c9)
      SVGA_WRITE(offset, value, 1);
  } else if (address >= 0x700000 && address < 0x800000) {
    BX_NVRIVA_THIS s.memory[(address - 0x700000) + BX_NVRIVA_THIS ramin_base] = value;
  } else {
    register_write32(address, (register_read32(address) & ~0xFF) | value);
  }
}

Bit32u bx_nvriva_c::register_read32(Bit32u address)
{
  Bit32u value;

  if (address == 0x0) {
    value = 0x20154000;
  } else if (address == 0x100) {
    value = get_mc_intr();
    if (BX_NVRIVA_THIS mc_soft_intr)
      value |= 0x80000000;
  } else if (address == 0x140)
    value = BX_NVRIVA_THIS mc_intr_en;
  else if (address == 0x200)
    value = BX_NVRIVA_THIS mc_enable;
  else if (address == 0x1100)
    value = BX_NVRIVA_THIS bus_intr;
  else if (address == 0x1140)
    value = BX_NVRIVA_THIS bus_intr_en;
  else if (address >= 0x1800 && address < 0x1900) {
    Bit32u offset = address - 0x1800;
    value =
      (BX_NVRIVA_THIS pci_conf[offset + 0] << 0) |
      (BX_NVRIVA_THIS pci_conf[offset + 1] << 8) |
      (BX_NVRIVA_THIS pci_conf[offset + 2] << 16) |
      (BX_NVRIVA_THIS pci_conf[offset + 3] << 24);
  } else if (address == 0x2100) {
    value = BX_NVRIVA_THIS fifo_intr;
  } else if (address == 0x2140) {
    value = BX_NVRIVA_THIS fifo_intr_en;
  } else if (address == 0x2210) {
    value = BX_NVRIVA_THIS fifo_ramht;
  } else if (address == 0x2214) {
    value = BX_NVRIVA_THIS fifo_ramfc;
  } else if (address == 0x2218) {
    value = BX_NVRIVA_THIS fifo_ramro;
  } else if (address == 0x2400) {
    value = 0x00000010;
    if (BX_NVRIVA_THIS fifo_cache1_get != BX_NVRIVA_THIS fifo_cache1_put)
      value = 0x00000000;
  } else if (address == 0x2504) {
    value = BX_NVRIVA_THIS fifo_mode;
  } else if (address == 0x3200) {
    value = BX_NVRIVA_THIS fifo_cache1_push0;
  } else if (address == 0x3204) {
    value = BX_NVRIVA_THIS fifo_cache1_push1;
  } else if (address == 0x3210) {
    value = BX_NVRIVA_THIS fifo_cache1_put;
  } else if (address == 0x3214) {
    value = 0x00000010;
    if (BX_NVRIVA_THIS fifo_cache1_get != BX_NVRIVA_THIS fifo_cache1_put)
      value = 0x00000000;
  } else if (address == 0x3220) {
    value = BX_NVRIVA_THIS fifo_cache1_dma_push;
  } else if (address == 0x322c) {
    value = BX_NVRIVA_THIS fifo_cache1_dma_instance;
  } else if (address == 0x3230) {
    value = 0x80000000;
  } else if (address == 0x3240) {
    value = BX_NVRIVA_THIS fifo_cache1_dma_put;
  } else if (address == 0x3244) {
    value = BX_NVRIVA_THIS fifo_cache1_dma_get;
  } else if (address == 0x3248) {
    value = BX_NVRIVA_THIS fifo_cache1_ref_cnt;
  } else if (address == 0x3250) {
    if (BX_NVRIVA_THIS fifo_cache1_get != BX_NVRIVA_THIS fifo_cache1_put)
      BX_NVRIVA_THIS fifo_cache1_pull0 |= 0x00000100;
    value = BX_NVRIVA_THIS fifo_cache1_pull0;
  } else if (address == 0x3270) {
    value = BX_NVRIVA_THIS fifo_cache1_get;
  } else if (address >= 0x3800 && address < 0x4000) {
    Bit32u offset = address - 0x3800;
    Bit32u index = offset / 8;
    if (offset % 8 == 0)
      value = BX_NVRIVA_THIS fifo_cache1_method[index];
    else
      value = BX_NVRIVA_THIS fifo_cache1_data[index];
  } else if (address == 0x9100) {
    value = BX_NVRIVA_THIS timer_intr;
  } else if (address == 0x9140) {
    value = BX_NVRIVA_THIS timer_intr_en;
  } else if (address == 0x9200)
    value = BX_NVRIVA_THIS timer_num;
  else if (address == 0x9210)
    value = BX_NVRIVA_THIS timer_den;
  else if (address == 0x9400)
    value = (Bit32u)get_current_time();
  else if (address == 0x9410)
    value = get_current_time() >> 32;
  else if (address == 0x9420)
    value = BX_NVRIVA_THIS timer_alarm;
  else if (address >= 0xc0300 && address < 0xc0400)
    value = register_read8(address);
  else if (address == 0x100000) {
    value = BX_NVRIVA_THIS pfb_boot_0;
  } else if (address == 0x100200) {
    value = BX_NVRIVA_THIS pfb_config_0;
  } else if (address == 0x100204) {
    value = BX_NVRIVA_THIS pfb_config_1;
  } else if (address == 0x101000)
    value = BX_NVRIVA_THIS straps0_primary;
  else if (address >= 0x300000 && address < 0x310000) {
    Bit32u offset = address - 0x300000;
    if (BX_NVRIVA_THIS pci_conf[0x50] == 0x00) {
      value =
        (BX_NVRIVA_THIS pci_rom[offset + 0] << 0) |
        (BX_NVRIVA_THIS pci_rom[offset + 1] << 8) |
        (BX_NVRIVA_THIS pci_rom[offset + 2] << 16) |
        (BX_NVRIVA_THIS pci_rom[offset + 3] << 24);
    } else {
      value = 0x00000000;
    }
  } else if (address == 0x400100) {
    value = BX_NVRIVA_THIS graph_intr;
  } else if (address == 0x400108) {
    value = BX_NVRIVA_THIS graph_nsource;
  } else if (address == 0x400140) {
    value = BX_NVRIVA_THIS graph_intr_en;
  } else if (address == 0x400160) {
    value = BX_NVRIVA_THIS graph_ctx_switch1;
  } else if (address == 0x400164) {
    value = BX_NVRIVA_THIS graph_ctx_switch2;
  } else if (address == 0x40016c) {
    value = BX_NVRIVA_THIS graph_ctx_switch4;
  } else if (address == 0x400700) {
    value = BX_NVRIVA_THIS graph_status;
  } else if (address == 0x400704) {
    value = BX_NVRIVA_THIS graph_trapped_addr;
  } else if (address == 0x400708) {
    value = BX_NVRIVA_THIS graph_trapped_data;
  } else if (address == 0x400714) {
    value = BX_NVRIVA_THIS graph_notify;
  } else if (address == 0x400720) {
    value = BX_NVRIVA_THIS graph_fifo;
  } else if (address == 0x400724) {
    value = BX_NVRIVA_THIS graph_bpixel;
  } else if (address == 0x400640) {
    value = BX_NVRIVA_THIS graph_offset0;
  } else if (address == 0x400670) {
    value = BX_NVRIVA_THIS graph_pitch0;
  } else if (address == 0x600100) {
    value = BX_NVRIVA_THIS crtc_intr;
  } else if (address == 0x600140) {
    value = BX_NVRIVA_THIS crtc_intr_en;
  } else if (address == 0x600800) {
    value = BX_NVRIVA_THIS crtc_start;
  } else if (address == 0x600804) {
    value = BX_NVRIVA_THIS crtc_config;
  } else if (address == 0x600808) {
    BX_NVRIVA_THIS crtc_raster_pos ^= 1;
    value = (VGA_READ(0x03da, 1) << 13) | BX_NVRIVA_THIS crtc_raster_pos;
  } else if (address == 0x60080c) {
    value = BX_NVRIVA_THIS crtc_cursor_offset;
  } else if (address == 0x600810) {
    value = BX_NVRIVA_THIS crtc_cursor_config;
  } else if (address == 0x60081c) {
    value = BX_NVRIVA_THIS crtc_gpio_ext;
  } else if (address == 0x600868) {
    Bit64u display_usec =
      bx_virt_timer.time_usec(BX_NVRIVA_THIS vsync_realtime) - BX_NVRIVA_THIS s.display_start_usec;
    display_usec = display_usec % BX_NVRIVA_THIS s.vtotal_usec;
    value = (Bit32u)(BX_NVRIVA_THIS get_crtc_vtotal() * display_usec / BX_NVRIVA_THIS s.vtotal_usec);
  } else if (address >= 0x601300 && address < 0x601400) {
    value = register_read8(address);
  } else if (address == 0x680300) {
    value = BX_NVRIVA_THIS ramdac_cu_start_pos;
  } else if (address == 0x680500) {
    value = BX_NVRIVA_THIS ramdac_nvpll;
  } else if (address == 0x680504) {
    value = BX_NVRIVA_THIS ramdac_mpll;
  } else if (address == 0x680508) {
    value = BX_NVRIVA_THIS ramdac_vpll;
  } else if (address == 0x68050c) {
    value = BX_NVRIVA_THIS ramdac_pll_select;
  } else if (address == 0x680600) {
    value = BX_NVRIVA_THIS ramdac_general_control;
  } else if (address >= 0x681300 && address < 0x681400) {
    value = register_read8(address);
  } else if (address >= 0x700000 && address < 0x800000) {
    Bit32u offset = address & 0x000fffff;
    if (offset & 3) {
      value =
        ramin_read8(offset + 0) << 0 |
        ramin_read8(offset + 1) << 8 |
        ramin_read8(offset + 2) << 16 |
        ramin_read8(offset + 3) << 24;
    } else {
      value = ramin_read32(offset);
    }
  } else if (address >= 0x800000 && address < 0x900000) {
    Bit32u chid = (address >> 16) & 0x0F;
    Bit32u offset = address & 0x1FFF;
    value = 0x00000000;
    Bit32u curchid = BX_NVRIVA_THIS fifo_cache1_push1 & 0x0F;
    if (offset == 0x10) {
      value = 0xffff;
    } else if (offset >= 0x40 && offset <= 0x44) {
      if (curchid == chid) {
        if (offset == 0x40)
          value = BX_NVRIVA_THIS fifo_cache1_dma_put;
        else if (offset == 0x44)
          value = BX_NVRIVA_THIS fifo_cache1_dma_get;
      } else {
        if (offset == 0x40)
          value = ramfc_read32(chid, 0x0);
        else if (offset == 0x44)
          value = ramfc_read32(chid, 0x4);
      }
    }
  } else {
    value = BX_NVRIVA_THIS unk_regs[address / 4];
  }
  return value;
}

void bx_nvriva_c::register_write32(Bit32u address, Bit32u value)
{
  if (address == 0x100) {
    BX_NVRIVA_THIS mc_soft_intr = (bool)(value >> 31);
    update_irq_level();
  } else if (address == 0x140) {
    BX_NVRIVA_THIS mc_intr_en = value;
    update_irq_level();
  } else if (address == 0x200) {
    BX_NVRIVA_THIS mc_enable = value;
  } else if (address >= 0x1800 && address < 0x1900) {
    BX_NVRIVA_THIS pci_write_handler(address - 0x1800, value, 4);
  } else if (address == 0x1100) {
    BX_NVRIVA_THIS bus_intr &= ~value;
    update_irq_level();
  } else if (address == 0x1140) {
    BX_NVRIVA_THIS bus_intr_en = value;
    update_irq_level();
  } else if (address == 0x2100) {
    BX_NVRIVA_THIS fifo_intr &= ~value;
    update_irq_level();
  } else if (address == 0x2140) {
    BX_NVRIVA_THIS fifo_intr_en = value;
    update_irq_level();
  } else if (address == 0x2210) {
    BX_NVRIVA_THIS fifo_ramht = value;
  } else if (address == 0x2214) {
    BX_NVRIVA_THIS fifo_ramfc = value;
  } else if (address == 0x2218) {
    BX_NVRIVA_THIS fifo_ramro = value;
  } else if (address == 0x2504) {
    bool process = (BX_NVRIVA_THIS fifo_mode | value) != BX_NVRIVA_THIS fifo_mode;
    BX_NVRIVA_THIS fifo_mode = value;
    if (process)
      fifo_process();
  } else if (address == 0x3200) {
    BX_NVRIVA_THIS fifo_cache1_push0 = value;
    if ((BX_NVRIVA_THIS fifo_cache1_push0 & 1) != 0)
      fifo_process();
  } else if (address == 0x3204) {
    BX_NVRIVA_THIS fifo_cache1_push1 = value;
  } else if (address == 0x3210) {
    BX_NVRIVA_THIS fifo_cache1_put = value;
  } else if (address == 0x3220) {
    BX_NVRIVA_THIS fifo_cache1_dma_push = value;
  } else if (address == 0x322c) {
    BX_NVRIVA_THIS fifo_cache1_dma_instance = value;
  } else if (address == 0x3240) {
    BX_NVRIVA_THIS fifo_cache1_dma_put = value;
  } else if (address == 0x3244) {
    BX_NVRIVA_THIS fifo_cache1_dma_get = value;
  } else if (address == 0x3248) {
    BX_NVRIVA_THIS fifo_cache1_ref_cnt = value;
  } else if (address == 0x3250) {
    BX_NVRIVA_THIS fifo_cache1_pull0 = value;
    if ((BX_NVRIVA_THIS fifo_cache1_pull0 & 1) != 0)
      fifo_process();
  } else if (address == 0x3270) {
    BX_NVRIVA_THIS fifo_cache1_get = value & (RIVA_CACHE1_SIZE * 4 - 1);
    if (BX_NVRIVA_THIS fifo_cache1_get != BX_NVRIVA_THIS fifo_cache1_put) {
      BX_NVRIVA_THIS fifo_intr |= 0x00000001;
    } else {
      BX_NVRIVA_THIS fifo_intr &= ~0x00000001;
      BX_NVRIVA_THIS fifo_cache1_pull0 &= ~0x00000100;
      if (BX_NVRIVA_THIS fifo_wait_soft) {
        BX_NVRIVA_THIS fifo_wait_soft = false;
        update_fifo_wait();
        fifo_process();
      }
    }
    update_irq_level();
  } else if (address == 0x9100) {
    BX_NVRIVA_THIS timer_intr &= ~value;
  } else if (address == 0x9140) {
    BX_NVRIVA_THIS timer_intr_en = value;
  } else if (address == 0x9200) {
    BX_NVRIVA_THIS timer_num = value;
  } else if (address == 0x9210) {
    BX_NVRIVA_THIS timer_den = value;
  } else if (address == 0x9400 || address == 0x9410) {
    BX_NVRIVA_THIS timer_inittime2 = bx_pc_system.time_nsec();
    if (address == 0x9400) {
      BX_NVRIVA_THIS timer_inittime1 =
        (BX_NVRIVA_THIS timer_inittime1 & BX_CONST64(0xFFFFFFFF00000000)) | value;
    } else {
      BX_NVRIVA_THIS timer_inittime1 =
        (BX_NVRIVA_THIS timer_inittime1 & BX_CONST64(0x00000000FFFFFFFF)) | ((Bit64u)value << 32);
    }
  } else if (address == 0x9420) {
    BX_NVRIVA_THIS timer_alarm = value;
  } else if (address >= 0xc0300 && address < 0xc0400) {
    register_write8(address, value);
  } else if (address == 0x100000) {
    BX_NVRIVA_THIS pfb_boot_0 = value;
  } else if (address == 0x100200) {
    BX_NVRIVA_THIS pfb_config_0 = value;
  } else if (address == 0x100204) {
    BX_NVRIVA_THIS pfb_config_1 = value;
  } else if (address == 0x101000) {
    if (value >> 31)
      BX_NVRIVA_THIS straps0_primary = value;
    else
      BX_NVRIVA_THIS straps0_primary = BX_NVRIVA_THIS straps0_primary_original;
  } else if (address == 0x400100) {
    BX_NVRIVA_THIS graph_intr &= ~value;
    update_irq_level();
    if (BX_NVRIVA_THIS fifo_wait_notify && BX_NVRIVA_THIS graph_intr == 0) {
      BX_NVRIVA_THIS fifo_wait_notify = false;
      update_fifo_wait();
      fifo_process();
    }
  } else if (address == 0x400108) {
    BX_NVRIVA_THIS graph_nsource = value;
  } else if (address == 0x400140) {
    BX_NVRIVA_THIS graph_intr_en = value;
    update_irq_level();
  } else if (address == 0x400160) {
    BX_NVRIVA_THIS graph_ctx_switch1 = value;
  } else if (address == 0x400164) {
    BX_NVRIVA_THIS graph_ctx_switch2 = value;
  } else if (address == 0x40016c) {
    BX_NVRIVA_THIS graph_ctx_switch4 = value;
  } else if (address == 0x400700) {
    BX_NVRIVA_THIS graph_status = value;
  } else if (address == 0x400704) {
    BX_NVRIVA_THIS graph_trapped_addr = value;
  } else if (address == 0x400708) {
    BX_NVRIVA_THIS graph_trapped_data = value;
  } else if (address == 0x400714) {
    BX_NVRIVA_THIS graph_notify = value;
  } else if (address == 0x40071c) {
    if ((value & 0x00000002) != 0) {
      BX_NVRIVA_THIS graph_flip_read++;
      BX_NVRIVA_THIS graph_flip_read %= BX_NVRIVA_THIS graph_flip_modulo;
      if (BX_NVRIVA_THIS fifo_wait_flip &&
          BX_NVRIVA_THIS graph_flip_read != BX_NVRIVA_THIS graph_flip_write) {
        BX_NVRIVA_THIS fifo_wait_flip = false;
        update_fifo_wait();
        fifo_process();
      }
    }
  } else if (address == 0x400720) {
    BX_NVRIVA_THIS graph_fifo = value;
  } else if (address == 0x400724) {
    BX_NVRIVA_THIS graph_bpixel = value;
  } else if (address == 0x400640) {
    BX_NVRIVA_THIS graph_offset0 = value;
  } else if (address == 0x400670) {
    BX_NVRIVA_THIS graph_pitch0 = value;
  } else if (address == 0x600100) {
    BX_NVRIVA_THIS crtc_intr &= ~value;
    update_irq_level();
  } else if (address == 0x600140) {
    BX_NVRIVA_THIS crtc_intr_en = value;
    update_irq_level();
  } else if (address == 0x600800) {
    BX_NVRIVA_THIS crtc_start = value;
    BX_NVRIVA_THIS svga_needs_update_mode = 1;
  } else if (address == 0x600804) {
    BX_NVRIVA_THIS crtc_config = value;
  } else if (address == 0x60080c) {
    BX_NVRIVA_THIS crtc_cursor_offset = value;
    BX_NVRIVA_THIS hw_cursor.offset = BX_NVRIVA_THIS crtc_cursor_offset;
  } else if (address == 0x600810) {
    BX_NVRIVA_THIS crtc_cursor_config = value;
    BX_NVRIVA_THIS hw_cursor.enabled =
      (BX_NVRIVA_THIS crtc.reg[0x31] & 0x01) || (value & 0x00000001);
    BX_NVRIVA_THIS hw_cursor.vram = false;
    BX_NVRIVA_THIS hw_cursor.size = 32;
    BX_NVRIVA_THIS hw_cursor.bpp32 = value & 0x00001000;
  } else if (address == 0x60081c) {
    BX_NVRIVA_THIS crtc_gpio_ext = value;
  } else if (address >= 0x601300 && address < 0x601400) {
    register_write8(address, value);
  } else if (address == 0x680300) {
    Bit16s prevx = BX_NVRIVA_THIS hw_cursor.x;
    Bit16s prevy = BX_NVRIVA_THIS hw_cursor.y;
    BX_NVRIVA_THIS ramdac_cu_start_pos = value;
    BX_NVRIVA_THIS hw_cursor.x = (Bit32s)BX_NVRIVA_THIS ramdac_cu_start_pos << 20 >> 20;
    BX_NVRIVA_THIS hw_cursor.y = (Bit32s)BX_NVRIVA_THIS ramdac_cu_start_pos << 4 >> 20;
    BX_NVRIVA_THIS redraw_area_nd(prevx, prevy,
      BX_NVRIVA_THIS hw_cursor.size, BX_NVRIVA_THIS hw_cursor.size);
    BX_NVRIVA_THIS redraw_area_nd(BX_NVRIVA_THIS hw_cursor.x, BX_NVRIVA_THIS hw_cursor.y,
      BX_NVRIVA_THIS hw_cursor.size, BX_NVRIVA_THIS hw_cursor.size);
  } else if (address == 0x680500) {
    BX_NVRIVA_THIS ramdac_nvpll = value;
  } else if (address == 0x680504) {
    BX_NVRIVA_THIS ramdac_mpll = value;
  } else if (address == 0x680508) {
    BX_NVRIVA_THIS ramdac_vpll = value;
    BX_NVRIVA_THIS calculate_retrace_timing();
  } else if (address == 0x68050c) {
    BX_NVRIVA_THIS ramdac_pll_select = value;
    BX_NVRIVA_THIS calculate_retrace_timing();
  } else if (address == 0x680600) {
    BX_NVRIVA_THIS ramdac_general_control = value;
    BX_NVRIVA_THIS s.dac_shift = (value >> 20) & 1 ? 0 : 2;
  } else if (address >= 0x681300 && address < 0x681400) {
    register_write8(address, value);
  } else if (address >= 0x700000 && address < 0x800000) {
    ramin_write32(address - 0x700000, value);
  } else if (address >= 0x800000 && address < 0x900000) {
    Bit32u chid = (address >> 16) & 0x0F;
    Bit32u offset = address & 0x1FFF;
    if ((BX_NVRIVA_THIS fifo_mode & (1 << chid)) != 0) {
      if (offset == 0x40) {
        Bit32u curchid = BX_NVRIVA_THIS fifo_cache1_push1 & 0x0F;
        if (curchid == chid)
          BX_NVRIVA_THIS fifo_cache1_dma_put = value;
        else
          ramfc_write32(chid, 0x0, value);
        fifo_process(chid);
      }
    } else {
      Bit32u subc = (address >> 13) & 7;
      execute_command(chid, subc, offset / 4, value);
    }
  } else {
    BX_NVRIVA_THIS unk_regs[address / 4] = value;
  }
}

Bit8u bx_nvriva_c::vram_read8(Bit32u address)
{
  return BX_NVRIVA_THIS s.memory[address & BX_NVRIVA_THIS memsize_mask];
}

Bit16u bx_nvriva_c::vram_read16(Bit32u address)
{
  address &= BX_NVRIVA_THIS memsize_mask;
  return
    BX_NVRIVA_THIS s.memory[address + 0] << 0 |
    BX_NVRIVA_THIS s.memory[address + 1] << 8;
}

Bit32u bx_nvriva_c::vram_read32(Bit32u address)
{
  address &= BX_NVRIVA_THIS memsize_mask;
  return
    BX_NVRIVA_THIS s.memory[address + 0] << 0 |
    BX_NVRIVA_THIS s.memory[address + 1] << 8 |
    BX_NVRIVA_THIS s.memory[address + 2] << 16 |
    BX_NVRIVA_THIS s.memory[address + 3] << 24;
}

void bx_nvriva_c::vram_write8(Bit32u address, Bit8u value)
{
  BX_NVRIVA_THIS s.memory[address & BX_NVRIVA_THIS memsize_mask] = value;
}

void bx_nvriva_c::vram_write16(Bit32u address, Bit16u value)
{
  address &= BX_NVRIVA_THIS memsize_mask;
  BX_NVRIVA_THIS s.memory[address + 0] = (value >> 0) & 0xFF;
  BX_NVRIVA_THIS s.memory[address + 1] = (value >> 8) & 0xFF;
}

void bx_nvriva_c::vram_write32(Bit32u address, Bit32u value)
{
  address &= BX_NVRIVA_THIS memsize_mask;
  BX_NVRIVA_THIS s.memory[address + 0] = (value >> 0) & 0xFF;
  BX_NVRIVA_THIS s.memory[address + 1] = (value >> 8) & 0xFF;
  BX_NVRIVA_THIS s.memory[address + 2] = (value >> 16) & 0xFF;
  BX_NVRIVA_THIS s.memory[address + 3] = (value >> 24) & 0xFF;
}

Bit8u bx_nvriva_c::ramin_read8(Bit32u address)
{
  return vram_read8((address & (BX_NVRIVA_THIS ramin_size - 1)) + BX_NVRIVA_THIS ramin_base);
}

Bit16u bx_nvriva_c::ramin_read16(Bit32u address)
{
  return vram_read16((address & (BX_NVRIVA_THIS ramin_size - 1)) + BX_NVRIVA_THIS ramin_base);
}

Bit32u bx_nvriva_c::ramin_read32(Bit32u address)
{
  return vram_read32((address & (BX_NVRIVA_THIS ramin_size - 1)) + BX_NVRIVA_THIS ramin_base);
}

void bx_nvriva_c::ramin_write8(Bit32u address, Bit8u value)
{
  vram_write8((address & (BX_NVRIVA_THIS ramin_size - 1)) + BX_NVRIVA_THIS ramin_base, value);
}

void bx_nvriva_c::ramin_write32(Bit32u address, Bit32u value)
{
  vram_write32((address & (BX_NVRIVA_THIS ramin_size - 1)) + BX_NVRIVA_THIS ramin_base, value);
}

Bit8u bx_nvriva_c::physical_read8(Bit32u address)
{
  Bit8u data;
  DEV_MEM_READ_PHYSICAL(address, 1, &data);
  return data;
}

Bit16u bx_nvriva_c::physical_read16(Bit32u address)
{
  Bit8u data[2];
  DEV_MEM_READ_PHYSICAL(address, 2, data);
  return data[0] << 0 | data[1] << 8;
}

Bit32u bx_nvriva_c::physical_read32(Bit32u address)
{
  Bit8u data[4];
  DEV_MEM_READ_PHYSICAL(address, 4, data);
  return data[0] << 0 | data[1] << 8 | data[2] << 16 | data[3] << 24;
}

void bx_nvriva_c::physical_write8(Bit32u address, Bit8u value)
{
  DEV_MEM_WRITE_PHYSICAL(address, 1, &value);
}

void bx_nvriva_c::physical_write16(Bit32u address, Bit16u value)
{
#ifndef BX_LITTLE_ENDIAN
  Bit8u data[2];
  data[0] = (value >> 0) & 0xFF;
  data[1] = (value >> 8) & 0xFF;
#else
  Bit8u *data = (Bit8u *)(&value);
#endif
  DEV_MEM_WRITE_PHYSICAL(address, 2, data);
}

void bx_nvriva_c::physical_write32(Bit32u address, Bit32u value)
{
#ifndef BX_LITTLE_ENDIAN
  Bit8u data[4];
  data[0] = (value >> 0) & 0xFF;
  data[1] = (value >> 8) & 0xFF;
  data[2] = (value >> 16) & 0xFF;
  data[3] = (value >> 24) & 0xFF;
#else
  Bit8u *data = (Bit8u *)(&value);
#endif
  DEV_MEM_WRITE_PHYSICAL(address, 4, data);
}

Bit32u bx_nvriva_c::dma_pt_lookup(Bit32u object, Bit32u address)
{
  Bit32u address_adj = address + (ramin_read32(object) >> 20);
  Bit32u page_offset = address_adj & 0xFFF;
  Bit32u page_index = address_adj >> 12;
  Bit32u page = ramin_read32(object + 8 + page_index * 4) & 0xFFFFF000;
  return page | page_offset;
}

Bit32u bx_nvriva_c::dma_lin_lookup(Bit32u object, Bit32u address)
{
  Bit32u adjust = ramin_read32(object) >> 20;
  Bit32u base = ramin_read32(object + 8) & 0xFFFFF000;
  return base + adjust + address;
}

Bit8u bx_nvriva_c::dma_read8(Bit32u object, Bit32u address)
{
  Bit32u flags = ramin_read32(object);
  Bit32u addr_abs;
  if (flags & 0x00002000)
    addr_abs = dma_lin_lookup(object, address);
  else
    addr_abs = dma_pt_lookup(object, address);
  if (flags & 0x00020000)
    return physical_read8(addr_abs);
  else
    return vram_read8(addr_abs & BX_NVRIVA_THIS memsize_mask);
}

Bit16u bx_nvriva_c::dma_read16(Bit32u object, Bit32u address)
{
  Bit32u flags = ramin_read32(object);
  Bit32u addr_abs;
  if (flags & 0x00002000)
    addr_abs = dma_lin_lookup(object, address);
  else
    addr_abs = dma_pt_lookup(object, address);
  if (flags & 0x00020000)
    return physical_read16(addr_abs);
  else
    return vram_read16(addr_abs & BX_NVRIVA_THIS memsize_mask);
}

Bit32u bx_nvriva_c::dma_read32(Bit32u object, Bit32u address)
{
  Bit32u flags = ramin_read32(object);
  Bit32u addr_abs;
  if (flags & 0x00002000)
    addr_abs = dma_lin_lookup(object, address);
  else
    addr_abs = dma_pt_lookup(object, address);
  if (flags & 0x00020000)
    return physical_read32(addr_abs);
  else
    return vram_read32(addr_abs & BX_NVRIVA_THIS memsize_mask);
}

void bx_nvriva_c::dma_write8(Bit32u object, Bit32u address, Bit8u value)
{
  Bit32u flags = ramin_read32(object);
  Bit32u addr_abs;
  if (flags & 0x00002000)
    addr_abs = dma_lin_lookup(object, address);
  else
    addr_abs = dma_pt_lookup(object, address);
  if (flags & 0x00020000)
    physical_write8(addr_abs, value);
  else
    vram_write8(addr_abs, value);
}

void bx_nvriva_c::dma_write16(Bit32u object, Bit32u address, Bit16u value)
{
  Bit32u flags = ramin_read32(object);
  Bit32u addr_abs;
  if (flags & 0x00002000)
    addr_abs = dma_lin_lookup(object, address);
  else
    addr_abs = dma_pt_lookup(object, address);
  if (flags & 0x00020000)
    physical_write16(addr_abs, value);
  else
    vram_write16(addr_abs, value);
}

void bx_nvriva_c::dma_write32(Bit32u object, Bit32u address, Bit32u value)
{
  Bit32u flags = ramin_read32(object);
  Bit32u addr_abs;
  if (flags & 0x00002000)
    addr_abs = dma_lin_lookup(object, address);
  else
    addr_abs = dma_pt_lookup(object, address);
  if (flags & 0x00020000)
    physical_write32(addr_abs, value);
  else
    vram_write32(addr_abs, value);
}

void bx_nvriva_c::dma_copy(Bit32u dst_obj, Bit32u dst_addr,
  Bit32u src_obj, Bit32u src_addr, Bit32u byte_count)
{
  Bit32u dst_flags = ramin_read32(dst_obj);
  Bit32u src_flags = ramin_read32(src_obj);
  Bit8u buffer[0x1000];
  Bit32u bytes_left = byte_count;
  while (bytes_left) {
    Bit32u dst_addr_abs;
    Bit32u src_addr_abs;
    if (dst_flags & 0x00002000)
      dst_addr_abs = dma_lin_lookup(dst_obj, dst_addr);
    else
      dst_addr_abs = dma_pt_lookup(dst_obj, dst_addr);
    if (src_flags & 0x00002000)
      src_addr_abs = dma_lin_lookup(src_obj, src_addr);
    else
      src_addr_abs = dma_pt_lookup(src_obj, src_addr);
    Bit32u chunk_bytes = BX_MIN(bytes_left, BX_MIN(
      0x1000 - (dst_addr_abs & 0xFFF),
      0x1000 - (src_addr_abs & 0xFFF)));
    if (src_flags & 0x00020000)
      DEV_MEM_READ_PHYSICAL(src_addr_abs, chunk_bytes, buffer);
    else
      memcpy(buffer, BX_NVRIVA_THIS s.memory + (src_addr_abs & BX_NVRIVA_THIS memsize_mask), chunk_bytes);
    if (dst_flags & 0x00020000)
      DEV_MEM_WRITE_PHYSICAL(dst_addr_abs, chunk_bytes, buffer);
    else
      memcpy(BX_NVRIVA_THIS s.memory + dst_addr_abs, buffer, chunk_bytes);
    dst_addr += chunk_bytes;
    src_addr += chunk_bytes;
    bytes_left -= chunk_bytes;
  }
}

Bit32u bx_nvriva_c::ramfc_address(Bit32u chid, Bit32u offset)
{
  Bit32u ramfc = (BX_NVRIVA_THIS fifo_ramfc & 0x1FE) << 8;
  Bit32u ramfc_ch_size = 0x20;
  return ramfc + chid * ramfc_ch_size + offset;
}

void bx_nvriva_c::ramfc_write32(Bit32u chid, Bit32u offset, Bit32u value)
{
  ramin_write32(ramfc_address(chid, offset), value);
}

Bit32u bx_nvriva_c::ramfc_read32(Bit32u chid, Bit32u offset)
{
  return ramin_read32(ramfc_address(chid, offset));
}

void bx_nvriva_c::ramht_lookup(Bit32u handle, Bit32u chid, Bit32u* object, Bit8u* engine)
{
  Bit32u ramht_addr = (BX_NVRIVA_THIS fifo_ramht & 0x1F0) << 8;
  Bit32u ramht_bits = ((BX_NVRIVA_THIS fifo_ramht >> 16) & 0x03) + 9;
  Bit32u ramht_size = 1 << ramht_bits << 3;

  Bit32u hash = 0;
  Bit32u x = handle;
  while (x) {
    hash ^= (x & ((1 << ramht_bits) - 1));
    x >>= ramht_bits;
  }
  hash ^= (chid & 0xF) << (ramht_bits - 4);
  hash = hash << 3;

  Bit32u it = hash;
  Bit32u steps = 1;
  do {
    if (ramin_read32(ramht_addr + it) == handle) {
      Bit32u context = ramin_read32(ramht_addr + it + 4);
      Bit32u ctx_chid = (context >> 24) & 0x0F;
      if (chid == ctx_chid) {
        BX_DEBUG(("ramht_lookup: 0x%08x -> 0x%08x, steps: %d", handle, context, steps));
        if (object)
          *object = (context & 0xFFFF) << 4;
        if (engine)
          *engine = (context >> 16) & 0xFF;
        return;
      }
    }
    steps++;
    it += 8;
    if (it >= ramht_size)
      it = 0;
  } while (it != hash);

  BX_PANIC(("ramht_lookup failed for 0x%08x", handle));
}

Bit64u bx_nvriva_c::get_current_time()
{
  return (BX_NVRIVA_THIS timer_inittime1 +
    bx_pc_system.time_nsec() - BX_NVRIVA_THIS timer_inittime2) & ~BX_CONST64(0x1F);
}

void bx_nvriva_c::set_irq_level(bool level)
{
  DEV_pci_set_irq(BX_NVRIVA_THIS devfunc, BX_NVRIVA_THIS pci_conf[0x3d], level);
}

Bit32u bx_nvriva_c::get_mc_intr()
{
  Bit32u value = 0x00000000;
  if (BX_NVRIVA_THIS bus_intr & BX_NVRIVA_THIS bus_intr_en)
    value |= 0x10000000;
  if (BX_NVRIVA_THIS fifo_intr & BX_NVRIVA_THIS fifo_intr_en)
    value |= 0x00000100;
  if (BX_NVRIVA_THIS graph_intr & BX_NVRIVA_THIS graph_intr_en)
    value |= 0x00001000;
  if (BX_NVRIVA_THIS crtc_intr & BX_NVRIVA_THIS crtc_intr_en)
    value |= 0x01000000;
  return value;
}

void bx_nvriva_c::update_irq_level()
{
  set_irq_level((get_mc_intr() && BX_NVRIVA_THIS mc_intr_en & 1) ||
    (BX_NVRIVA_THIS mc_soft_intr && BX_NVRIVA_THIS mc_intr_en & 2));
}

void bx_nvriva_c::update_fifo_wait()
{
  BX_NVRIVA_THIS fifo_wait =
    BX_NVRIVA_THIS fifo_wait_soft ||
    BX_NVRIVA_THIS fifo_wait_notify ||
    BX_NVRIVA_THIS fifo_wait_flip ||
    BX_NVRIVA_THIS fifo_wait_acquire;
}

void bx_nvriva_c::fifo_process()
{
  Bit32u offset = (BX_NVRIVA_THIS fifo_cache1_push1 & 0x0f) + 1;
  for (Bit32u i = 0; i < RIVA_CHANNEL_COUNT; i++)
    fifo_process((i + offset) & 0x0f);
}

void bx_nvriva_c::fifo_process(Bit32u chid)
{
  if (BX_NVRIVA_THIS fifo_wait)
    return;
  if ((BX_NVRIVA_THIS fifo_mode & (1 << chid)) == 0)
    return;
  if ((BX_NVRIVA_THIS fifo_cache1_push0 & 1) == 0)
    return;
  if ((BX_NVRIVA_THIS fifo_cache1_pull0 & 1) == 0)
    return;
  Bit32u oldchid = BX_NVRIVA_THIS fifo_cache1_push1 & 0x0F;
  if (oldchid == chid) {
    if (BX_NVRIVA_THIS fifo_cache1_dma_put == BX_NVRIVA_THIS fifo_cache1_dma_get)
      return;
  } else {
    if (ramfc_read32(chid, 0x0) == ramfc_read32(chid, 0x4))
      return;
  }
  if (oldchid != chid) {
    ramfc_write32(oldchid, 0x0, BX_NVRIVA_THIS fifo_cache1_dma_put);
    ramfc_write32(oldchid, 0x4, BX_NVRIVA_THIS fifo_cache1_dma_get);
    ramfc_write32(oldchid, 0x8, BX_NVRIVA_THIS fifo_cache1_ref_cnt);
    ramfc_write32(oldchid, 0xC, BX_NVRIVA_THIS fifo_cache1_dma_instance);
    BX_NVRIVA_THIS fifo_cache1_dma_put = ramfc_read32(chid, 0x0);
    BX_NVRIVA_THIS fifo_cache1_dma_get = ramfc_read32(chid, 0x4);
    BX_NVRIVA_THIS fifo_cache1_ref_cnt = ramfc_read32(chid, 0x8);
    BX_NVRIVA_THIS fifo_cache1_dma_instance = ramfc_read32(chid, 0xC);
    BX_NVRIVA_THIS fifo_cache1_push1 = (BX_NVRIVA_THIS fifo_cache1_push1 & ~0x0F) | chid;
  }
  BX_NVRIVA_THIS fifo_cache1_dma_push |= 0x100;
  if (BX_NVRIVA_THIS fifo_cache1_dma_instance == 0) {
    BX_PANIC(("fifo: DMA instance = 0"));
    return;
  }
  nv4_channel* ch = &BX_NVRIVA_THIS chs[chid];
  while (BX_NVRIVA_THIS fifo_cache1_dma_get != BX_NVRIVA_THIS fifo_cache1_dma_put) {
    BX_DEBUG(("fifo: processing at 0x%08x", BX_NVRIVA_THIS fifo_cache1_dma_get));
    Bit32u word = dma_read32(
      BX_NVRIVA_THIS fifo_cache1_dma_instance << 4,
      BX_NVRIVA_THIS fifo_cache1_dma_get);
    BX_NVRIVA_THIS fifo_cache1_dma_get += 4;
    if (ch->dma_state.mcnt) {
      int cmd_result = execute_command(chid,
        ch->dma_state.subc, ch->dma_state.mthd, word);
      if (cmd_result <= 1) {
        if (!ch->dma_state.ni)
          ch->dma_state.mthd++;
        ch->dma_state.mcnt--;
      } else {
        BX_NVRIVA_THIS fifo_cache1_dma_get -= 4;
      }
      if (cmd_result != 0)
        break;
    } else {
      if ((word & 0xe0000003) == 0x20000000) {
        BX_NVRIVA_THIS fifo_cache1_dma_get = word & 0x1fffffff;
        BX_DEBUG(("fifo: old jump to 0x%08x", BX_NVRIVA_THIS fifo_cache1_dma_get));
      } else if ((word & 3) == 1) {
        BX_NVRIVA_THIS fifo_cache1_dma_get = word & 0xfffffffc;
        BX_DEBUG(("fifo: jump to 0x%08x", BX_NVRIVA_THIS fifo_cache1_dma_get));
      } else if ((word & 0xa0030003) == 0) {
        ch->dma_state.mthd = (word >> 2) & 0x7ff;
        ch->dma_state.subc = (word >> 13) & 7;
        ch->dma_state.mcnt = (word >> 18) & 0x7ff;
        ch->dma_state.ni = word & 0x40000000;
      } else {
        BX_PANIC(("fifo: unexpected word 0x%08x", word));
      }
    }
  }
}



// ========== Pixel helpers ==========

Bit32u nv4_color_565_to_888(Bit16u value)
{
  Bit8u r, g, b;
  EXTRACT_565_TO_888(value, r, g, b);
  return r << 16 | g << 8 | b;
}

Bit16u nv4_color_888_to_565(Bit32u value)
{
  return (((value >> 19) & 0x1F) << 11) | (((value >> 10) & 0x3F) << 5) | ((value >> 3) & 0x1F);
}

Bit8u nv4_alpha_wrap(int value)
{
  return -(value >> 8) ^ value;
}

float nv4_uint32_as_float(Bit32u val)
{
  union {
    Bit32u ui32;
    float f;
  } conv;
  conv.ui32 = val;
  return conv.f;
}

Bit32u nv4_swizzle(Bit32u x, Bit32u y, Bit32u z, Bit32u width, Bit32u height, Bit32u depth)
{
  bool xleft = true;
  bool yleft = height != 1;
  bool zleft = depth != 1;
  Bit32u xbit = 1;
  Bit32u ybit = 1;
  Bit32u zbit = 1;
  Bit32u rbit = 1;
  Bit32u r = 0;
  do {
    if (xleft) {
      if ((x & xbit) != 0) r |= rbit;
      rbit <<= 1; xbit <<= 1; xleft = xbit < width;
    }
    if (yleft) {
      if ((y & ybit) != 0) r |= rbit;
      rbit <<= 1; ybit <<= 1; yleft = ybit < height;
    }
    if (zleft) {
      if ((z & zbit) != 0) r |= rbit;
      rbit <<= 1; zbit <<= 1; zleft = zbit < depth;
    }
  } while (xleft || yleft || zleft);
  return r;
}

double nv4_edge_function(float v0[2], float v1[2], float v2[2])
{
  return ((double)v1[0] - v0[0]) * ((double)v2[1] - v0[1]) -
         ((double)v1[1] - v0[1]) * ((double)v2[0] - v0[0]);
}

// ========== Color format conversion ==========

void bx_nvriva_c::update_color_bytes(Bit32u s2d_color_fmt, Bit32u color_fmt, Bit32u* color_bytes)
{
  if (s2d_color_fmt == 1)
    *color_bytes = 1;
  else if (color_fmt == 1 || color_fmt == 2 || color_fmt == 3)
    *color_bytes = 2;
  else if (color_fmt == 4 || color_fmt == 5)
    *color_bytes = 4;
  else
    BX_ERROR(("unknown color format: 0x%02x", color_fmt));
}

void bx_nvriva_c::update_color_bytes_s2d(nv4_channel* ch)
{
  if (ch->s2d_color_fmt == 0x1)
    ch->s2d_color_bytes = 1;
  else if (ch->s2d_color_fmt == 0x2 || ch->s2d_color_fmt == 0x4 || ch->s2d_color_fmt == 0x5)
    ch->s2d_color_bytes = 2;
  else if (ch->s2d_color_fmt == 0x6 || ch->s2d_color_fmt == 0x7 ||
           ch->s2d_color_fmt == 0xA || ch->s2d_color_fmt == 0xB)
    ch->s2d_color_bytes = 4;
  else
    BX_ERROR(("unknown 2d surface color format: 0x%02x", ch->s2d_color_fmt));
}

void bx_nvriva_c::update_color_bytes_ifc(nv4_channel* ch)
{
  BX_NVRIVA_THIS update_color_bytes(ch->s2d_color_fmt, ch->ifc_color_fmt, &ch->ifc_color_bytes);
}

void bx_nvriva_c::update_color_bytes_sifc(nv4_channel* ch)
{
  BX_NVRIVA_THIS update_color_bytes(ch->s2d_color_fmt, ch->sifc_color_fmt, &ch->sifc_color_bytes);
}

void bx_nvriva_c::update_color_bytes_tfc(nv4_channel* ch)
{
  BX_NVRIVA_THIS update_color_bytes(ch->s2d_color_fmt, ch->tfc_color_fmt, &ch->tfc_color_bytes);
}

void bx_nvriva_c::update_color_bytes_iifc(nv4_channel* ch)
{
  BX_NVRIVA_THIS update_color_bytes(0, ch->iifc_color_fmt, &ch->iifc_color_bytes);
}

// ========== Pixel read/write ==========

Bit32u bx_nvriva_c::get_pixel(Bit32u obj, Bit32u ofs, Bit32u x, Bit32u cb)
{
  if (cb == 1)
    return dma_read8(obj, ofs + x);
  else if (cb == 2)
    return dma_read16(obj, ofs + x * 2);
  else
    return dma_read32(obj, ofs + x * 4);
}

void bx_nvriva_c::put_pixel(nv4_channel* ch, Bit32u ofs, Bit32u x, Bit32u value)
{
  if (ch->s2d_color_bytes == 1)
    dma_write8(ch->s2d_img_dst, ofs + x, value);
  else if (ch->s2d_color_bytes == 2)
    dma_write16(ch->s2d_img_dst, ofs + x * 2, value);
  else if (ch->s2d_color_fmt == 6)
    dma_write32(ch->s2d_img_dst, ofs + x * 4, value & 0x00FFFFFF);
  else
    dma_write32(ch->s2d_img_dst, ofs + x * 4, value);
}

void bx_nvriva_c::put_pixel_swzs(nv4_channel* ch, Bit32u ofs, Bit32u value)
{
  if (ch->swzs_color_bytes == 1)
    dma_write8(ch->swzs_img_obj, ofs, value);
  else if (ch->swzs_color_bytes == 2)
    dma_write16(ch->swzs_img_obj, ofs, value);
  else
    dma_write32(ch->swzs_img_obj, ofs, value);
}

void bx_nvriva_c::pixel_operation(nv4_channel* ch, Bit32u op,
  Bit32u* dstcolor, const Bit32u* srccolor, Bit32u cb, Bit32u px, Bit32u py)
{
  if (op == 1) {
    Bit8u rop = ch->rop;
    if (BX_NVRIVA_THIS rop_flags[rop]) {
      Bit32u i = py % 8 * 8 + px % 8;
      Bit32u patt_color;
      if (ch->patt_type_color)
        patt_color = ch->patt_data_color[i];
      else
        patt_color = ch->patt_data_mono[i] ? ch->patt_fg_color : ch->patt_bg_color;
      bx_ternary_rop(rop, (Bit8u*)dstcolor, (Bit8u*)srccolor, (Bit8u*)&patt_color, cb);
    } else {
      BX_NVRIVA_THIS rop_handler[rop]((Bit8u*)dstcolor, (Bit8u*)srccolor, 0, 0, cb, 1);
    }
  } else if (op == 5) {
    if (cb == 4) {
      if (*srccolor) {
        Bit8u sb = *srccolor;
        Bit8u sg = *srccolor >> 8;
        Bit8u sr = *srccolor >> 16;
        Bit8u sa = *srccolor >> 24;
        Bit32u beta = ch->beta;
        if (beta != 0xFFFFFFFF) {
          Bit8u bb = beta; Bit8u bg = beta >> 8;
          Bit8u br = beta >> 16; Bit8u ba = beta >> 24;
          sb = sb * bb / 0xFF; sg = sg * bg / 0xFF;
          sr = sr * br / 0xFF; sa = sa * ba / 0xFF;
        }
        Bit8u db = *dstcolor; Bit8u dg = *dstcolor >> 8;
        Bit8u dr = *dstcolor >> 16; Bit8u da = *dstcolor >> 24;
        Bit8u isa = 0xFF - sa;
        Bit8u b = nv4_alpha_wrap(db * isa / 0xFF + sb);
        Bit8u g = nv4_alpha_wrap(dg * isa / 0xFF + sg);
        Bit8u r = nv4_alpha_wrap(dr * isa / 0xFF + sr);
        Bit8u a = nv4_alpha_wrap(da * isa / 0xFF + sa);
        *dstcolor = b << 0 | g << 8 | r << 16 | a << 24;
      }
    } else {
      Bit32u beta = ch->beta;
      Bit8u bb = beta; Bit8u bg = beta >> 8;
      Bit8u br = beta >> 16; Bit8u iba = 0xFF - (beta >> 24);
      Bit8u sb = *srccolor & 0x1F; Bit8u sg = (*srccolor >> 5) & 0x3F;
      Bit8u sr = (*srccolor >> 11) & 0x1F;
      Bit8u db = *dstcolor & 0x1F; Bit8u dg = (*dstcolor >> 5) & 0x3F;
      Bit8u dr = (*dstcolor >> 11) & 0x1F;
      Bit8u b = (db * iba + sb * bb) / 0xFF;
      Bit8u g = (dg * iba + sg * bg) / 0xFF;
      Bit8u r = (dr * iba + sr * br) / 0xFF;
      *dstcolor = b << 0 | g << 5 | r << 11;
    }
  } else {
    *dstcolor = *srccolor;
  }
}

// ========== 2D rendering functions ==========

void bx_nvriva_c::gdi_fillrect(nv4_channel* ch, bool clipped)
{
  Bit16s clipx0, clipy0, clipx1, clipy1;
  if (clipped) {
    clipx0 = ch->gdi_clip_yx0 & 0xFFFF;
    clipy0 = ch->gdi_clip_yx0 >> 16;
    clipx1 = ch->gdi_clip_yx1 & 0xFFFF;
    clipy1 = ch->gdi_clip_yx1 >> 16;
  }
  Bit16s dx, dy;
  if (clipped) {
    dx = ch->gdi_rect_yx0 & 0xFFFF;
    dy = ch->gdi_rect_yx0 >> 16;
    clipx0 -= dx; clipy0 -= dy; clipx1 -= dx; clipy1 -= dy;
  } else {
    dx = ch->gdi_rect_xy >> 16;
    dy = ch->gdi_rect_xy & 0xFFFF;
  }
  Bit16u width, height;
  if (clipped) {
    width = (ch->gdi_rect_yx1 & 0xFFFF) - dx;
    height = (ch->gdi_rect_yx1 >> 16) - dy;
  } else {
    width = ch->gdi_rect_wh >> 16;
    height = ch->gdi_rect_wh & 0xFFFF;
  }
  Bit32u pitch = ch->s2d_pitch_dst;
  Bit32u srccolor = ch->gdi_rect_color;
  Bit32u draw_offset = ch->s2d_ofs_dst + dy * pitch + dx * ch->s2d_color_bytes;
  Bit32u redraw_offset = dma_lin_lookup(ch->s2d_img_dst, draw_offset) -
    BX_NVRIVA_THIS disp_offset;
  for (Bit16u y = 0; y < height; y++) {
    for (Bit16u x = 0; x < width; x++) {
      if (!clipped || (x >= clipx0 && x < clipx1 && y >= clipy0 && y < clipy1)) {
        Bit32u dstcolor = get_pixel(ch->s2d_img_dst, draw_offset, x, ch->s2d_color_bytes);
        pixel_operation(ch, ch->gdi_operation, &dstcolor, &srccolor, ch->s2d_color_bytes, dx + x, dy + y);
        put_pixel(ch, draw_offset, x, dstcolor);
      }
    }
    draw_offset += pitch;
  }
  BX_NVRIVA_THIS redraw_area_nd(redraw_offset, width, height);
}

void bx_nvriva_c::gdi_blit(nv4_channel* ch, Bit32u type)
{
  Bit16s dx = ch->gdi_image_xy & 0xFFFF;
  Bit16s dy = ch->gdi_image_xy >> 16;
  Bit16s clipx0 = (ch->gdi_clip_yx0 & 0xFFFF) - dx;
  Bit16s clipy0 = (ch->gdi_clip_yx0 >> 16) - dy;
  Bit16s clipx1 = (ch->gdi_clip_yx1 & 0xFFFF) - dx;
  Bit16s clipy1 = (ch->gdi_clip_yx1 >> 16) - dy;
  Bit32u swidth = ch->gdi_image_swh & 0xFFFF;
  Bit32u dwidth = type ? ch->gdi_image_dwh & 0xFFFF : swidth;
  Bit32u height = ch->gdi_image_swh >> 16;
  Bit32u pitch = ch->s2d_pitch_dst;
  Bit32u bg_color = ch->gdi_bg_color;
  Bit32u fg_color = ch->gdi_fg_color;
  if (ch->s2d_color_bytes == 4 && ch->gdi_color_fmt != 3) {
    bg_color = nv4_color_565_to_888(bg_color);
    fg_color = nv4_color_565_to_888(fg_color);
  }
  Bit32u draw_offset = ch->s2d_ofs_dst + dy * pitch + dx * ch->s2d_color_bytes;
  Bit32u redraw_offset = dma_lin_lookup(ch->s2d_img_dst, draw_offset) -
    BX_NVRIVA_THIS disp_offset;
  Bit32u bit_index = 0;
  for (Bit16u y = 0; y < height; y++) {
    for (Bit16u x = 0; x < dwidth; x++) {
      if (x >= clipx0 && x < clipx1 && y >= clipy0 && y < clipy1) {
        Bit32u word_offset = bit_index / 32;
        Bit32u bit_offset = bit_index % 32;
        if (ch->gdi_mono_fmt == 1) bit_offset ^= 7;
        bool pixel = (ch->gdi_words[word_offset] >> bit_offset) & 1;
        if (type || (!type && pixel)) {
          Bit32u dstcolor = get_pixel(ch->s2d_img_dst, draw_offset, x, ch->s2d_color_bytes);
          Bit32u srccolor = pixel ? fg_color : bg_color;
          pixel_operation(ch, ch->gdi_operation, &dstcolor, &srccolor, ch->s2d_color_bytes, dx + x, dy + y);
          put_pixel(ch, draw_offset, x, dstcolor);
        }
      }
      bit_index++;
    }
    bit_index += swidth - dwidth;
    draw_offset += pitch;
  }
  BX_NVRIVA_THIS redraw_area_nd(redraw_offset, dwidth, height);
}

void bx_nvriva_c::rect(nv4_channel* ch)
{
  Bit16s dx = ch->rect_yx & 0xFFFF;
  Bit16s dy = ch->rect_yx >> 16;
  Bit16u width = ch->rect_hw & 0xFFFF;
  Bit16u height = ch->rect_hw >> 16;
  Bit32u pitch = ch->s2d_pitch_dst;
  Bit32u srccolor = ch->rect_color;
  Bit32u draw_offset = ch->s2d_ofs_dst + dy * pitch + dx * ch->s2d_color_bytes;
  Bit32u redraw_offset = dma_lin_lookup(ch->s2d_img_dst, draw_offset) -
    BX_NVRIVA_THIS disp_offset;
  for (Bit16u y = 0; y < height; y++) {
    for (Bit16u x = 0; x < width; x++) {
      Bit32u dstcolor = get_pixel(ch->s2d_img_dst, draw_offset, x, ch->s2d_color_bytes);
      pixel_operation(ch, ch->rect_operation, &dstcolor, &srccolor, ch->s2d_color_bytes, dx + x, dy + y);
      put_pixel(ch, draw_offset, x, dstcolor);
    }
    draw_offset += pitch;
  }
  BX_NVRIVA_THIS redraw_area_nd(redraw_offset, width, height);
}

void bx_nvriva_c::ifc(nv4_channel* ch, Bit32u word)
{
  Bit32u chromacolor;
  bool chroma_enabled = false;
  if (ch->ifc_color_key_enable) {
    if (ch->ifc_color_bytes == 4) {
      chromacolor = ch->chroma_color & 0x00FFFFFF;
      chroma_enabled = ch->chroma_color & 0xFF000000;
    } else if (ch->ifc_color_bytes == 2) {
      chromacolor = ch->chroma_color & 0x0000FFFF;
      chroma_enabled = ch->chroma_color & 0xFFFF0000;
    } else {
      chromacolor = ch->chroma_color & 0x000000FF;
      chroma_enabled = ch->chroma_color & 0xFFFFFF00;
    }
  }
  for (Bit32u i = 0; i < ch->ifc_pixels_per_word; i++) {
    if (ch->ifc_x >= ch->ifc_clip_x0 && ch->ifc_x < ch->ifc_clip_x1 &&
        ch->ifc_y >= ch->ifc_clip_y0 && ch->ifc_y < ch->ifc_clip_y1) {
      Bit32u srccolor;
      if (ch->ifc_color_bytes == 4)
        srccolor = word;
      else if (ch->ifc_color_bytes == 2)
        srccolor = i == 0 ? word & 0xffff : word >> 16;
      else
        srccolor = (word >> (i * 8)) & 0xff;
      if (!chroma_enabled || srccolor != chromacolor) {
        Bit32u dstcolor = get_pixel(ch->s2d_img_dst, ch->ifc_draw_offset, ch->ifc_x, ch->s2d_color_bytes);
        if (ch->ifc_color_bytes == 4 && ch->s2d_color_bytes == 2)
          dstcolor = nv4_color_565_to_888(dstcolor);
        pixel_operation(ch, ch->ifc_operation, &dstcolor, &srccolor,
          ch->ifc_color_bytes, ch->ifc_ofs_x + ch->ifc_x, ch->ifc_ofs_y + ch->ifc_y);
        if (ch->ifc_color_bytes == 4 && ch->s2d_color_bytes == 2)
          dstcolor = nv4_color_888_to_565(dstcolor);
        put_pixel(ch, ch->ifc_draw_offset, ch->ifc_x, dstcolor);
      }
    }
    ch->ifc_x++;
    if (ch->ifc_x >= ch->ifc_src_width) {
      BX_NVRIVA_THIS redraw_area_nd(ch->ifc_redraw_offset, ch->ifc_dst_width, 1);
      ch->ifc_draw_offset += ch->s2d_pitch_dst;
      ch->ifc_redraw_offset += ch->s2d_pitch_dst;
      ch->ifc_x = 0;
      ch->ifc_y++;
    }
  }
}

void bx_nvriva_c::iifc(nv4_channel* ch)
{
  Bit16s dx = ch->iifc_yx & 0xFFFF;
  Bit16s dy = ch->iifc_yx >> 16;
  Bit16s clipx0 = ch->clip_x - dx;
  Bit16s clipy0 = ch->clip_y - dy;
  Bit16s clipx1 = clipx0 + ch->clip_width;
  Bit16s clipy1 = clipy0 + ch->clip_height;
  Bit32u swidth = ch->iifc_shw & 0xFFFF;
  Bit32u dwidth = ch->iifc_dhw & 0xFFFF;
  Bit32u height = ch->iifc_dhw >> 16;
  Bit32u pitch = ch->s2d_pitch_dst;
  Bit32u draw_offset = ch->s2d_ofs_dst + dy * pitch + dx * ch->s2d_color_bytes;
  Bit32u redraw_offset = dma_lin_lookup(ch->s2d_img_dst, draw_offset) -
    BX_NVRIVA_THIS disp_offset;
  Bit32u symbol_index = 0;
  for (Bit16u y = 0; y < height; y++) {
    for (Bit16u x = 0; x < dwidth; x++) {
      if (x >= clipx0 && x < clipx1 && y >= clipy0 && y < clipy1) {
        Bit8u symbol;
        if (ch->iifc_bpp4) {
          Bit32u word_offset = symbol_index / 8;
          Bit32u symbol_offset = (symbol_index % 8 ^ 1) * 4;
          symbol = ch->iifc_words[word_offset] >> symbol_offset & 0xF;
        } else {
          Bit32u word_offset = symbol_index / 4;
          Bit32u symbol_offset = symbol_index % 4 * 8;
          symbol = ch->iifc_words[word_offset] >> symbol_offset & 0xFF;
        }
        Bit32u dstcolor = get_pixel(ch->s2d_img_dst, draw_offset, x, ch->s2d_color_bytes);
        if (ch->iifc_color_bytes == 4) {
          Bit32u srccolor = dma_read32(ch->iifc_palette, ch->iifc_palette_ofs + symbol * 4);
          if (ch->s2d_color_bytes == 2) dstcolor = nv4_color_565_to_888(dstcolor);
          pixel_operation(ch, ch->iifc_operation, &dstcolor, &srccolor, 4, dx + x, dy + y);
          if (ch->s2d_color_bytes == 2) dstcolor = nv4_color_888_to_565(dstcolor);
        } else if (ch->iifc_color_bytes == 2) {
          Bit32u srccolor = dma_read16(ch->iifc_palette, ch->iifc_palette_ofs + symbol * 2);
          pixel_operation(ch, ch->iifc_operation, &dstcolor, &srccolor, 2, dx + x, dy + y);
        }
        put_pixel(ch, draw_offset, x, dstcolor);
      }
      symbol_index++;
    }
    symbol_index += swidth - dwidth;
    draw_offset += pitch;
  }
  BX_NVRIVA_THIS redraw_area_nd(redraw_offset, dwidth, height);
}

void bx_nvriva_c::sifc(nv4_channel* ch)
{
  Bit16u dx = ch->sifc_clip_yx & 0xFFFF;
  Bit16u dy = ch->sifc_clip_yx >> 16;
  Bit32u dsdx = (Bit32u)(BX_CONST64(1099511627776) / ch->sifc_dxds);
  Bit32u dtdy = (Bit32u)(BX_CONST64(1099511627776) / ch->sifc_dydt);
  Bit32u swidth = ch->sifc_shw & 0xFFFF;
  Bit32u dwidth = ch->sifc_clip_hw & 0xFFFF;
  Bit32u height = ch->sifc_clip_hw >> 16;
  Bit32u pitch = ch->s2d_pitch_dst;
  Bit32u draw_offset = ch->s2d_ofs_dst + dy * pitch + dx * ch->s2d_color_bytes;
  Bit32u redraw_offset = dma_lin_lookup(ch->s2d_img_dst, draw_offset) -
    BX_NVRIVA_THIS disp_offset;
  Bit32s sx0 = ((ch->sifc_syx & 0xFFFF) << 16) - (dx << 20) - 0x80000;
  Bit32s sy = (ch->sifc_syx & 0xFFFF0000) - (dy << 20) - 0x80000;
  if (sx0 < 0) sx0 = 0;
  if (sy < 0) sy = 0;
  Bit32u symbol_offset_y = 0;
  for (Bit16u y = 0; y < height; y++) {
    Bit32u sx = sx0;
    for (Bit16u x = 0; x < dwidth; x++) {
      Bit32u dstcolor = get_pixel(ch->s2d_img_dst, draw_offset, x, ch->s2d_color_bytes);
      Bit32u srccolor;
      Bit32u symbol_offset = symbol_offset_y + (sx >> 20);
      if (ch->sifc_color_bytes == 4)
        srccolor = ch->sifc_words[symbol_offset];
      else if (ch->sifc_color_bytes == 2) {
        Bit16u *sifc_words16 = (Bit16u*)ch->sifc_words;
        srccolor = sifc_words16[symbol_offset];
      } else {
        Bit8u *sifc_words8 = (Bit8u*)ch->sifc_words;
        srccolor = sifc_words8[symbol_offset];
      }
      if (ch->sifc_color_bytes == 4 && ch->s2d_color_bytes == 2)
        dstcolor = nv4_color_565_to_888(dstcolor);
      pixel_operation(ch, ch->sifc_operation, &dstcolor, &srccolor, ch->sifc_color_bytes, dx + x, dy + y);
      if (ch->sifc_color_bytes == 4 && ch->s2d_color_bytes == 2)
        dstcolor = nv4_color_888_to_565(dstcolor);
      put_pixel(ch, draw_offset, x, dstcolor);
      sx += dsdx;
    }
    sy += dtdy;
    symbol_offset_y = (sy >> 20) * swidth;
    draw_offset += pitch;
  }
  BX_NVRIVA_THIS redraw_area_nd(redraw_offset, dwidth, height);
}

void bx_nvriva_c::copyarea(nv4_channel* ch)
{
  Bit16u sx = ch->blit_syx & 0xFFFF;
  Bit16u sy = ch->blit_syx >> 16;
  Bit16u dx = ch->blit_dyx & 0xFFFF;
  Bit16u dy = ch->blit_dyx >> 16;
  Bit16u width = ch->blit_hw & 0xFFFF;
  Bit16u height = ch->blit_hw >> 16;
  Bit32u spitch = ch->s2d_pitch_src;
  Bit32u dpitch = ch->s2d_pitch_dst;
  Bit32u src_offset = ch->s2d_ofs_src;
  Bit32u draw_offset = ch->s2d_ofs_dst;
  bool xdir = dx > sx;
  bool ydir = dy > sy;
  src_offset += (sy + ydir * (height - 1)) * spitch + sx * ch->s2d_color_bytes;
  Bit32u redraw_offset = dma_lin_lookup(ch->s2d_img_dst, draw_offset) +
    dy * dpitch + dx * ch->s2d_color_bytes - BX_NVRIVA_THIS disp_offset;
  draw_offset += (dy + ydir * (height - 1)) * dpitch + dx * ch->s2d_color_bytes;
  Bit32u chromacolor;
  bool chroma_enabled = false;
  if (ch->blit_color_key_enable) {
    if (ch->s2d_color_bytes == 4) {
      chromacolor = ch->chroma_color & 0x00FFFFFF;
      chroma_enabled = ch->chroma_color & 0xFF000000;
    } else if (ch->s2d_color_bytes == 2) {
      chromacolor = ch->chroma_color & 0x0000FFFF;
      chroma_enabled = ch->chroma_color & 0xFFFF0000;
    } else {
      chromacolor = ch->chroma_color & 0x000000FF;
      chroma_enabled = ch->chroma_color & 0xFFFFFF00;
    }
  }
  for (Bit16u y = 0; y < height; y++) {
    for (Bit16u x = 0; x < width; x++) {
      Bit16u xa = xdir ? width - x - 1 : x;
      Bit32u srccolor = get_pixel(ch->s2d_img_src, src_offset, xa, ch->s2d_color_bytes);
      if (!chroma_enabled || srccolor != chromacolor) {
        Bit32u dstcolor = get_pixel(ch->s2d_img_dst, draw_offset, xa, ch->s2d_color_bytes);
        pixel_operation(ch, ch->blit_operation, &dstcolor, &srccolor, ch->s2d_color_bytes, dx + x, dy + y);
        put_pixel(ch, draw_offset, xa, dstcolor);
      }
    }
    src_offset += spitch * (1 - 2 * ydir);
    draw_offset += dpitch * (1 - 2 * ydir);
  }
  BX_NVRIVA_THIS redraw_area_nd(redraw_offset, width, height);
}

void bx_nvriva_c::m2mf(nv4_channel* ch)
{
  Bit32u src_offset = ch->m2mf_src_offset;
  Bit32u dst_offset = ch->m2mf_dst_offset;
  for (Bit16u y = 0; y < ch->m2mf_line_count; y++) {
    dma_copy(ch->m2mf_dst, dst_offset, ch->m2mf_src, src_offset, ch->m2mf_line_length);
    src_offset += ch->m2mf_src_pitch;
    dst_offset += ch->m2mf_dst_pitch;
  }
  Bit32u dma_target = ramin_read32(ch->m2mf_dst) >> 12 & 0xFF;
  if (dma_target == 0x03 || dma_target == 0x0b) {
    Bit32u redraw_offset = dma_lin_lookup(ch->m2mf_dst, ch->m2mf_dst_offset) -
      BX_NVRIVA_THIS disp_offset;
    Bit32u width = ch->m2mf_line_length / (BX_NVRIVA_THIS svga_bpp >> 3);
    BX_NVRIVA_THIS redraw_area_nd(redraw_offset, width, ch->m2mf_line_count);
  }
}

void bx_nvriva_c::tfc(nv4_channel* ch)
{
  Bit16u dx = ch->tfc_yx & 0xFFFF;
  Bit16u dy = ch->tfc_yx >> 16;
  Bit16s clipx0 = (ch->tfc_clip_wx & 0xFFFF) - dx;
  Bit16s clipy0 = (ch->tfc_clip_hy & 0xFFFF) - dy;
  Bit16s clipx1 = clipx0 + (ch->tfc_clip_wx >> 16);
  Bit16s clipy1 = clipy0 + (ch->tfc_clip_hy >> 16);
  Bit32u width = ch->tfc_hw & 0xFFFF;
  Bit32u height = ch->tfc_hw >> 16;
  Bit32u word_offset = 0;
  if (ch->tfc_swizzled) {
    for (Bit16u y = 0; y < height; y++) {
      for (Bit16u x = 0; x < width; x++) {
        if (x >= clipx0 && x < clipx1 && y >= clipy0 && y < clipy1) {
          Bit32u srccolor;
          if (ch->tfc_color_bytes == 4)
            srccolor = ch->tfc_words[word_offset];
          else if (ch->tfc_color_bytes == 2) {
            Bit16u *tfc_words16 = (Bit16u*)ch->tfc_words;
            srccolor = tfc_words16[word_offset];
          } else {
            Bit8u *tfc_words8 = (Bit8u*)ch->tfc_words;
            srccolor = tfc_words8[word_offset];
          }
          put_pixel_swzs(ch, ch->swzs_ofs +
            nv4_swizzle(x + dx, y + dy, 0, ch->swzs_width, ch->swzs_height, 1) *
            ch->swzs_color_bytes, srccolor);
        }
        word_offset++;
      }
    }
  } else {
    Bit32u pitch = ch->s2d_pitch_dst;
    Bit32u draw_offset = ch->s2d_ofs_dst + dy * pitch + dx * ch->s2d_color_bytes;
    for (Bit16u y = 0; y < height; y++) {
      for (Bit16u x = 0; x < width; x++) {
        if (x >= clipx0 && x < clipx1 && y >= clipy0 && y < clipy1) {
          Bit32u srccolor;
          if (ch->tfc_color_bytes == 4)
            srccolor = ch->tfc_words[word_offset];
          else if (ch->tfc_color_bytes == 2) {
            Bit16u *tfc_words16 = (Bit16u*)ch->tfc_words;
            srccolor = tfc_words16[word_offset];
          } else {
            Bit8u *tfc_words8 = (Bit8u*)ch->tfc_words;
            srccolor = tfc_words8[word_offset];
          }
          put_pixel(ch, draw_offset, x, srccolor);
        }
        word_offset++;
      }
      draw_offset += pitch;
    }
  }
}

void bx_nvriva_c::sifm(nv4_channel* ch, bool swizzled)
{
  Bit16u dx = ch->sifm_dyx & 0xFFFF;
  Bit16u dy = ch->sifm_dyx >> 16;
  Bit16u dwidth = ch->sifm_dhw & 0xFFFF;
  Bit16u dheight = ch->sifm_dhw >> 16;
  Bit32u spitch = ch->sifm_sfmt & 0xFFFF;
  if (ch->sifm_dudx == 0x00100000 && ch->sifm_dvdy == 0x00100000) {
    Bit16u sx = (ch->sifm_syx & 0xFFFF) >> 4;
    Bit16u sy = (ch->sifm_syx >> 16) >> 4;
    Bit32u src_offset = ch->sifm_sofs + sy * spitch + sx * ch->sifm_color_bytes;
    if (swizzled) {
      for (Bit16u y = 0; y < dheight; y++) {
        for (Bit16u x = 0; x < dwidth; x++) {
          Bit32u srccolor = get_pixel(ch->sifm_src, src_offset, x, ch->sifm_color_bytes);
          if (ch->sifm_color_bytes == 2 && ch->swzs_color_bytes == 4)
            srccolor = nv4_color_565_to_888(srccolor);
          put_pixel_swzs(ch, ch->swzs_ofs +
            nv4_swizzle(x + dx, y + dy, 0, ch->swzs_width, ch->swzs_height, 1) *
            ch->swzs_color_bytes, srccolor);
        }
        src_offset += spitch;
      }
    } else {
      Bit32u dpitch = ch->s2d_pitch_dst;
      Bit32u draw_offset = ch->s2d_ofs_dst + dy * dpitch + dx * ch->s2d_color_bytes;
      Bit32u redraw_offset = dma_lin_lookup(ch->s2d_img_dst, draw_offset) -
        BX_NVRIVA_THIS disp_offset;
      for (Bit16u y = 0; y < dheight; y++) {
        for (Bit16u x = 0; x < dwidth; x++) {
          Bit32u dstcolor = get_pixel(ch->s2d_img_dst, draw_offset, x, ch->s2d_color_bytes);
          Bit32u srccolor = get_pixel(ch->sifm_src, src_offset, x, ch->sifm_color_bytes);
          if (ch->sifm_color_fmt == 4) srccolor |= 0xFF000000;
          pixel_operation(ch, ch->sifm_operation, &dstcolor, &srccolor, ch->s2d_color_bytes, dx + x, dy + y);
          put_pixel(ch, draw_offset, x, dstcolor);
        }
        src_offset += spitch;
        draw_offset += dpitch;
      }
      BX_NVRIVA_THIS redraw_area_nd(redraw_offset, dwidth, dheight);
    }
  } else {
    Bit32s sx0 = ((ch->sifm_syx & 0xFFFF) << 16) - 0x80000;
    Bit32s sy = (ch->sifm_syx & 0xFFFF0000) + ((Bit32s)ch->sifm_dvdy < 0 ? 0x80000 : -0x80000);
    if (sx0 < 0) sx0 = 0;
    if (sy < 0) sy = 0;
    if (swizzled) {
      for (Bit16u y = 0; y < dheight; y++) {
        Bit32u sx = sx0;
        Bit32u src_offset = ch->sifm_sofs + (sy >> 20) * spitch;
        for (Bit16u x = 0; x < dwidth; x++) {
          Bit32u srccolor = get_pixel(ch->sifm_src, src_offset, sx >> 20, ch->sifm_color_bytes);
          if (ch->sifm_color_bytes == 2 && ch->swzs_color_bytes == 4)
            srccolor = nv4_color_565_to_888(srccolor);
          put_pixel_swzs(ch, ch->swzs_ofs +
            nv4_swizzle(x + dx, y + dy, 0, ch->swzs_width, ch->swzs_height, 1) *
            ch->swzs_color_bytes, srccolor);
          sx += ch->sifm_dudx;
        }
        sy += ch->sifm_dvdy;
      }
    } else {
      Bit32u dpitch = ch->s2d_pitch_dst;
      Bit32u draw_offset = ch->s2d_ofs_dst + dy * dpitch + dx * ch->s2d_color_bytes;
      Bit32u redraw_offset = dma_lin_lookup(ch->s2d_img_dst, draw_offset) -
        BX_NVRIVA_THIS disp_offset;
      for (Bit16u y = 0; y < dheight; y++) {
        Bit32u sx = sx0;
        Bit32u src_offset = ch->sifm_sofs + (sy >> 20) * spitch;
        for (Bit16u x = 0; x < dwidth; x++) {
          Bit32u dstcolor = get_pixel(ch->s2d_img_dst, draw_offset, x, ch->s2d_color_bytes);
          Bit32u srccolor = get_pixel(ch->sifm_src, src_offset, sx >> 20, ch->sifm_color_bytes);
          if (ch->sifm_color_fmt == 4) srccolor |= 0xFF000000;
          pixel_operation(ch, ch->sifm_operation, &dstcolor, &srccolor, ch->s2d_color_bytes, dx + x, dy + y);
          put_pixel(ch, draw_offset, x, dstcolor);
          sx += ch->sifm_dudx;
        }
        sy += ch->sifm_dvdy;
        draw_offset += dpitch;
      }
      BX_NVRIVA_THIS redraw_area_nd(redraw_offset, dwidth, dheight);
    }
  }
}

// ========== 2D method dispatch ==========

void bx_nvriva_c::execute_clip(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x0c0) {
    ch->clip_x = (Bit16u)param;
    ch->clip_y = param >> 16;
  } else if (method == 0x0c1) {
    ch->clip_width = (Bit16u)param;
    ch->clip_height = param >> 16;
  }
}

void bx_nvriva_c::execute_rop(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x0c0)
    ch->rop = param;
}

void bx_nvriva_c::execute_patt(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x0c2)
    ch->patt_shape = param;
  else if (method == 0x0c3)
    ch->patt_type_color = param == 2;
  else if (method == 0x0c4)
    ch->patt_bg_color = param;
  else if (method == 0x0c5)
    ch->patt_fg_color = param;
  else if (method == 0x0c6 || method == 0x0c7) {
    for (Bit32u i = 0; i < 32; i++)
      ch->patt_data_mono[i + (method & 1) * 32] = 1 << (i ^ 7) & param;
  } else if (method >= 0x100 && method < 0x110) {
    Bit32u i = (method - 0x100) * 4;
    ch->patt_data_color[i] = param & 0xFF;
    ch->patt_data_color[i + 1] = (param >> 8) & 0xFF;
    ch->patt_data_color[i + 2] = (param >> 16) & 0xFF;
    ch->patt_data_color[i + 3] = param >> 24;
  } else if (method >= 0x140 && method < 0x160) {
    Bit32u i = (method - 0x140) * 2;
    ch->patt_data_color[i] = param & 0xFFFF;
    ch->patt_data_color[i + 1] = param >> 16;
  } else if (method >= 0x1c0 && method < 0x200)
    ch->patt_data_color[method - 0x1c0] = param;
}

void bx_nvriva_c::execute_chroma(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x0c0)
    ch->chroma_color_fmt = param;
  else if (method == 0x0c1)
    ch->chroma_color = param;
}

void bx_nvriva_c::execute_beta(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x0c0)
    ch->beta = param;
}

void bx_nvriva_c::execute_surf2d(nv4_channel* ch, Bit32u method, Bit32u param)
{
  ch->s2d_locked = true;
  if (method == 0x061)
    ch->s2d_img_src = param;
  else if (method == 0x062)
    ch->s2d_img_dst = param;
  else if (method == 0x0c0) {
    ch->s2d_color_fmt = param;
    Bit32u s2d_color_bytes_prev = ch->s2d_color_bytes;
    update_color_bytes_s2d(ch);
    if (ch->s2d_color_bytes != s2d_color_bytes_prev &&
        (ch->s2d_color_bytes == 1 || s2d_color_bytes_prev == 1)) {
      update_color_bytes_ifc(ch);
      update_color_bytes_sifc(ch);
      update_color_bytes_tfc(ch);
    }
  } else if (method == 0x0c1) {
    ch->s2d_pitch_src = param & 0xFFFF;
    ch->s2d_pitch_dst = param >> 16;
  } else if (method == 0x0c2)
    ch->s2d_ofs_src = param;
  else if (method == 0x0c3)
    ch->s2d_ofs_dst = param;
}

void bx_nvriva_c::execute_swzsurf(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x061)
    ch->swzs_img_obj = param;
  else if (method == 0x0c0) {
    ch->swzs_fmt = param;
    ch->swzs_width = 1 << ((param >> 16) & 0xff);
    ch->swzs_height = 1 << (param >> 24);
    Bit32u color_fmt = param & 0xffff;
    if (color_fmt == 1)
      ch->swzs_color_bytes = 1;
    else if (color_fmt == 2 || color_fmt == 4)
      ch->swzs_color_bytes = 2;
    else if (color_fmt == 0x6 || color_fmt == 0xA || color_fmt == 0xB)
      ch->swzs_color_bytes = 4;
    else
      BX_ERROR(("unknown swizzled surface color format: 0x%02x", color_fmt));
  } else if (method == 0x0c1)
    ch->swzs_ofs = param;
}

void bx_nvriva_c::execute_rect(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x0bf)
    ch->rect_operation = param;
  else if (method == 0x0c0)
    ch->rect_color_fmt = param;
  else if (method == 0x0c1)
    ch->rect_color = param;
  else if (method >= 0x100 && method < 0x120) {
    if (method & 1) {
      ch->rect_hw = param;
      rect(ch);
    } else
      ch->rect_yx = param;
  }
}

void bx_nvriva_c::execute_imageblit(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x061)
    ch->blit_color_key_enable = (ramin_read32(param) & 0xFF) != 0x30;
  else if (method == 0x0bf)
    ch->blit_operation = param;
  else if (method == 0x0c0)
    ch->blit_syx = param;
  else if (method == 0x0c1)
    ch->blit_dyx = param;
  else if (method == 0x0c2) {
    ch->blit_hw = param;
    copyarea(ch);
  }
}

void bx_nvriva_c::execute_ifc(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x061)
    ch->ifc_color_key_enable = (ramin_read32(param) & 0xFF) != 0x30;
  else if (method == 0x062)
    ch->ifc_clip_enable = (ramin_read32(param) & 0xFF) != 0x30;
  else if (method == 0x0bf)
    ch->ifc_operation = param;
  else if (method == 0x0c0) {
    ch->ifc_color_fmt = param;
    update_color_bytes_ifc(ch);
    ch->ifc_pixels_per_word = 4 / ch->ifc_color_bytes;
  } else if (method == 0x0c1) {
    ch->ifc_x = 0;
    ch->ifc_y = 0;
    ch->ifc_ofs_x = param & 0xFFFF;
    ch->ifc_ofs_y = param >> 16;
    ch->ifc_draw_offset = ch->s2d_ofs_dst +
      ch->ifc_ofs_y * ch->s2d_pitch_dst + ch->ifc_ofs_x * ch->s2d_color_bytes;
    ch->ifc_redraw_offset = dma_lin_lookup(ch->s2d_img_dst,
      ch->ifc_draw_offset) - BX_NVRIVA_THIS disp_offset;
  } else if (method == 0x0c2) {
    ch->ifc_dst_width = param & 0xFFFF;
    ch->ifc_dst_height = param >> 16;
    ch->ifc_clip_x0 = 0;
    ch->ifc_clip_y0 = 0;
    ch->ifc_clip_x1 = ch->ifc_dst_width;
    ch->ifc_clip_y1 = ch->ifc_dst_height;
    if (ch->ifc_clip_enable) {
      Bit32s clipx0 = ch->clip_x - ch->ifc_ofs_x;
      Bit32s clipy0 = ch->clip_y - ch->ifc_ofs_y;
      Bit32s clipx1 = clipx0 + ch->clip_width;
      Bit32s clipy1 = clipy0 + ch->clip_height;
      ch->ifc_clip_x0 = BX_MAX((Bit32s)ch->ifc_clip_x0, clipx0);
      ch->ifc_clip_y0 = BX_MAX((Bit32s)ch->ifc_clip_y0, clipy0);
      ch->ifc_clip_x1 = BX_MIN((Bit32s)ch->ifc_clip_x1, clipx1);
      ch->ifc_clip_y1 = BX_MIN((Bit32s)ch->ifc_clip_y1, clipy1);
    }
  } else if (method == 0x0c3) {
    ch->ifc_src_width = param & 0xFFFF;
    ch->ifc_src_height = param >> 16;
  } else if (method >= 0x100 && method < 0x800) {
    ifc(ch, param);
  }
}

void bx_nvriva_c::execute_iifc(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x061)
    ch->iifc_palette = param;
  else if (method == 0x0f9)
    ch->iifc_operation = param;
  else if (method == 0x0fa) {
    ch->iifc_color_fmt = param;
    update_color_bytes_iifc(ch);
  } else if (method == 0x0fb)
    ch->iifc_bpp4 = param;
  else if (method == 0x0fc)
    ch->iifc_palette_ofs = param;
  else if (method == 0x0fd)
    ch->iifc_yx = param;
  else if (method == 0x0fe)
    ch->iifc_dhw = param;
  else if (method == 0x0ff) {
    ch->iifc_shw = param;
    Bit32u width = ch->iifc_shw & 0xFFFF;
    Bit32u height = ch->iifc_shw >> 16;
    Bit32u wordCount = ALIGN(width * height * (ch->iifc_bpp4 ? 4 : 8), 32) >> 5;
    if (ch->iifc_words != nullptr) delete[] ch->iifc_words;
    ch->iifc_words_ptr = 0;
    ch->iifc_words_left = wordCount;
    ch->iifc_words = new Bit32u[wordCount];
  } else if (method >= 0x100 && method < 0x800) {
    ch->iifc_words[ch->iifc_words_ptr++] = param;
    ch->iifc_words_left--;
    if (!ch->iifc_words_left) {
      iifc(ch);
      delete[] ch->iifc_words;
      ch->iifc_words = nullptr;
    }
  }
}

void bx_nvriva_c::execute_sifc(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x0bf)
    ch->sifc_operation = param;
  else if (method == 0x0c0) {
    ch->sifc_color_fmt = param;
    update_color_bytes_sifc(ch);
  } else if (method == 0x0c1)
    ch->sifc_shw = param;
  else if (method == 0x0c2)
    ch->sifc_dxds = param;
  else if (method == 0x0c3)
    ch->sifc_dydt = param;
  else if (method == 0x0c4)
    ch->sifc_clip_yx = param;
  else if (method == 0x0c5)
    ch->sifc_clip_hw = param;
  else if (method == 0x0c6) {
    ch->sifc_syx = param;
    Bit32u width = ch->sifc_shw & 0xFFFF;
    Bit32u height = ch->sifc_shw >> 16;
    Bit32u wordCount = ALIGN(width * height * ch->sifc_color_bytes, 4) >> 2;
    if (ch->sifc_words != nullptr) delete[] ch->sifc_words;
    ch->sifc_words_ptr = 0;
    ch->sifc_words_left = wordCount;
    ch->sifc_words = new Bit32u[wordCount];
  } else if (method >= 0x100 && method < 0x800) {
    ch->sifc_words[ch->sifc_words_ptr++] = param;
    ch->sifc_words_left--;
    if (!ch->sifc_words_left) {
      sifc(ch);
      delete[] ch->sifc_words;
      ch->sifc_words = nullptr;
    }
  }
}

void bx_nvriva_c::execute_tfc(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x061) {
    Bit8u cls8 = ramin_read32(param);
    ch->tfc_swizzled = cls8 == 0x52;
  } else if (method == 0x0c0) {
    ch->tfc_color_fmt = param;
    update_color_bytes_tfc(ch);
  } else if (method == 0x0c1)
    ch->tfc_yx = param;
  else if (method == 0x0c2) {
    ch->tfc_hw = param;
    ch->tfc_upload = param == 0x01000100 && ch->tfc_yx == 0 &&
      ch->tfc_color_fmt == 4 && ch->s2d_color_fmt == 0xA &&
      ch->s2d_pitch_src == 0x0400 && ch->s2d_pitch_dst == 0x0400;
    if (ch->tfc_upload) {
      ch->tfc_upload_offset = ch->s2d_ofs_dst;
    } else {
      Bit32u width = ch->tfc_hw & 0xFFFF;
      Bit32u height = ch->tfc_hw >> 16;
      Bit32u wordCount = ALIGN(width * height * ch->tfc_color_bytes, 4) >> 2;
      if (ch->tfc_words != nullptr) delete[] ch->tfc_words;
      ch->tfc_words_ptr = 0;
      ch->tfc_words_left = wordCount;
      ch->tfc_words = new Bit32u[wordCount];
    }
  } else if (method == 0x0c3)
    ch->tfc_clip_wx = param;
  else if (method == 0x0c4)
    ch->tfc_clip_hy = param;
  else if (method >= 0x100 && method < 0x800) {
    if (ch->tfc_upload) {
      dma_write32(ch->s2d_img_dst, ch->tfc_upload_offset, param);
      ch->tfc_upload_offset += 4;
    } else if (ch->tfc_words != nullptr) {
      ch->tfc_words[ch->tfc_words_ptr++] = param;
      ch->tfc_words_left--;
      if (!ch->tfc_words_left) {
        tfc(ch);
        delete[] ch->tfc_words;
        ch->tfc_words = nullptr;
      }
    }
  }
}

void bx_nvriva_c::execute_sifm(nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param)
{
  if (method == 0x061)
    ch->sifm_src = param;
  else if (method == 0x066) {
    Bit8u surf_cls8 = ramin_read32(param);
    ch->sifm_swizzled = surf_cls8 == 0x52;
  } else if (method == 0x0c0) {
    ch->sifm_color_fmt = param;
    if (ch->sifm_color_fmt == 8)
      ch->sifm_color_bytes = 1;
    else if (ch->sifm_color_fmt == 1 || ch->sifm_color_fmt == 2 || ch->sifm_color_fmt == 7)
      ch->sifm_color_bytes = 2;
    else if (ch->sifm_color_fmt == 3 || ch->sifm_color_fmt == 4)
      ch->sifm_color_bytes = 4;
    else
      BX_ERROR(("unknown sifm color format: 0x%02x", ch->sifm_color_fmt));
  } else if (method == 0x0c1)
    ch->sifm_operation = param;
  else if (method == 0x0c4)
    ch->sifm_dyx = param;
  else if (method == 0x0c5)
    ch->sifm_dhw = param;
  else if (method == 0x0c6)
    ch->sifm_dudx = param;
  else if (method == 0x0c7)
    ch->sifm_dvdy = param;
  else if (method == 0x100)
    ch->sifm_shw = param;
  else if (method == 0x101)
    ch->sifm_sfmt = param;
  else if (method == 0x102)
    ch->sifm_sofs = param;
  else if (method == 0x103) {
    ch->sifm_syx = param;
    sifm(ch, ch->sifm_swizzled);
  }
}

void bx_nvriva_c::execute_m2mf(nv4_channel* ch, Bit32u subc, Bit32u method, Bit32u param)
{
  if (method == 0x061)
    ch->m2mf_src = param;
  else if (method == 0x062)
    ch->m2mf_dst = param;
  else if (method == 0x0c3)
    ch->m2mf_src_offset = param;
  else if (method == 0x0c4)
    ch->m2mf_dst_offset = param;
  else if (method == 0x0c5)
    ch->m2mf_src_pitch = param;
  else if (method == 0x0c6)
    ch->m2mf_dst_pitch = param;
  else if (method == 0x0c7)
    ch->m2mf_line_length = param;
  else if (method == 0x0c8)
    ch->m2mf_line_count = param;
  else if (method == 0x0c9)
    ch->m2mf_format = param;
  else if (method == 0x0ca) {
    ch->m2mf_buffer_notify = param;
    m2mf(ch);
    if ((ramin_read32(ch->schs[subc].notifier) & 0xFF) == 0x30) {
      BX_DEBUG(("M2MF notify skipped"));
    } else {
      dma_write32(ch->schs[subc].notifier, 0x10 + 0x8, 0);
      dma_write32(ch->schs[subc].notifier, 0x10 + 0xC, 0);
    }
  }
}

void bx_nvriva_c::execute_gdi(nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param)
{
  if (method == 0x0bf)
    ch->gdi_operation = param;
  else if (method == 0x0c0)
    ch->gdi_color_fmt = param;
  else if (method == 0x0c1)
    ch->gdi_mono_fmt = param;
  else if (method == 0x0ff)
    ch->gdi_rect_color = param;
  else if (method >= 0x100 && method < 0x140) {
    if (method & 1) {
      ch->gdi_rect_wh = param;
      gdi_fillrect(ch, false);
    } else
      ch->gdi_rect_xy = param;
  } else if (method == 0x17d)
    ch->gdi_clip_yx0 = param;
  else if (method == 0x17e)
    ch->gdi_clip_yx1 = param;
  else if (method == 0x17f)
    ch->gdi_rect_color = param;
  else if (method >= 0x180 && method < 0x1c0) {
    if (method & 1) {
      ch->gdi_rect_yx1 = param;
      gdi_fillrect(ch, true);
    } else
      ch->gdi_rect_yx0 = param;
  } else if ((method == 0x1fb && cls == 0x004a) || (method == 0x2fb && cls == 0x004b))
    ch->gdi_clip_yx0 = param;
  else if ((method == 0x1fc && cls == 0x004a) || (method == 0x2fc && cls == 0x004b))
    ch->gdi_clip_yx1 = param;
  else if ((method == 0x1fd && cls == 0x004a) || (method == 0x2fd && cls == 0x004b))
    ch->gdi_fg_color = param;
  else if ((method == 0x1fe && cls == 0x004a) || (method == 0x2fe && cls == 0x004b))
    ch->gdi_image_swh = param;
  else if ((method == 0x1ff && cls == 0x004a) || (method == 0x2ff && cls == 0x004b)) {
    ch->gdi_image_xy = param;
    Bit32u width = ch->gdi_image_swh & 0xFFFF;
    Bit32u height = ch->gdi_image_swh >> 16;
    Bit32u wordCount = ALIGN(width * height, 32) >> 5;
    if (ch->gdi_words != nullptr) delete[] ch->gdi_words;
    ch->gdi_words_ptr = 0;
    ch->gdi_words_left = wordCount;
    ch->gdi_words = new Bit32u[wordCount];
  } else if ((method >= 0x200 && method < 0x280 && cls == 0x004a) ||
             (method >= 0x300 && method < 0x380 && cls == 0x004b)) {
    ch->gdi_words[ch->gdi_words_ptr++] = param;
    ch->gdi_words_left--;
    if (!ch->gdi_words_left) {
      gdi_blit(ch, 0);
      delete[] ch->gdi_words;
      ch->gdi_words = nullptr;
    }
  } else if ((method == 0x2f9 && cls == 0x004a) || (method == 0x4f9 && cls == 0x004b))
    ch->gdi_clip_yx0 = param;
  else if ((method == 0x2fa && cls == 0x004a) || (method == 0x4fa && cls == 0x004b))
    ch->gdi_clip_yx1 = param;
  else if ((method == 0x2fb && cls == 0x004a) || (method == 0x4fb && cls == 0x004b))
    ch->gdi_bg_color = param;
  else if ((method == 0x2fc && cls == 0x004a) || (method == 0x4fc && cls == 0x004b))
    ch->gdi_fg_color = param;
  else if ((method == 0x2fd && cls == 0x004a) || (method == 0x4fd && cls == 0x004b))
    ch->gdi_image_swh = param;
  else if ((method == 0x2fe && cls == 0x004a) || (method == 0x4fe && cls == 0x004b))
    ch->gdi_image_dwh = param;
  else if ((method == 0x2ff && cls == 0x004a) || (method == 0x4ff && cls == 0x004b)) {
    ch->gdi_image_xy = param;
    Bit32u width = ch->gdi_image_swh & 0xFFFF;
    Bit32u height = ch->gdi_image_swh >> 16;
    Bit32u wordCount = ALIGN(width * height, 32) >> 5;
    if (ch->gdi_words != nullptr) delete[] ch->gdi_words;
    ch->gdi_words_ptr = 0;
    ch->gdi_words_left = wordCount;
    ch->gdi_words = new Bit32u[wordCount];
  } else if ((method >= 0x300 && method < 0x380 && cls == 0x004a) ||
             (method >= 0x500 && method < 0x580 && cls == 0x004b)) {
    ch->gdi_words[ch->gdi_words_ptr++] = param;
    ch->gdi_words_left--;
    if (!ch->gdi_words_left) {
      gdi_blit(ch, 1);
      delete[] ch->gdi_words;
      ch->gdi_words = nullptr;
    }
  } else if (method == 0x3fd)
    ch->gdi_clip_yx0 = param;
  else if (method == 0x3fe)
    ch->gdi_clip_yx1 = param;
  else if (method == 0x3ff)
    ch->gdi_fg_color = param;
}

// ========== 3D Surface ==========

void bx_nvriva_c::execute_surf3d(nv4_channel* ch, Bit32u method, Bit32u param)
{
  if (method == 0x061)
    ch->d3d_surf_obj = param;
  else if (method == 0x0bf) {
    ch->d3d_surface_clip_horizontal = param;
  } else if (method == 0x0c0) {
    ch->d3d_surface_clip_vertical = param;
  } else if (method == 0x0c1) {
    ch->d3d_surface_format = param;
    Bit32u color_fmt = param & 0xFF;
    if (color_fmt <= 3) ch->d3d_surface_color_bytes = 2;
    else ch->d3d_surface_color_bytes = 4;
    Bit32u type = (param >> 8) & 0xFF;
    Bit32u base_u = (param >> 16) & 0xFF;
    Bit32u base_v = (param >> 24) & 0xFF;
  } else if (method == 0x0c2) {
    ch->d3d_surface_clip_size = param;
  } else if (method == 0x0c3) {
    ch->d3d_surface_pitch = param;
  } else if (method == 0x0c4) {
    ch->d3d_surface_color_offset = param;
  } else if (method == 0x0c5) {
    ch->d3d_surface_zeta_offset = param;
  }
}

// ========== NV4 3D Texture Sampling ==========

void bx_nvriva_c::d3d_texture_sample(nv4_channel* ch, nv4_texture* tex,
  float u, float v, float color_out[4])
{
  if (!tex->enabled || !tex->base_size_u || !tex->base_size_v) {
    color_out[0] = 1.0f;
    color_out[1] = 1.0f;
    color_out[2] = 1.0f;
    color_out[3] = 1.0f;
    return;
  }

  Bit32u tw = 1 << tex->base_size_u;
  Bit32u th = 1 << tex->base_size_v;

  float su = u;
  float sv = v;
  if (su < 0.0f || su > 1.0f) {
    switch (tex->wrap_u) {
      case 1: su = su - floorf(su); break;
      case 2: su = fmodf(su, 2.0f); if (su < 0) su += 2.0f; if (su > 1.0f) su = 2.0f - su; break;
      default: su = su < 0.0f ? 0.0f : 1.0f; break;
    }
  }
  if (sv < 0.0f || sv > 1.0f) {
    switch (tex->wrap_v) {
      case 1: sv = sv - floorf(sv); break;
      case 2: sv = fmodf(sv, 2.0f); if (sv < 0) sv += 2.0f; if (sv > 1.0f) sv = 2.0f - sv; break;
      default: sv = sv < 0.0f ? 0.0f : 1.0f; break;
    }
  }

  Bit32u tx = (su >= 1.0f) ? tw - 1 : (Bit32u)(su * tw);
  Bit32u ty = (sv >= 1.0f) ? th - 1 : (Bit32u)(sv * th);

  Bit32u dma_obj = tex->dma_a ? ch->d3d_a_obj : ch->d3d_b_obj;
  Bit32u texel_ofs = tex->offset +
    nv4_swizzle(tx, ty, 0, tw, th, 1) * (tex->color_fmt <= 5 ? 2 : 4);

  switch (tex->color_fmt) {
    case 1: { // Y8
      Bit8u val = dma_read8(dma_obj, texel_ofs);
      color_out[0] = 1.0f;
      color_out[1] = val / 255.0f;
      color_out[2] = val / 255.0f;
      color_out[3] = val / 255.0f;
      break;
    }
    case 2: { // A1R5G5B5
      Bit16u val = dma_read16(dma_obj, texel_ofs);
      color_out[0] = (val >> 15) & 1 ? 1.0f : 0.0f;
      color_out[1] = ((val >> 10) & 0x1F) / 31.0f;
      color_out[2] = ((val >> 5) & 0x1F) / 31.0f;
      color_out[3] = (val & 0x1F) / 31.0f;
      break;
    }
    case 3: { // X1R5G5B5
      Bit16u val = dma_read16(dma_obj, texel_ofs);
      color_out[0] = 1.0f;
      color_out[1] = ((val >> 10) & 0x1F) / 31.0f;
      color_out[2] = ((val >> 5) & 0x1F) / 31.0f;
      color_out[3] = (val & 0x1F) / 31.0f;
      break;
    }
    case 4: { // A4R4G4B4
      Bit16u val = dma_read16(dma_obj, texel_ofs);
      color_out[0] = ((val >> 12) & 0xF) / 15.0f;
      color_out[1] = ((val >> 8) & 0xF) / 15.0f;
      color_out[2] = ((val >> 4) & 0xF) / 15.0f;
      color_out[3] = (val & 0xF) / 15.0f;
      break;
    }
    case 5: { // R5G6B5
      Bit16u val = dma_read16(dma_obj, texel_ofs);
      color_out[0] = 1.0f;
      color_out[1] = ((val >> 11) & 0x1F) / 31.0f;
      color_out[2] = ((val >> 5) & 0x3F) / 63.0f;
      color_out[3] = (val & 0x1F) / 31.0f;
      break;
    }
    case 6: { // A8R8G8B8
      Bit32u val = dma_read32(dma_obj, texel_ofs);
      color_out[0] = ((val >> 24) & 0xFF) / 255.0f;
      color_out[1] = ((val >> 16) & 0xFF) / 255.0f;
      color_out[2] = ((val >> 8) & 0xFF) / 255.0f;
      color_out[3] = (val & 0xFF) / 255.0f;
      break;
    }
    case 7: { // X8R8G8B8
      Bit32u val = dma_read32(dma_obj, texel_ofs);
      color_out[0] = 1.0f;
      color_out[1] = ((val >> 16) & 0xFF) / 255.0f;
      color_out[2] = ((val >> 8) & 0xFF) / 255.0f;
      color_out[3] = (val & 0xFF) / 255.0f;
      break;
    }
    default:
      color_out[0] = 1.0f;
      color_out[1] = 1.0f;
      color_out[2] = 0.0f;
      color_out[3] = 1.0f;
      break;
  }
}

// ========== NV4 3D Triangle Rasterizer ==========

void bx_nvriva_c::d3d_rasterize_triangle(nv4_channel* ch,
  float v0[8], float v1[8], float v2[8], bool multitex)
{
  float pos0[2] = { v0[0], v0[1] };
  float pos1[2] = { v1[0], v1[1] };
  float pos2[2] = { v2[0], v2[1] };

  double area = nv4_edge_function(pos0, pos1, pos2);
  if (area == 0.0) return;

  Bit32u control0 = multitex ? ch->d3d_control0 : ch->d3d_blend >> 20 & 0xFFF;
  Bit32u blend_reg = multitex ? ch->d3d_blend : ch->d3d_blend;
  bool z_enable = (control0 >> 14) & 1;
  Bit32u z_func = (control0 >> 16) & 0xF;
  bool z_write = (control0 >> 24) & 1;
  bool alpha_enable = (control0 >> 12) & 1;
  Bit32u alpha_func = (control0 >> 8) & 0xF;
  Bit32u alpha_ref = control0 & 0xFF;
  bool blend_enable = (blend_reg >> 20) & 1;
  Bit32u blend_src = (blend_reg >> 24) & 0xF;
  Bit32u blend_dst = (blend_reg >> 28) & 0xF;
  Bit32u cull_mode = (control0 >> 20) & 3;

  if (cull_mode == 2 && area > 0) return;
  if (cull_mode == 3 && area < 0) return;
  if (area < 0) area = -area;
  double inv_area = 1.0 / area;

  Bit32s minx = (Bit32s)BX_MIN(BX_MIN(v0[0], v1[0]), v2[0]);
  Bit32s maxx = (Bit32s)ceilf(BX_MAX(BX_MAX(v0[0], v1[0]), v2[0]));
  Bit32s miny = (Bit32s)BX_MIN(BX_MIN(v0[1], v1[1]), v2[1]);
  Bit32s maxy = (Bit32s)ceilf(BX_MAX(BX_MAX(v0[1], v1[1]), v2[1]));

  if (minx < 0) minx = 0;
  if (miny < 0) miny = 0;

  Bit32u color_pitch = ch->d3d_surface_pitch & 0xFFFF;
  Bit32u zeta_pitch = ch->d3d_surface_pitch >> 16;
  Bit32u color_offset = ch->d3d_surface_color_offset;
  Bit32u zeta_offset = ch->d3d_surface_zeta_offset;
  Bit32u color_obj = ch->d3d_surf_obj;

  Bit32u tex_map_mode = blend_reg & 0xF;

  for (Bit32s py = miny; py < maxy; py++) {
    for (Bit32s px = minx; px < maxx; px++) {
      float p[2] = { px + 0.5f, py + 0.5f };

      double w0 = nv4_edge_function(pos1, pos2, p);
      double w1 = nv4_edge_function(pos2, pos0, p);
      double w2 = nv4_edge_function(pos0, pos1, p);

      bool inside;
      if (area > 0)
        inside = w0 >= 0 && w1 >= 0 && w2 >= 0;
      else
        inside = w0 <= 0 && w1 <= 0 && w2 <= 0;

      if (!inside) continue;

      float b0 = (float)(w0 * inv_area);
      float b1 = (float)(w1 * inv_area);
      float b2 = (float)(w2 * inv_area);

      float z = b0 * v0[2] + b1 * v1[2] + b2 * v2[2];
      float rhw = b0 * v0[3] + b1 * v1[3] + b2 * v2[3];

      if (z_enable) {
        Bit32u fb_z;
        if (ch->d3d_surface_color_bytes == 2) {
          fb_z = dma_read16(color_obj, zeta_offset + py * zeta_pitch + px * 2);
        } else {
          fb_z = dma_read32(color_obj, zeta_offset + py * zeta_pitch + px * 4) >> 8;
        }
        Bit32u z_int = (Bit32u)(z * 0xFFFF);
        bool z_pass = false;
        switch (z_func) {
          case 1: z_pass = false; break;
          case 2: z_pass = z_int < fb_z; break;
          case 3: z_pass = z_int == fb_z; break;
          case 4: z_pass = z_int <= fb_z; break;
          case 5: z_pass = z_int > fb_z; break;
          case 6: z_pass = z_int != fb_z; break;
          case 7: z_pass = z_int >= fb_z; break;
          case 8: z_pass = true; break;
        }
        if (!z_pass) continue;

        if (z_write) {
          if (ch->d3d_surface_color_bytes == 2)
            dma_write16(color_obj, zeta_offset + py * zeta_pitch + px * 2, z_int);
          else
            dma_write32(color_obj, zeta_offset + py * zeta_pitch + px * 4, z_int << 8);
        }
      }

      float r = b0 * v0[4] + b1 * v1[4] + b2 * v2[4];
      float g = b0 * v0[5] + b1 * v1[5] + b2 * v2[5];
      float b = b0 * v0[6] + b1 * v1[6] + b2 * v2[6];
      float a = b0 * v0[7] + b1 * v1[7] + b2 * v2[7];

      if (r < 0) r = 0; if (r > 1) r = 1;
      if (g < 0) g = 0; if (g > 1) g = 1;
      if (b < 0) b = 0; if (b > 1) b = 1;
      if (a < 0) a = 0; if (a > 1) a = 1;

      if (ch->d3d_tex[0].enabled && rhw > 0) {
        float inv_w = 1.0f / rhw;
        float tu = (b0 * v0[3] * 0 + b1 * v1[3] * 0 + b2 * v2[3] * 0) * inv_w;
        float tv = tu;
        float tex_color[4];
        d3d_texture_sample(ch, &ch->d3d_tex[0], tu, tv, tex_color);

        switch (tex_map_mode) {
          case 1: // DECAL
            r = tex_color[1]; g = tex_color[2]; b = tex_color[3];
            break;
          case 2: // MODULATE
            r *= tex_color[1]; g *= tex_color[2]; b *= tex_color[3];
            break;
          case 3: // DECALALPHA
            r = tex_color[0] * tex_color[1] + (1 - tex_color[0]) * r;
            g = tex_color[0] * tex_color[2] + (1 - tex_color[0]) * g;
            b = tex_color[0] * tex_color[3] + (1 - tex_color[0]) * b;
            break;
          case 4: // MODULATEALPHA
            r *= tex_color[1]; g *= tex_color[2]; b *= tex_color[3];
            a *= tex_color[0];
            break;
          default:
            break;
        }
      }

      if (alpha_enable) {
        Bit32u a_int = (Bit32u)(a * 255.0f);
        bool a_pass = false;
        switch (alpha_func) {
          case 1: a_pass = false; break;
          case 2: a_pass = a_int < alpha_ref; break;
          case 3: a_pass = a_int == alpha_ref; break;
          case 4: a_pass = a_int <= alpha_ref; break;
          case 5: a_pass = a_int > alpha_ref; break;
          case 6: a_pass = a_int != alpha_ref; break;
          case 7: a_pass = a_int >= alpha_ref; break;
          case 8: a_pass = true; break;
        }
        if (!a_pass) continue;
      }

      Bit32u final_color;
      if (ch->d3d_surface_color_bytes == 2) {
        if (blend_enable) {
          Bit16u fb_color = dma_read16(color_obj, color_offset + py * color_pitch + px * 2);
          Bit8u dr, dg, db;
          EXTRACT_565_TO_888(fb_color, dr, dg, db);
          float dfr = dr / 255.0f, dfg = dg / 255.0f, dfb = db / 255.0f;
          float sf = 0, df = 0;
          switch (blend_src) {
            case 1: sf = 0; break; case 2: sf = 1; break;
            case 5: sf = a; break; case 6: sf = 1 - a; break;
            default: sf = 1; break;
          }
          switch (blend_dst) {
            case 1: df = 0; break; case 2: df = 1; break;
            case 5: df = a; break; case 6: df = 1 - a; break;
            default: df = 0; break;
          }
          r = r * sf + dfr * df; g = g * sf + dfg * df; b = b * sf + dfb * df;
          if (r > 1) r = 1; if (g > 1) g = 1; if (b > 1) b = 1;
        }
        final_color = ((Bit32u)(r * 31) << 11) | ((Bit32u)(g * 63) << 5) | (Bit32u)(b * 31);
        dma_write16(color_obj, color_offset + py * color_pitch + px * 2, final_color);
      } else {
        if (blend_enable) {
          Bit32u fb_color = dma_read32(color_obj, color_offset + py * color_pitch + px * 4);
          float dfb = (fb_color & 0xFF) / 255.0f;
          float dfg = ((fb_color >> 8) & 0xFF) / 255.0f;
          float dfr = ((fb_color >> 16) & 0xFF) / 255.0f;
          float sf = 0, df = 0;
          switch (blend_src) {
            case 1: sf = 0; break; case 2: sf = 1; break;
            case 5: sf = a; break; case 6: sf = 1 - a; break;
            default: sf = 1; break;
          }
          switch (blend_dst) {
            case 1: df = 0; break; case 2: df = 1; break;
            case 5: df = a; break; case 6: df = 1 - a; break;
            default: df = 0; break;
          }
          r = r * sf + dfr * df; g = g * sf + dfg * df; b = b * sf + dfb * df;
          if (r > 1) r = 1; if (g > 1) g = 1; if (b > 1) b = 1;
        }
        final_color = (Bit32u)(b * 255) | ((Bit32u)(g * 255) << 8) |
          ((Bit32u)(r * 255) << 16) | ((Bit32u)(a * 255) << 24);
        dma_write32(color_obj, color_offset + py * color_pitch + px * 4, final_color);
      }
    }
  }

  Bit32u redraw_offset = dma_lin_lookup(color_obj,
    color_offset + miny * color_pitch + minx * ch->d3d_surface_color_bytes) -
    BX_NVRIVA_THIS disp_offset;
  BX_NVRIVA_THIS redraw_area_nd(redraw_offset, maxx - minx, maxy - miny);
}

void bx_nvriva_c::d3d_draw_primitive(nv4_channel* ch, Bit32u prim_word, bool multitex)
{
  for (int i = 0; i < 6; i += 3) {
    Bit32u i0 = (prim_word >> (i * 4)) & 0xF;
    Bit32u i1 = (prim_word >> ((i + 1) * 4)) & 0xF;
    Bit32u i2 = (prim_word >> ((i + 2) * 4)) & 0xF;

    Bit32u max_vtx = multitex ? 8 : 16;
    if (i0 >= max_vtx || i1 >= max_vtx || i2 >= max_vtx) continue;
    if (i0 == i1 || i1 == i2 || i0 == i2) continue;

    float *src0, *src1, *src2;
    float va[8], vb[8], vc[8];

    if (multitex) {
      src0 = ch->d3d_tlmtvertex[i0];
      src1 = ch->d3d_tlmtvertex[i1];
      src2 = ch->d3d_tlmtvertex[i2];
    } else {
      src0 = ch->d3d_tlvertex[i0];
      src1 = ch->d3d_tlvertex[i1];
      src2 = ch->d3d_tlvertex[i2];
    }

    for (int j = 0; j < 4; j++) { va[j] = src0[j]; vb[j] = src1[j]; vc[j] = src2[j]; }

    Bit32u c0 = *(Bit32u*)&src0[4];
    va[4] = ((c0 >> 16) & 0xFF) / 255.0f;
    va[5] = ((c0 >> 8) & 0xFF) / 255.0f;
    va[6] = (c0 & 0xFF) / 255.0f;
    va[7] = ((c0 >> 24) & 0xFF) / 255.0f;

    Bit32u c1 = *(Bit32u*)&src1[4];
    vb[4] = ((c1 >> 16) & 0xFF) / 255.0f;
    vb[5] = ((c1 >> 8) & 0xFF) / 255.0f;
    vb[6] = (c1 & 0xFF) / 255.0f;
    vb[7] = ((c1 >> 24) & 0xFF) / 255.0f;

    Bit32u c2 = *(Bit32u*)&src2[4];
    vc[4] = ((c2 >> 16) & 0xFF) / 255.0f;
    vc[5] = ((c2 >> 8) & 0xFF) / 255.0f;
    vc[6] = (c2 & 0xFF) / 255.0f;
    vc[7] = ((c2 >> 24) & 0xFF) / 255.0f;

    d3d_rasterize_triangle(ch, va, vb, vc, multitex);
  }
}

// ========== NV4 3D method dispatch ==========

void bx_nvriva_c::execute_tex_tri(nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param)
{
  if (method == 0x061) ch->d3d_a_obj = param;
  else if (method == 0x062) ch->d3d_b_obj = param;
  else if (method == 0x063) ch->d3d_surf_obj = param;
  else if (method == 0x0c0) ch->d3d_tex[0].color_key = param;
  else if (method == 0x0c1) ch->d3d_tex[0].offset = param;
  else if (method == 0x0c2) {
    Bit32u fmt = param;
    ch->d3d_tex[0].format = fmt;
    ch->d3d_tex[0].dma_a = fmt & 1;
    ch->d3d_tex[0].dma_b = (fmt >> 1) & 1;
    ch->d3d_tex[0].color_key_enable = (fmt >> 2) & 1;
    ch->d3d_tex[0].color_fmt = (fmt >> 8) & 0xF;
    ch->d3d_tex[0].mipmap_levels = (fmt >> 12) & 0xF;
    ch->d3d_tex[0].base_size_u = (fmt >> 16) & 0xF;
    ch->d3d_tex[0].base_size_v = (fmt >> 20) & 0xF;
    ch->d3d_tex[0].wrap_u = (fmt >> 24) & 0xF;
    ch->d3d_tex[0].wrap_v = (fmt >> 28) & 0xF;
    ch->d3d_tex[0].enabled = true;
  } else if (method == 0x0c3) {
    ch->d3d_tex[0].filter = param;
    ch->d3d_tex[0].min_filter = (param >> 24) & 0xF;
    ch->d3d_tex[0].mag_filter = (param >> 28) & 0xF;
  } else if (method == 0x0c4) {
    ch->d3d_blend = param;
  } else if (method == 0x0c5) {
    ch->d3d_control0 = param;
  } else if (method == 0x0c6) {
    ch->d3d_fog_color = param;
  } else if (method >= 0x100 && method < 0x200) {
    Bit32u vtx_idx = (method - 0x100) / 8;
    Bit32u comp = (method - 0x100) % 8;
    if (vtx_idx < 16 && comp < 8)
      ch->d3d_tlvertex[vtx_idx][comp] = nv4_uint32_as_float(param);
  } else if (method >= 0x180 && method < 0x200) {
    d3d_draw_primitive(ch, param, false);
  }
}

void bx_nvriva_c::execute_mtex_tri(nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param)
{
  if (method == 0x061) ch->d3d_a_obj = param;
  else if (method == 0x062) ch->d3d_b_obj = param;
  else if (method == 0x063) ch->d3d_surf_obj = param;
  else if (method >= 0x0c2 && method <= 0x0c3) {
    Bit32u idx = method - 0x0c2;
    ch->d3d_tex[idx].offset = param;
  } else if (method >= 0x0c4 && method <= 0x0c5) {
    Bit32u idx = method - 0x0c4;
    Bit32u fmt = param;
    ch->d3d_tex[idx].format = fmt;
    ch->d3d_tex[idx].dma_a = fmt & 1;
    ch->d3d_tex[idx].dma_b = (fmt >> 1) & 1;
    ch->d3d_tex[idx].color_fmt = (fmt >> 8) & 0xF;
    ch->d3d_tex[idx].mipmap_levels = (fmt >> 12) & 0xF;
    ch->d3d_tex[idx].base_size_u = (fmt >> 16) & 0xF;
    ch->d3d_tex[idx].base_size_v = (fmt >> 20) & 0xF;
    ch->d3d_tex[idx].wrap_u = (fmt >> 24) & 0xF;
    ch->d3d_tex[idx].wrap_v = (fmt >> 28) & 0xF;
    ch->d3d_tex[idx].enabled = true;
  } else if (method >= 0x0c6 && method <= 0x0c7) {
    Bit32u idx = method - 0x0c6;
    ch->d3d_tex[idx].filter = param;
    ch->d3d_tex[idx].min_filter = (param >> 24) & 0xF;
    ch->d3d_tex[idx].mag_filter = (param >> 28) & 0xF;
  } else if (method >= 0x0c8 && method <= 0x0cd) {
    Bit32u sub = method - 0x0c8;
    if (sub == 0) ch->d3d_combine_alpha[0] = param;
    else if (sub == 1) ch->d3d_combine_color[0] = param;
    else if (sub == 3) ch->d3d_combine_alpha[1] = param;
    else if (sub == 4) ch->d3d_combine_color[1] = param;
    else if (sub == 5) ch->d3d_combine_factor = param;
  } else if (method == 0x0ce) {
    ch->d3d_blend = param;
  } else if (method == 0x0cf) {
    ch->d3d_control0 = param;
  } else if (method == 0x0d0) {
    ch->d3d_control1 = param;
  } else if (method == 0x0d1) {
    ch->d3d_control2 = param;
  } else if (method == 0x0d2) {
    ch->d3d_fog_color = param;
  } else if (method >= 0x100 && method < 0x1a0) {
    Bit32u vtx_idx = (method - 0x100) / 0x0a;
    Bit32u comp = (method - 0x100) % 0x0a;
    if (vtx_idx < 8 && comp < 10)
      ch->d3d_tlmtvertex[vtx_idx][comp] = nv4_uint32_as_float(param);
  } else if (method >= 0x150 && method < 0x1c0) {
    d3d_draw_primitive(ch, param, true);
  }
}

// ========== Empty method handler ==========

void bx_nvriva_c::empty_method_handler(nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param)
{
  BX_DEBUG(("empty_method_handler: cls 0x%04x, method 0x%03x, param 0x%08x", cls, method, param));
}

// ========== Init method handlers ==========

void bx_nvriva_c::init_method_handlers()
{
  for (int i = 0; i < RIVA_METHOD_COUNT; i++) {
    BX_NVRIVA_THIS empty_method_handlers[i] = &bx_nvriva_c::empty_method_handler;
    BX_NVRIVA_THIS cl0054_method_handlers[i] = &bx_nvriva_c::empty_method_handler;
    BX_NVRIVA_THIS cl0055_method_handlers[i] = &bx_nvriva_c::empty_method_handler;
  }
  for (int i = 0; i < RIVA_CLASS_COUNT; i++)
    BX_NVRIVA_THIS class_method_handlers[i] = BX_NVRIVA_THIS empty_method_handlers;
  BX_NVRIVA_THIS class_method_handlers[0x0054] = BX_NVRIVA_THIS cl0054_method_handlers;
  BX_NVRIVA_THIS class_method_handlers[0x0055] = BX_NVRIVA_THIS cl0055_method_handlers;
}

// ========== Execute command ==========

int bx_nvriva_c::execute_command(Bit32u chid, Bit32u subc, Bit32u method, Bit32u param)
{
  int result = 0;
  bool software_method = false;
  BX_DEBUG(("execute_command: chid 0x%02x, subc 0x%02x, method 0x%03x, param 0x%08x",
    chid, subc, method, param));
  nv4_channel* ch = &BX_NVRIVA_THIS chs[chid];
  if (method == 0x000) {
    if (ch->schs[subc].engine == 0x01) {
      Bit32u word1 = ramin_read32(ch->schs[subc].object + 0x4);
      word1 = (word1 & 0x0000FFFF) | (ch->schs[subc].notifier >> 4 << 16);
      Bit32u word0 = ramin_read32(ch->schs[subc].object);
      Bit8u cls8 = word0;
      if (cls8 == 0x4a || cls8 == 0x4b) {
        word0 = (word0 & 0xFFFC7FFF) | (ch->gdi_operation << 15);
        word1 = (word1 & 0xFFFFFFFC) | ch->gdi_mono_fmt;
        ramin_write32(ch->schs[subc].object, word0);
      } else if (cls8 == 0x42) {
        Bit32u srcdst = ramin_read32(ch->schs[subc].object + 0x8);
        ch->s2d_img_src = (srcdst & 0xFFFF) << 4;
        ch->s2d_img_dst = srcdst >> 16 << 4;
      } else if (cls8 == 0x60 || cls8 == 0x64) {
        ch->iifc_palette = ramin_read32(ch->schs[subc].object + 0x8) << 4;
        word0 = (word0 & 0xFFFC7FFF) | (ch->iifc_operation << 15);
        ramin_write32(ch->schs[subc].object, word0);
        word1 = (word1 & 0xFFFF00FF) | ((ch->iifc_color_fmt + 9) << 8);
      }
      ramin_write32(ch->schs[subc].object + 0x4, word1);
    }
    ramht_lookup(param, chid, &ch->schs[subc].object, &ch->schs[subc].engine);
    if (ch->schs[subc].engine == 0x01) {
      Bit32u word1 = ramin_read32(ch->schs[subc].object + 0x4);
      ch->schs[subc].notifier = word1 >> 16 << 4;
      Bit32u word0 = ramin_read32(ch->schs[subc].object);
      Bit8u cls8 = word0;
      if (cls8 == 0x48) {
        if (!ch->s2d_locked) {
          Bit32u srcdst = ramin_read32(ch->schs[subc].object + 0x8);
          ch->s2d_img_src = (srcdst & 0xFFFF) << 4;
          ch->s2d_img_dst = srcdst >> 16 << 4;
          ch->s2d_color_fmt = BX_NVRIVA_THIS graph_bpixel & 0xf;
          update_color_bytes_s2d(ch);
          ch->s2d_pitch_src = BX_NVRIVA_THIS graph_pitch0 & 0xffff;
          ch->s2d_pitch_dst = ch->s2d_pitch_src;
          ch->s2d_ofs_src = BX_NVRIVA_THIS graph_offset0;
          ch->s2d_ofs_dst = BX_NVRIVA_THIS graph_offset0;
        }
      } else if (cls8 == 0x4a || cls8 == 0x4b) {
        ch->gdi_operation = (word0 >> 15) & 7;
        ch->gdi_mono_fmt = word1 & 3;
      } else if (cls8 == 0x42) {
        Bit32u srcdst = ramin_read32(ch->schs[subc].object + 0x8);
        ch->s2d_img_src = (srcdst & 0xFFFF) << 4;
        ch->s2d_img_dst = srcdst >> 16 << 4;
      } else if (cls8 == 0x60 || cls8 == 0x64) {
        ch->iifc_palette = ramin_read32(ch->schs[subc].object + 0x8) << 4;
        ch->iifc_operation = (word0 >> 15) & 7;
        ch->iifc_color_fmt = (word1 >> 8 & 0xFF) - 9;
        update_color_bytes_iifc(ch);
      }
    } else if (ch->schs[subc].engine == 0x00) {
      software_method = true;
    }
  } else if (method == 0x014) {
    BX_NVRIVA_THIS fifo_cache1_ref_cnt = param;
  } else if (method >= 0x040) {
    if (ch->schs[subc].engine == 0x01) {
      if (method >= 0x060 && method < 0x080)
        ramht_lookup(param, chid, &param, nullptr);
      Bit32u cls = ramin_read32(ch->schs[subc].object) & 0xFF;
      BX_DEBUG(("execute_command: obj 0x%08x, class 0x%02x, method 0x%03x, param 0x%08x",
        ch->schs[subc].object, cls, method, param));
      switch (cls) {
        case 0x19: execute_clip(ch, method, param); break;
        case 0x39: execute_m2mf(ch, subc, method, param); break;
        case 0x43: execute_rop(ch, method, param); break;
        case 0x44: case 0x18: execute_patt(ch, method, param); break;
        case 0x4a: case 0x4b: execute_gdi(ch, cls, method, param); break;
        case 0x52: execute_swzsurf(ch, method, param); break;
        case 0x57: case 0x17: execute_chroma(ch, method, param); break;
        case 0x5e: case 0x1e: execute_rect(ch, method, param); break;
        case 0x5f: case 0x1f: execute_imageblit(ch, method, param); break;
        case 0x61: case 0x65: case 0x21: execute_ifc(ch, method, param); break;
        case 0x42: execute_surf2d(ch, method, param); break;
        case 0x64: case 0x60: execute_iifc(ch, method, param); break;
        case 0x66: case 0x76: case 0x36: execute_sifc(ch, method, param); break;
        case 0x12: case 0x72: execute_beta(ch, method, param); break;
        case 0x7b: execute_tfc(ch, method, param); break;
        case 0x77: case 0x37: case 0x89: execute_sifm(ch, cls, method, param); break;
        case 0x53: execute_surf3d(ch, method, param); break;
        case 0x54: execute_tex_tri(ch, cls, method, param); break;
        case 0x55: execute_mtex_tri(ch, cls, method, param); break;
      }
      if (ch->notify_pending) {
        ch->notify_pending = false;
        if ((ramin_read32(ch->schs[subc].notifier) & 0xFF) != 0x30) {
          dma_write32(ch->schs[subc].notifier, 0x8, 0);
          dma_write32(ch->schs[subc].notifier, 0xC, 0);
        }
        if (ch->notify_type) {
          BX_NVRIVA_THIS graph_intr |= 0x00000001;
          update_irq_level();
          BX_NVRIVA_THIS graph_nsource |= 0x00000001;
          BX_NVRIVA_THIS graph_notify = 0x00110000;
          Bit32u notifier = ch->schs[subc].notifier >> 4;
          BX_NVRIVA_THIS graph_ctx_switch2 = notifier << 16;
          BX_NVRIVA_THIS graph_ctx_switch4 = ch->schs[subc].object >> 4;
          BX_NVRIVA_THIS graph_trapped_addr = (method << 2) | (subc << 16) | (chid << 20);
          BX_NVRIVA_THIS graph_trapped_data = param;
          BX_NVRIVA_THIS fifo_wait_notify = true;
          BX_NVRIVA_THIS fifo_wait = true;
        }
      }
      if (method == 0x041) {
        ch->notify_pending = true;
        ch->notify_type = param;
      } else if (method == 0x060)
        ch->schs[subc].notifier = param;
    } else if (ch->schs[subc].engine == 0x00) {
      software_method = true;
    }
  }
  if (software_method) {
    BX_NVRIVA_THIS fifo_wait_soft = true;
    BX_NVRIVA_THIS fifo_wait = true;
    BX_NVRIVA_THIS fifo_intr |= 0x00000001;
    update_irq_level();
    BX_NVRIVA_THIS fifo_cache1_pull0 |= 0x00000100;
    BX_NVRIVA_THIS fifo_cache1_method[BX_NVRIVA_THIS fifo_cache1_put / 4] = (method << 2) | (subc << 13);
    BX_NVRIVA_THIS fifo_cache1_data[BX_NVRIVA_THIS fifo_cache1_put / 4] = param;
    BX_NVRIVA_THIS fifo_cache1_put += 4;
    if (BX_NVRIVA_THIS fifo_cache1_put == RIVA_CACHE1_SIZE * 4)
      BX_NVRIVA_THIS fifo_cache1_put = 0;
    result = 1;
  }
  return result;
}

// ========== PCI write handler ==========

void bx_nvriva_c::pci_write_handler(Bit8u address, Bit32u value, unsigned io_len)
{
  Bit8u value8, oldval;

  if ((address >= 0x1c) && (address < 0x2c))
    return;

  BX_DEBUG_PCI_WRITE(address, value, io_len);
  for (unsigned i=0; i<io_len; i++) {
    value8 = (value >> (i*8)) & 0xFF;
    oldval = pci_conf[address+i];
    switch (address + i) {
      case 0x06:
      case 0x07:
        value8 = oldval;
        break;
    }
    pci_conf[address+i] = value8;
  }
}

#if BX_DEBUGGER
void bx_nvriva_c::debug_dump(int argc, char **argv)
{
  bx_vgacore_c::debug_dump(argc, argv);
}
#endif

#endif // BX_SUPPORT_NVRIVA