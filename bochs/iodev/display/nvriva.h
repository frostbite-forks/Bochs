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

#if BX_SUPPORT_NVRIVA

#if BX_USE_NVRIVA_SMF
#  define BX_NVRIVA_SMF  static
#  define BX_NVRIVA_THIS theSvga->
#  define BX_NVRIVA_THIS_PTR theSvga
#else
#  define BX_NVRIVA_SMF
#  define BX_NVRIVA_THIS this->
#  define BX_NVRIVA_THIS_PTR this
#endif

#define VGA_CRTC_MAX 0x18
#define RIVA_CRTC_MAX 0xF0

#define RIVA_CHANNEL_COUNT 16
#define RIVA_SUBCHANNEL_COUNT 8
#define RIVA_CACHE1_SIZE 64

#define RIVA_CLASS_COUNT 0x100
#define RIVA_METHOD_COUNT 0x800

#define BX_ROP_PATTERN 0x01

struct nv4_texture
{
  Bit32u offset;
  Bit32u format;
  Bit32u filter;
  Bit32u color_key;
  Bit32u color_fmt;
  Bit32u mipmap_levels;
  Bit32u base_size_u;
  Bit32u base_size_v;
  Bit32u wrap_u;
  Bit32u wrap_v;
  Bit32u min_filter;
  Bit32u mag_filter;
  bool color_key_enable;
  bool dma_a;
  bool dma_b;
  bool enabled;
};

struct nv4_channel
{
  Bit32u subr_return;
  bool subr_active;
  struct {
    Bit32u mthd;
    Bit32u subc;
    Bit32u mcnt;
    bool ni;
  } dma_state;
  struct {
    Bit32u object;
    Bit8u engine;
    Bit32u notifier;
  } schs[RIVA_SUBCHANNEL_COUNT];

  bool notify_pending;
  Bit32u notify_type;

  bool s2d_locked;
  Bit32u s2d_img_src;
  Bit32u s2d_img_dst;
  Bit32u s2d_color_fmt;
  Bit32u s2d_color_bytes;
  Bit32u s2d_pitch_src;
  Bit32u s2d_pitch_dst;
  Bit32u s2d_ofs_src;
  Bit32u s2d_ofs_dst;

  Bit32u swzs_img_obj;
  Bit32u swzs_fmt;
  Bit32u swzs_color_bytes;
  Bit32u swzs_width;
  Bit32u swzs_height;
  Bit32u swzs_ofs;

  bool ifc_color_key_enable;
  bool ifc_clip_enable;
  Bit32u ifc_operation;
  Bit32u ifc_color_fmt;
  Bit32u ifc_color_bytes;
  Bit32u ifc_pixels_per_word;
  Bit32u ifc_x;
  Bit32u ifc_y;
  Bit32u ifc_ofs_x;
  Bit32u ifc_ofs_y;
  Bit32u ifc_draw_offset;
  Bit32u ifc_redraw_offset;
  Bit32u ifc_dst_width;
  Bit32u ifc_dst_height;
  Bit32u ifc_src_width;
  Bit32u ifc_src_height;
  Bit32u ifc_clip_x0;
  Bit32u ifc_clip_y0;
  Bit32u ifc_clip_x1;
  Bit32u ifc_clip_y1;

  Bit32u iifc_palette;
  Bit32u iifc_palette_ofs;
  Bit32u iifc_operation;
  Bit32u iifc_color_fmt;
  Bit32u iifc_color_bytes;
  Bit32u iifc_bpp4;
  Bit32u iifc_yx;
  Bit32u iifc_dhw;
  Bit32u iifc_shw;
  Bit32u iifc_words_ptr;
  Bit32u iifc_words_left;
  Bit32u* iifc_words;

