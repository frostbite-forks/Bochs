/////////////////////////////////////////////////////////////////////////
// $Id$
/////////////////////////////////////////////////////////////////////////
//
//  Apple SMC emulation for Bochs
//  Ported from QEMU's hw/misc/applesmc.c
//  Original QEMU authors: Alexander Graf <agraf@suse.de>
//                         Susanne Graf <suse@csgraf.de>
//
//  In all Intel-based Apple hardware there is an SMC chip to control the
//  backlight, fans and several other generic device parameters. It also
//  contains the magic keys used to dongle Mac OS X to the device.
//
//  Copyright (C) 2007 Alexander Graf
//  Copyright (C) 2024  The Bochs Project
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, see <http://www.gnu.org/licenses/>.

#define BX_PLUGGABLE

#include "iodev.h"
#include "applesmc.h"

#define LOG_THIS theAppleSMC->

bx_applesmc_c *theAppleSMC = NULL;

void applesmc_init_options(void)
{
  bx_list_c *misc = (bx_list_c *)SIM->get_param("misc");
  bx_list_c *applesmc = new bx_list_c(misc, "applesmc", "Apple SMC device");
  new bx_param_bool_c(applesmc,
    "enabled",
    "Enable AppleSMC device",
    "Enables the Apple SMC device required by Mac OS X",
    0);
  new bx_param_string_c(applesmc,
    "osk",
    "OSK string",
    "64-character OSK key required by Mac OS X",
    "ourhardworkbythesewordsguardedpleasedontsteal(c)AppleComputerInc",
    APPLESMC_OSK_LEN + 1);
}

Bit32s applesmc_options_parser(const char *context, int num_params, char *params[])
{
  if (!strcmp(params[0], "applesmc")) {
    for (int i = 1; i < num_params; i++) {
      if (SIM->parse_param_from_list(context, params[i],
            (bx_list_c *)SIM->get_param(BXPN_APPLESMC_ROOT)) < 0) {
        BX_PANIC(("%s: applesmc directive malformed.", context));
      }
    }
  }
  return 0;
}

Bit32s applesmc_options_save(FILE *fp)
{
  return SIM->write_param_list(fp, (bx_list_c *)SIM->get_param(BXPN_APPLESMC_ROOT), NULL, 0);
}

PLUGIN_ENTRY_FOR_MODULE(applesmc)
{
  if (mode == PLUGIN_INIT) {
    theAppleSMC = new bx_applesmc_c();
    BX_REGISTER_DEVICE_DEVMODEL(plugin, type, theAppleSMC, BX_PLUGIN_APPLESMC);
    applesmc_init_options();
    SIM->register_addon_option("applesmc", applesmc_options_parser, applesmc_options_save);
  } else if (mode == PLUGIN_FINI) {
    SIM->unregister_addon_option("applesmc");
    bx_list_c *menu = (bx_list_c *)SIM->get_param("misc");
    menu->remove("applesmc");
    delete theAppleSMC;
    theAppleSMC = NULL;
  } else if (mode == PLUGIN_PROBE) {
    return (int)PLUGTYPE_OPTIONAL;
  }
  return 0;
}

bx_applesmc_c::bx_applesmc_c()
{
  put("applesmc", "SMC");
  data_list = NULL;
  memset(osk, 0, sizeof(osk));
}

bx_applesmc_c::~bx_applesmc_c()
{
  // free linked list
  AppleSMCData *d = data_list;
  while (d) {
    AppleSMCData *next = d->next;
    delete d;
    d = next;
  }
  data_list = NULL;

  SIM->get_bochs_root()->remove("applesmc");
  BX_DEBUG(("Exit"));
}

void bx_applesmc_c::add_key(const char *key, Bit8u len, const char *data)
{
  AppleSMCData *def = new AppleSMCData;
  def->key  = key;
  def->len  = len;
  def->data = data;
  def->next = BX_APPLESMC_THIS data_list;
  BX_APPLESMC_THIS data_list = def;
}

