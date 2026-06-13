/////////////////////////////////////////////////////////////////////////
// $Id$
/////////////////////////////////////////////////////////////////////////
//
//  Apple SMC emulation for Bochs
//  Ported from QEMU's hw/misc/applesmc.c
//  Original QEMU authors: Alexander Graf <agraf@suse.de>
//                         Susanne Graf <suse@csgraf.de>
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

#ifndef BX_IODEV_APPLESMC_H
#define BX_IODEV_APPLESMC_H

// AppleSMC I/O port base (matches QEMU / real hardware)
#define APPLESMC_DEFAULT_IOBASE     0x300

// Offsets from iobase
#define APPLESMC_DATA_PORT          0x00
#define APPLESMC_CMD_PORT           0x04
#define APPLESMC_ERR_PORT           0x1e
#define APPLESMC_NUM_PORTS          0x20

// Commands
#define APPLESMC_READ_CMD           0x10
#define APPLESMC_WRITE_CMD          0x11
#define APPLESMC_GET_KEY_BY_INDEX_CMD 0x12
#define APPLESMC_GET_KEY_TYPE_CMD   0x13

// Status register bits (cmd port read)
#define APPLESMC_ST_CMD_DONE        0x00
#define APPLESMC_ST_DATA_READY      0x01
#define APPLESMC_ST_BUSY            0x02
#define APPLESMC_ST_ACK             0x04
#define APPLESMC_ST_NEW_CMD         0x08

// Error codes (err port read)
#define APPLESMC_ST_1E_CMD_INTRUPTED  0x80
#define APPLESMC_ST_1E_STILL_BAD_CMD  0x81
#define APPLESMC_ST_1E_BAD_CMD        0x82
#define APPLESMC_ST_1E_NOEXIST        0x84
#define APPLESMC_ST_1E_WRITEONLY      0x85
#define APPLESMC_ST_1E_READONLY       0x86
#define APPLESMC_ST_1E_BAD_INDEX      0xb8

#define APPLESMC_OSK_LEN            64
#define APPLESMC_MAX_DATA_LENGTH    32

#if BX_USE_APPLESMC_SMF
#  define BX_APPLESMC_SMF   static
#  define BX_APPLESMC_THIS  theAppleSMC->
#else
#  define BX_APPLESMC_SMF
#  define BX_APPLESMC_THIS  this->
#endif

struct AppleSMCData {
  Bit8u      len;
  const char *key;
  const char *data;
  AppleSMCData *next;
};

class bx_applesmc_c : public bx_devmodel_c {
public:
  bx_applesmc_c();
  virtual ~bx_applesmc_c();
  virtual void init(void);
  virtual void reset(unsigned type);
  virtual void register_state(void);

private:
  struct {
    Bit8u  cmd;
    Bit8u  status;
    Bit8u  status_1e;
    Bit8u  last_ret;
    char   key[4];
    Bit8u  read_pos;
    Bit8u  data_len;
    Bit8u  data_pos;
    Bit8u  data[255];
  } s;

  char osk[APPLESMC_OSK_LEN + 1];
  AppleSMCData *data_list;

  void add_key(const char *key, Bit8u len, const char *data);
  const AppleSMCData *find_key(void);

  static Bit32u read_handler(void *this_ptr, Bit32u address, unsigned io_len);
  static void   write_handler(void *this_ptr, Bit32u address, Bit32u value, unsigned io_len);
#if !BX_USE_APPLESMC_SMF
  BX_APPLESMC_SMF Bit32u read(Bit32u address, unsigned io_len);
  BX_APPLESMC_SMF void   write(Bit32u address, Bit32u value, unsigned io_len);
#endif
};

#endif