  Bit32u sifc_operation;
  Bit32u sifc_color_fmt;
  Bit32u sifc_color_bytes;
  Bit32u sifc_shw;
  Bit32u sifc_dxds;
  Bit32u sifc_dydt;
  Bit32u sifc_clip_yx;
  Bit32u sifc_clip_hw;
  Bit32u sifc_syx;
  Bit32u sifc_words_ptr;
  Bit32u sifc_words_left;
  Bit32u* sifc_words;

  bool blit_color_key_enable;
  Bit32u blit_operation;
  Bit32u blit_syx;
  Bit32u blit_dyx;
  Bit32u blit_hw;

  bool tfc_swizzled;
  Bit32u tfc_color_fmt;
  Bit32u tfc_color_bytes;
  Bit32u tfc_yx;
  Bit32u tfc_hw;
  Bit32u tfc_clip_wx;
  Bit32u tfc_clip_hy;
  Bit32u tfc_words_ptr;
  Bit32u tfc_words_left;
  Bit32u* tfc_words;
  bool tfc_upload;
  Bit32u tfc_upload_offset;

  Bit32u sifm_src;
  bool sifm_swizzled;
  bool sifm_swizzled_0389;
  Bit32u sifm_operation;
  Bit32u sifm_color_fmt;
  Bit32u sifm_color_bytes;
  Bit32u sifm_syx;
  Bit32u sifm_dyx;
  Bit32u sifm_shw;
  Bit32u sifm_dhw;
  Bit32s sifm_dudx;
  Bit32s sifm_dvdy;
  Bit32u sifm_sfmt;
  Bit32u sifm_sofs;

  Bit32u m2mf_src;
  Bit32u m2mf_dst;
  Bit32u m2mf_src_offset;
  Bit32u m2mf_dst_offset;
  Bit32u m2mf_src_pitch;
  Bit32u m2mf_dst_pitch;
  Bit32u m2mf_line_length;
  Bit32u m2mf_line_count;
  Bit32u m2mf_format;
  Bit32u m2mf_buffer_notify;

  Bit32u d3d_a_obj;
  Bit32u d3d_b_obj;
  Bit32u d3d_surf_obj;
  Bit32u d3d_color_key;

  nv4_texture d3d_tex[2];

  Bit32u d3d_blend;
  Bit32u d3d_control0;
  Bit32u d3d_control1;
  Bit32u d3d_control2;
  Bit32u d3d_fog_color;

  Bit32u d3d_combine_alpha[2];
  Bit32u d3d_combine_color[2];
  Bit32u d3d_combine_factor;

  Bit32u d3d_surface_clip_horizontal;
  Bit32u d3d_surface_clip_vertical;
  Bit32u d3d_surface_format;
  Bit32u d3d_surface_color_bytes;
  Bit32u d3d_surface_depth_bytes;
  Bit32u d3d_surface_pitch;
  Bit32u d3d_surface_color_offset;
  Bit32u d3d_surface_zeta_offset;
  Bit32u d3d_surface_clip_size;

  float d3d_tlvertex[16][8];
  float d3d_tlmtvertex[8][10];
  Bit32u d3d_vertex_count;

  Bit8u  rop;

  Bit32u beta;

  Bit16u clip_x;
  Bit16u clip_y;
  Bit16u clip_width;
  Bit16u clip_height;

  Bit32u chroma_color_fmt;
  Bit32u chroma_color;

  Bit32u patt_shape;
  bool patt_type_color;
  Bit32u patt_bg_color;
  Bit32u patt_fg_color;
  bool patt_data_mono[64];
  Bit32u patt_data_color[64];

  Bit32u gdi_operation;
  Bit32u gdi_color_fmt;
  Bit32u gdi_mono_fmt;
  Bit32u gdi_clip_yx0;
  Bit32u gdi_clip_yx1;
  Bit32u gdi_rect_color;
  Bit32u gdi_rect_xy;
  Bit32u gdi_rect_yx0;
  Bit32u gdi_rect_yx1;
  Bit32u gdi_rect_wh;
  Bit32u gdi_bg_color;
  Bit32u gdi_fg_color;
  Bit32u gdi_image_swh;
  Bit32u gdi_image_dwh;
  Bit32u gdi_image_xy;
  Bit32u gdi_words_ptr;
  Bit32u gdi_words_left;
  Bit32u* gdi_words;