void bx_applesmc_c::init(void)
{
  bx_param_string_c *osk_param;

  BX_DEBUG(("Init"));

  // Register the 0x20 ports starting at APPLESMC_DEFAULT_IOBASE
  for (Bit32u addr = APPLESMC_DEFAULT_IOBASE;
       addr < APPLESMC_DEFAULT_IOBASE + APPLESMC_NUM_PORTS; addr++) {
    DEV_register_ioread_handler(this, read_handler, addr, "AppleSMC", 1);
    DEV_register_iowrite_handler(this, write_handler, addr, "AppleSMC", 1);
  }

  // Get OSK from config
  osk_param = (bx_param_string_c *)SIM->get_param_string(BXPN_APPLESMC_OSK);
  if (osk_param != NULL) {
    const char *osk_val = osk_param->getptr();
    if (osk_val && strlen(osk_val) == APPLESMC_OSK_LEN) {
      strncpy(BX_APPLESMC_THIS osk, osk_val, APPLESMC_OSK_LEN);
    } else {
      BX_ERROR(("applesmc: OSK string must be exactly 64 characters, using dummy key"));
      strncpy(BX_APPLESMC_THIS osk,
              "This is a dummy key. Enter the real key using the osk parameter",
              APPLESMC_OSK_LEN);
    }
  } else {
    BX_ERROR(("applesmc: no OSK parameter found, using dummy key"));
    strncpy(BX_APPLESMC_THIS osk,
            "This is a dummy key. Enter the real key using the osk parameter",
            APPLESMC_OSK_LEN);
  }
  BX_APPLESMC_THIS osk[APPLESMC_OSK_LEN] = '\0';

  // Populate SMC key-value store (same keys as QEMU)
  add_key("REV ",  6, "\x01\x13\x0f\x00\x00\x03");
  add_key("OSK0", 32, BX_APPLESMC_THIS osk);
  add_key("OSK1", 32, BX_APPLESMC_THIS osk + 32);
  add_key("NATJ",  1, "\0");
  add_key("MSSP",  1, "\0");
  add_key("MSSD",  1, "\x03");

  // Initial state
  BX_APPLESMC_THIS s.cmd       = 0;
  BX_APPLESMC_THIS s.status    = 0;
  BX_APPLESMC_THIS s.status_1e = 0;
  BX_APPLESMC_THIS s.last_ret  = 0;
  BX_APPLESMC_THIS s.read_pos  = 0;
  BX_APPLESMC_THIS s.data_pos  = 0;
  BX_APPLESMC_THIS s.data_len  = 0;
  memset(BX_APPLESMC_THIS s.key, 0, 4);
  memset(BX_APPLESMC_THIS s.data, 0, sizeof(BX_APPLESMC_THIS s.data));
}

void bx_applesmc_c::reset(unsigned type)
{
  BX_APPLESMC_THIS s.status    = 0x00;
  BX_APPLESMC_THIS s.status_1e = 0x00;
  BX_APPLESMC_THIS s.last_ret  = 0x00;
  BX_APPLESMC_THIS s.cmd       = 0;
  BX_APPLESMC_THIS s.read_pos  = 0;
  BX_APPLESMC_THIS s.data_pos  = 0;
  BX_APPLESMC_THIS s.data_len  = 0;
}

void bx_applesmc_c::register_state(void)
{
  bx_list_c *list = new bx_list_c(SIM->get_bochs_root(), "applesmc", "AppleSMC State");
  BXRS_HEX_PARAM_FIELD(list, cmd,       BX_APPLESMC_THIS s.cmd);
  BXRS_HEX_PARAM_FIELD(list, status,    BX_APPLESMC_THIS s.status);
  BXRS_HEX_PARAM_FIELD(list, status_1e, BX_APPLESMC_THIS s.status_1e);
  BXRS_HEX_PARAM_FIELD(list, last_ret,  BX_APPLESMC_THIS s.last_ret);
  BXRS_HEX_PARAM_FIELD(list, read_pos,  BX_APPLESMC_THIS s.read_pos);
  BXRS_HEX_PARAM_FIELD(list, data_len,  BX_APPLESMC_THIS s.data_len);
  BXRS_HEX_PARAM_FIELD(list, data_pos,  BX_APPLESMC_THIS s.data_pos);
}

const AppleSMCData *bx_applesmc_c::find_key(void)
{
  AppleSMCData *d = BX_APPLESMC_THIS data_list;
  while (d) {
    if (!memcmp(d->key, BX_APPLESMC_THIS s.key, 4)) {
      return d;
    }
    d = d->next;
  }
  return NULL;
}

// static read/write callbacks
Bit32u bx_applesmc_c::read_handler(void *this_ptr, Bit32u address, unsigned io_len)
{
#if !BX_USE_APPLESMC_SMF
  bx_applesmc_c *class_ptr = (bx_applesmc_c *)this_ptr;
  return class_ptr->read(address, io_len);
}

Bit32u bx_applesmc_c::read(Bit32u address, unsigned io_len)
{
#else
  UNUSED(this_ptr);
#endif
  Bit32u offset = address - APPLESMC_DEFAULT_IOBASE;

  switch (offset) {
    case APPLESMC_DATA_PORT:
      // Read data
      switch (BX_APPLESMC_THIS s.cmd) {
        case APPLESMC_READ_CMD:
          if (!(BX_APPLESMC_THIS s.status & APPLESMC_ST_DATA_READY))
            break;
          if (BX_APPLESMC_THIS s.data_pos < BX_APPLESMC_THIS s.data_len) {
            BX_APPLESMC_THIS s.last_ret = BX_APPLESMC_THIS s.data[BX_APPLESMC_THIS s.data_pos];
            BX_DEBUG(("READ key[%d] = 0x%02x", BX_APPLESMC_THIS s.data_pos,
                      BX_APPLESMC_THIS s.last_ret));
            BX_APPLESMC_THIS s.data_pos++;
            if (BX_APPLESMC_THIS s.data_pos == BX_APPLESMC_THIS s.data_len) {
              BX_APPLESMC_THIS s.status = APPLESMC_ST_CMD_DONE;
            } else {
              BX_APPLESMC_THIS s.status = APPLESMC_ST_ACK | APPLESMC_ST_DATA_READY;
            }
          }
          break;
        default:
          BX_APPLESMC_THIS s.status    = APPLESMC_ST_CMD_DONE;
          BX_APPLESMC_THIS s.status_1e = APPLESMC_ST_1E_STILL_BAD_CMD;
          break;
      }
      return BX_APPLESMC_THIS s.last_ret;

    case APPLESMC_CMD_PORT:
      BX_DEBUG(("CMD read: status=0x%02x", BX_APPLESMC_THIS s.status));
      return BX_APPLESMC_THIS s.status;

    case APPLESMC_ERR_PORT:
      BX_DEBUG(("ERR read: 0x%02x", BX_APPLESMC_THIS s.status_1e));
      return BX_APPLESMC_THIS s.status_1e;

    default:
      return 0xff;
  }
}

