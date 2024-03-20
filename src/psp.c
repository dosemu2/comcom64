/*
 *  comcom64 - 64bit command.com
 *  psp.c: PSP handling routines
 *  Copyright (C) 2023-2024  @stsp
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stubinfo.h>
#include <sys/movedata.h>
#include "command.h"
#include "psp.h"

static unsigned psp_addr;
static unsigned short orig_psp_seg;

void set_psp_parent(void)
{
  unsigned short psp = _stubinfo->psp_selector;
  unsigned short psp_seg;
  int err;

  err = get_segment_base_address(psp, &psp_addr);
  if (!err && !(psp_addr & 0xf) && psp_addr < 0x110000) {
    psp_seg = psp_addr >> 4;
    dosmemget(psp_addr + 0x16, 2, &orig_psp_seg);
    dosmemput(&psp_seg, 2, psp_addr + 0x16);
  }
}

void restore_psp_parent(void)
{
  dosmemput(&orig_psp_seg, 2, psp_addr + 0x16);
}