  Bit32u rect_operation;
  Bit32u rect_color_fmt;
  Bit32u rect_color;
  Bit32u rect_yx;
  Bit32u rect_hw;
};

class bx_nvriva_c : public bx_vgacore_c
{
public:
  bx_nvriva_c();
  virtual ~bx_nvriva_c();

  virtual bool init_vga_extension(void);
  Bit16u get_crtc_vtotal();
  virtual void get_crtc_params(bx_crtc_params_t* crtcp, Bit32u* vclock);
  virtual void reset(unsigned type);
  virtual void redraw_area(unsigned x0, unsigned y0,
                           unsigned width, unsigned height);
  void redraw_area_d(Bit32s x0, Bit32s y0, Bit32u width, Bit32u height);
  void redraw_area_nd(Bit32s x0, Bit32s y0, Bit32u width, Bit32u height);
  void redraw_area_nd(Bit32u offset, Bit32u width, Bit32u height);
  virtual Bit8u mem_read(bx_phy_address addr);
  virtual void mem_write(bx_phy_address addr, Bit8u value);
  virtual void get_text_snapshot(Bit8u **text_snapshot,
                                 unsigned *txHeight, unsigned *txWidth);
  virtual void register_state(void);
  virtual void after_restore_state(void);

#if BX_SUPPORT_PCI
  virtual void pci_write_handler(Bit8u address, Bit32u value, unsigned io_len);
#endif
#if BX_DEBUGGER
  virtual void debug_dump(int argc, char **argv);
#endif

protected:
  virtual void update(void);

private:
#if BX_USE_NVRIVA_SMF
  typedef void (*nv4_method_handler)(
    nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param);
#else
  typedef void (bx_nvriva_c::*nv4_method_handler)(
    nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param);
#endif

  static Bit32u svga_read_handler(void *this_ptr, Bit32u address, unsigned io_len);
  static void   svga_write_handler(void *this_ptr, Bit32u address, Bit32u value, unsigned io_len);
#if !BX_USE_NVRIVA_SMF
  Bit32u svga_read(Bit32u address, unsigned io_len);
  void   svga_write(Bit32u address, Bit32u value, unsigned io_len);
#endif
  BX_NVRIVA_SMF void   svga_init_members();
  BX_NVRIVA_SMF void   bitblt_init();
  BX_NVRIVA_SMF void   init_method_handlers();
  BX_NVRIVA_SMF void   set_method_handler(Bit32u cls, Bit32u method, nv4_method_handler handler);
  BX_NVRIVA_SMF void   set_method_handler(Bit32u cls, Bit32u method_start, Bit32u method_end, nv4_method_handler handler);

  BX_NVRIVA_SMF Bit16u cursor_read16(Bit32u address);
  BX_NVRIVA_SMF Bit32u cursor_read32(Bit32u address);
  BX_NVRIVA_SMF void draw_hardware_cursor(unsigned, unsigned, bx_svga_tileinfo_t *);

  BX_NVRIVA_SMF Bit8u svga_read_crtc(Bit32u address, unsigned index);
  BX_NVRIVA_SMF void  svga_write_crtc(Bit32u address, unsigned index, Bit8u value);

  BX_NVRIVA_SMF void set_irq_level(bool level);
  BX_NVRIVA_SMF Bit32u get_mc_intr();
  BX_NVRIVA_SMF void update_irq_level();

  BX_NVRIVA_SMF Bit8u register_read8(Bit32u address);
  BX_NVRIVA_SMF void  register_write8(Bit32u address, Bit8u value);
  BX_NVRIVA_SMF Bit32u register_read32(Bit32u address);
  BX_NVRIVA_SMF void  register_write32(Bit32u address, Bit32u value);