void bx_applesmc_c::write_handler(void *this_ptr, Bit32u address, Bit32u value, unsigned io_len)
{
#if !BX_USE_APPLESMC_SMF
  bx_applesmc_c *class_ptr = (bx_applesmc_c *)this_ptr;
  class_ptr->write(address, value, io_len);
}

void bx_applesmc_c::write(Bit32u address, Bit32u value, unsigned io_len)
{
#else
  UNUSED(this_ptr);
#endif
  Bit32u offset = address - APPLESMC_DEFAULT_IOBASE;
  Bit8u  val    = (Bit8u)(value & 0xff);

  switch (offset) {
    case APPLESMC_CMD_PORT:
      {
        Bit8u cur_status = BX_APPLESMC_THIS s.status & 0x0f;
        BX_DEBUG(("CMD write: 0x%02x", val));
        switch (val) {
          case APPLESMC_READ_CMD:
            if (cur_status == APPLESMC_ST_CMD_DONE || cur_status == APPLESMC_ST_NEW_CMD) {
              BX_APPLESMC_THIS s.cmd    = val;
              BX_APPLESMC_THIS s.status = APPLESMC_ST_NEW_CMD | APPLESMC_ST_ACK;
            } else {
              BX_DEBUG(("previous command interrupted"));
              BX_APPLESMC_THIS s.status    = APPLESMC_ST_NEW_CMD;
              BX_APPLESMC_THIS s.status_1e = APPLESMC_ST_1E_CMD_INTRUPTED;
            }
            break;
          default:
            BX_DEBUG(("unexpected CMD 0x%02x", val));
            BX_APPLESMC_THIS s.status    = APPLESMC_ST_NEW_CMD;
            BX_APPLESMC_THIS s.status_1e = APPLESMC_ST_1E_BAD_CMD;
            break;
        }
        BX_APPLESMC_THIS s.read_pos = 0;
        BX_APPLESMC_THIS s.data_pos = 0;
      }
      break;

    case APPLESMC_DATA_PORT:
      {
        BX_DEBUG(("DATA write: 0x%02x", val));
        switch (BX_APPLESMC_THIS s.cmd) {
          case APPLESMC_READ_CMD:
            if ((BX_APPLESMC_THIS s.status & 0x0f) == APPLESMC_ST_CMD_DONE)
              break;
            if (BX_APPLESMC_THIS s.read_pos < 4) {
              BX_APPLESMC_THIS s.key[BX_APPLESMC_THIS s.read_pos] = (char)val;
              BX_APPLESMC_THIS s.status = APPLESMC_ST_ACK;
            } else if (BX_APPLESMC_THIS s.read_pos == 4) {
              const AppleSMCData *d = find_key();
              if (d != NULL) {
                memcpy(BX_APPLESMC_THIS s.data, d->data, d->len);
                BX_APPLESMC_THIS s.data_len = d->len;
                BX_APPLESMC_THIS s.data_pos = 0;
                BX_APPLESMC_THIS s.status    = APPLESMC_ST_ACK | APPLESMC_ST_DATA_READY;
                BX_APPLESMC_THIS s.status_1e = APPLESMC_ST_CMD_DONE;
              } else {
                BX_DEBUG(("key '%c%c%c%c' not found",
                          BX_APPLESMC_THIS s.key[0], BX_APPLESMC_THIS s.key[1],
                          BX_APPLESMC_THIS s.key[2], BX_APPLESMC_THIS s.key[3]));
                BX_APPLESMC_THIS s.status    = APPLESMC_ST_CMD_DONE;
                BX_APPLESMC_THIS s.status_1e = APPLESMC_ST_1E_NOEXIST;
              }
            }
            BX_APPLESMC_THIS s.read_pos++;
            break;
          default:
            BX_APPLESMC_THIS s.status    = APPLESMC_ST_CMD_DONE;
            BX_APPLESMC_THIS s.status_1e = APPLESMC_ST_1E_STILL_BAD_CMD;
            break;
        }
      }
      break;

    case APPLESMC_ERR_PORT:
      // writes to error port are ignored (matches QEMU behavior)
      BX_DEBUG(("ERR write ignored: 0x%02x", val));
      break;

    default:
      break;
  }
}
