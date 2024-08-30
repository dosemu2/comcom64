/*
 *  comcom64 - 64bit command.com
 *  clip.c: winoldap clipboard support
 *  Copyright (C) 2024  @stsp
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

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <dpmi.h>
#include <sys/movedata.h>
#include <libc/dosio.h>
#include <go32.h>
#include "clip.h"

#define CF 1

static int clip_open(void)
{
  __dpmi_regs r = {};

  r.d.eax = 0x1701;
  __dpmi_int(0x2f, &r);
  if ((r.x.flags & CF) || r.x.ax != 0x3244)  // check dosemu2 extension
    return -1;
  return r.x.ax;
}

static void clip_close(void)
{
  __dpmi_regs r = {};

  r.d.eax = 0x1708;
  __dpmi_int(0x2f, &r);
}

static unsigned clip_avail(int type)
{
  __dpmi_regs r = {};

  r.d.eax = 0x1704;
  r.d.edx = type;
  __dpmi_int(0x2f, &r);
  return ((r.x.dx << 16) | r.x.ax);
}

int clip_read(int type, void (*cbk)(const char *buf, int len))
{
  __dpmi_regs r = {};
  int rc;
  int ret = 0;
  unsigned avail;

  rc = clip_open();
  if (rc == -1)
    return rc;
  avail = clip_avail(type);
  while (avail > 0) {
    char buf[0x10000];
    uint16_t todo = (avail < 0xffff ? avail : 0xffff);
    r.d.eax = 0x1705;
    r.d.edx = type;
    r.d.edi = rc;  // enable dosemu2 extension
    r.d.ecx = todo;
    r.x.es = __tb_segment;
    r.d.ebx = __tb_offset;
    __dpmi_int(0x2f, &r);
    if ((r.x.flags & CF) || r.x.ax != todo) {
      ret = -1;
      break;
    }
    dosmemget(__tb, todo, buf);
    cbk(buf, todo);
    ret += todo;
    avail -= todo;
  }
  clip_close();
  return ret;
}

int clip_write(int type, int (*cbk)(char *buf, int len))
{
  __dpmi_regs r = {};
  int rc;
  int ret = 0;
  char buf[0x10000];
  int todo;

  rc = clip_open();
  if (rc == -1)
    return rc;
  while ((todo = cbk(buf, 0xffff)) > 0) {
    r.d.eax = 0x1703;
    r.d.edx = type;
    r.d.ecx = todo;
    r.x.es = __tb_segment;
    r.d.ebx = __tb_offset;
    dosmemput(buf, todo, __tb);
    __dpmi_int(0x2f, &r);
    if ((r.x.flags & CF) || r.x.ax == 0) {
      ret = -1;
      break;
    }
    ret += todo;
  }
  if (todo == -1)
    ret = -1;
  clip_close();
  return ret;
}