  BX_NVRIVA_SMF Bit8u vram_read8(Bit32u address);
  BX_NVRIVA_SMF Bit16u vram_read16(Bit32u address);
  BX_NVRIVA_SMF Bit32u vram_read32(Bit32u address);
  BX_NVRIVA_SMF void vram_write8(Bit32u address, Bit8u value);
  BX_NVRIVA_SMF void vram_write16(Bit32u address, Bit16u value);
  BX_NVRIVA_SMF void vram_write32(Bit32u address, Bit32u value);
  BX_NVRIVA_SMF Bit8u ramin_read8(Bit32u address);
  BX_NVRIVA_SMF Bit16u ramin_read16(Bit32u address);
  BX_NVRIVA_SMF Bit32u ramin_read32(Bit32u address);
  BX_NVRIVA_SMF void ramin_write8(Bit32u address, Bit8u value);
  BX_NVRIVA_SMF void ramin_write32(Bit32u address, Bit32u value);
  BX_NVRIVA_SMF Bit8u physical_read8(Bit32u address);
  BX_NVRIVA_SMF Bit16u physical_read16(Bit32u address);
  BX_NVRIVA_SMF Bit32u physical_read32(Bit32u address);
  BX_NVRIVA_SMF void physical_write8(Bit32u address, Bit8u value);
  BX_NVRIVA_SMF void physical_write16(Bit32u address, Bit16u value);
  BX_NVRIVA_SMF void physical_write32(Bit32u address, Bit32u value);
  BX_NVRIVA_SMF Bit8u dma_read8(Bit32u object, Bit32u address);
  BX_NVRIVA_SMF Bit16u dma_read16(Bit32u object, Bit32u address);
  BX_NVRIVA_SMF Bit32u dma_read32(Bit32u object, Bit32u address);
  BX_NVRIVA_SMF void dma_write8(Bit32u object, Bit32u address, Bit8u value);
  BX_NVRIVA_SMF void dma_write16(Bit32u object, Bit32u address, Bit16u value);
  BX_NVRIVA_SMF void dma_write32(Bit32u object, Bit32u address, Bit32u value);
  BX_NVRIVA_SMF void dma_copy(Bit32u dst_obj, Bit32u dst_addr,
    Bit32u src_obj, Bit32u src_addr, Bit32u byte_count);
  BX_NVRIVA_SMF Bit32u dma_pt_lookup(Bit32u object, Bit32u address);
  BX_NVRIVA_SMF Bit32u dma_lin_lookup(Bit32u object, Bit32u address);
  BX_NVRIVA_SMF Bit32u ramfc_address(Bit32u chid, Bit32u offset);
  BX_NVRIVA_SMF void ramfc_write32(Bit32u chid, Bit32u offset, Bit32u value);
  BX_NVRIVA_SMF Bit32u ramfc_read32(Bit32u chid, Bit32u offset);

  BX_NVRIVA_SMF Bit64u get_current_time();

  BX_NVRIVA_SMF void ramht_lookup(Bit32u handle, Bit32u chid, Bit32u* object, Bit8u* engine);

  BX_NVRIVA_SMF void update_fifo_wait();
  BX_NVRIVA_SMF void fifo_process();
  BX_NVRIVA_SMF void fifo_process(Bit32u chid);
  BX_NVRIVA_SMF int execute_command(Bit32u chid, Bit32u subc, Bit32u method, Bit32u param);

  BX_NVRIVA_SMF void update_color_bytes_s2d(nv4_channel* ch);
  BX_NVRIVA_SMF void update_color_bytes_ifc(nv4_channel* ch);
  BX_NVRIVA_SMF void update_color_bytes_sifc(nv4_channel* ch);
  BX_NVRIVA_SMF void update_color_bytes_tfc(nv4_channel* ch);
  BX_NVRIVA_SMF void update_color_bytes_iifc(nv4_channel* ch);
  BX_NVRIVA_SMF void update_color_bytes(Bit32u s2d_color_fmt, Bit32u color_fmt, Bit32u* color_bytes);

  BX_NVRIVA_SMF void execute_clip(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_m2mf(nv4_channel* ch, Bit32u subc, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_rop(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_patt(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_gdi(nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_swzsurf(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_chroma(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_rect(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_imageblit(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_ifc(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_surf2d(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_iifc(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_sifc(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_beta(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_tfc(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_sifm(nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param);

  BX_NVRIVA_SMF void execute_surf3d(nv4_channel* ch, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_tex_tri(nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param);
  BX_NVRIVA_SMF void execute_mtex_tri(nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param);

  BX_NVRIVA_SMF void empty_method_handler(nv4_channel* ch, Bit32u cls, Bit32u method, Bit32u param);

  BX_NVRIVA_SMF Bit32u get_pixel(Bit32u obj, Bit32u ofs, Bit32u x, Bit32u cb);
  BX_NVRIVA_SMF void put_pixel(nv4_channel* ch, Bit32u ofs, Bit32u x, Bit32u value);
  BX_NVRIVA_SMF void put_pixel_swzs(nv4_channel* ch, Bit32u ofs, Bit32u value);
  BX_NVRIVA_SMF void pixel_operation(nv4_channel* ch, Bit32u op,
    Bit32u* dstcolor, const Bit32u* srccolor, Bit32u cb, Bit32u px, Bit32u py);

  BX_NVRIVA_SMF void gdi_fillrect(nv4_channel* ch, bool clipped);
  BX_NVRIVA_SMF void gdi_blit(nv4_channel* ch, Bit32u type);
  BX_NVRIVA_SMF void rect(nv4_channel* ch);
  BX_NVRIVA_SMF void ifc(nv4_channel* ch, Bit32u word);
  BX_NVRIVA_SMF void iifc(nv4_channel* ch);
  BX_NVRIVA_SMF void sifc(nv4_channel* ch);
  BX_NVRIVA_SMF void copyarea(nv4_channel* ch);
  BX_NVRIVA_SMF void tfc(nv4_channel* ch);
  BX_NVRIVA_SMF void m2mf(nv4_channel* ch);
  BX_NVRIVA_SMF void sifm(nv4_channel* ch, bool swizzled);

  BX_NVRIVA_SMF void d3d_texture_sample(nv4_channel* ch, nv4_texture* tex,
    float u, float v, float color_out[4]);
  BX_NVRIVA_SMF void d3d_rasterize_triangle(nv4_channel* ch,
    float v0[8], float v1[8], float v2[8], bool multitex);
  BX_NVRIVA_SMF void d3d_draw_primitive(nv4_channel* ch, Bit32u prim_word, bool multitex);

  struct {
    Bit8u index;
    Bit8u reg[RIVA_CRTC_MAX+1];
  } crtc;

  bool mc_soft_intr;
  Bit32u mc_intr_en;
  Bit32u mc_enable;
  Bit32u bus_intr;
  Bit32u bus_intr_en;
  bool fifo_wait;
  bool fifo_wait_soft;
  bool fifo_wait_notify;
  bool fifo_wait_flip;
  bool fifo_wait_acquire;
  Bit32u fifo_intr;
  Bit32u fifo_intr_en;
  Bit32u fifo_ramht;
  Bit32u fifo_ramfc;
  Bit32u fifo_ramro;
  Bit32u fifo_mode;
  Bit32u fifo_cache1_push0;
  Bit32u fifo_cache1_push1;
  Bit32u fifo_cache1_put;
  Bit32u fifo_cache1_dma_push;
  Bit32u fifo_cache1_dma_instance;
  Bit32u fifo_cache1_dma_put;
  Bit32u fifo_cache1_dma_get;
  Bit32u fifo_cache1_ref_cnt;
  Bit32u fifo_cache1_pull0;
  Bit32u fifo_cache1_get;
  Bit32u fifo_cache1_method[RIVA_CACHE1_SIZE];
  Bit32u fifo_cache1_data[RIVA_CACHE1_SIZE];
  Bit32u rma_addr;
  Bit32u timer_intr;
  Bit32u timer_intr_en;
  Bit32u timer_num;
  Bit32u timer_den;
  Bit64u timer_inittime1;
  Bit64u timer_inittime2;
  Bit32u timer_alarm;
  Bit32u straps0_primary;
  Bit32u straps0_primary_original;
  Bit32u graph_intr;
  Bit32u graph_nsource;
  Bit32u graph_intr_en;
  Bit32u graph_ctx_switch1;
  Bit32u graph_ctx_switch2;
  Bit32u graph_ctx_switch4;
  Bit32u graph_ctxctl_cur;
  Bit32u graph_status;
  Bit32u graph_trapped_addr;
  Bit32u graph_trapped_data;
  Bit32u graph_flip_read;
  Bit32u graph_flip_write;
  Bit32u graph_flip_modulo;
  Bit32u graph_notify;
  Bit32u graph_fifo;
  Bit32u graph_bpixel;
  Bit32u graph_channel_ctx_table;
  Bit32u graph_offset0;
  Bit32u graph_pitch0;
  Bit32u crtc_intr;
  Bit32u crtc_intr_en;
  Bit32u crtc_start;
  Bit32u crtc_config;
  Bit32u crtc_raster_pos;
  Bit32u crtc_cursor_offset;
  Bit32u crtc_cursor_config;
  Bit32u crtc_gpio_ext;
  Bit32u ramdac_cu_start_pos;
  Bit32u ramdac_nvpll;
  Bit32u ramdac_mpll;
  Bit32u ramdac_vpll;
  Bit32u ramdac_pll_select;
  Bit32u ramdac_general_control;
  Bit32u pfb_boot_0;
  Bit32u pfb_config_0;
  Bit32u pfb_config_1;

  nv4_method_handler empty_method_handlers[RIVA_METHOD_COUNT];
  nv4_method_handler cl0054_method_handlers[RIVA_METHOD_COUNT];
  nv4_method_handler cl0055_method_handlers[RIVA_METHOD_COUNT];
  nv4_method_handler* class_method_handlers[RIVA_CLASS_COUNT];
  bx_bitblt_rop_t rop_handler[0x100];
  Bit8u rop_flags[0x100];

  nv4_channel chs[RIVA_CHANNEL_COUNT];

  Bit32u unk_regs[4*1024*1024];

  bool svga_unlock_special;
  bool svga_needs_update_tile;
  bool svga_needs_update_dispentire;
  bool svga_needs_update_mode;
  bool svga_double_width;

  Bit8u devfunc;
  unsigned svga_xres;
  unsigned svga_yres;
  unsigned svga_pitch;
  unsigned svga_bpp;
  unsigned svga_dispbpp;

  Bit32u memsize_mask;
  Bit32u ramin_base;
  Bit32u ramin_size;

  Bit8u *disp_ptr;
  Bit32u disp_offset;
  Bit32u disp_end_offset;
  Bit32u bank_base[2];

  struct {
    bool vram;
    Bit32u offset;
    Bit16s x, y;
    Bit8u size;
    bool bpp32;
    bool enabled;
  } hw_cursor;

  bx_ddc_c ddc;

  bool is_unlocked() { return svga_unlock_special; }

  BX_NVRIVA_SMF void svga_init_pcihandlers(void);

  void vertical_timer();

  static bool nvriva_mem_read_handler(bx_phy_address addr, unsigned len, void *data, void *param);
  static bool nvriva_mem_write_handler(bx_phy_address addr, unsigned len, void *data, void *param);
};

#endif // BX_SUPPORT_NVRIVA
