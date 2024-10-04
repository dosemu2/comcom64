/*
 *  comcom64 - 64bit command.com
 *  ae0x.c: interface to int2f ax=0xae00,0xae01
 *  Copyright (C) 2018       @andrewbird
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <dpmi.h>
#include <sys/movedata.h>
#include <libc/dosio.h>
#include <go32.h>
#include "env.h"
#include "ae0x.h"

#define CF 1

struct ae0x {
  struct {
    uint8_t cmax;
    uint8_t clen;
    char cbuf[256];
  } __attribute__((packed)) cmdl;
  struct {
    uint8_t nlen;
    char nbuf[11];
    char _z;
  } __attribute__((packed)) cmdn;
};

static int exec_ae01(struct ae0x *s)
{
  __dpmi_regs r = {};

  r.d.eax = 0xae01;
  r.d.edx = 0xffff;
  r.d.ecx = s->cmdn.nlen;
  /* dosmemput() was already done before ae00 */
  r.x.ds = __tb_segment;
  r.d.ebx = __tb_offset;
  r.d.esi = __tb_offset + sizeof(s->cmdl);
  r.d.edi = 0;
  set_env_seg();
  __dpmi_int(0x2f, &r);
  set_env_sel();
  if (r.x.flags & CF)
    return -1;
  dosmemget(__tb, sizeof(*s), s);
  return s->cmdn.nlen > 0;
}

int installable_command_check(char *cmd, const char *tail)
{
  /* from RBIL

  AX = AE00h
  DX = magic value FFFFh
  CH = FFh
  CL = length of command line tail (4DOS v4.0)
  DS:BX -> command line buffer (see #02977)
  DS:SI -> command name buffer (see #02978)
  DI = 0000h (4DOS v4.0)

  Return:
  AL = FFh if this command is a TSR extension to COMMAND.COM
  AL = 00h if the command should be executed as usual


  Format of COMMAND.COM command line buffer:

  Offset  Size    Description     (Table 02977)
  00h    BYTE    max length of command line, as in INT 21/AH=0Ah
  01h    BYTE    count of bytes to follow, excluding terminating 0Dh
  N BYTEs   command line text, terminated by 0Dh


  Format of command name buffer:

  Offset  Size    Description     (Table 02978)
  00h    BYTE    length of command name
  01h  N BYTEs   uppercased command name (blank-padded to 11 chars by 4DOS v4)

  */

  char *p;
  char *q;
  int i;
  char *name;
  int tlen;
  int nlen;
  __dpmi_regs r = {};
  struct ae0x s = {};
  int rc;

  p = strrchr(cmd, '\\');
  if (p)
    name = p + 1;
  else
    name = cmd;

  nlen = 0;
  for (p = name, q = &s.cmdn.nbuf[0], i = 0; *p; p++) {
    if (*p == '.') {
      nlen = i;
      if (i < 8) {
        memset(q + i, ' ', 8 - i);
        i = 8;
      }
      continue;
    }
    if (i >= sizeof(s.cmdn.nbuf))
      return -1;
    q[i++] = toupper(*p);
  }
  if (i < 11)
    memset(q + i, ' ', 11 - i);
  if (!nlen)        // no dot found
    nlen = i;
  s.cmdn.nlen = nlen;    // does not cover extension

  if (strlen(cmd) + strlen(tail) + 2 >= sizeof(s.cmdl.cbuf))
    return -1;
  s.cmdl.cmax = sizeof(s.cmdl.cbuf) - 1;
  if (tail[0]) {
    s.cmdl.clen = snprintf(s.cmdl.cbuf, sizeof(s.cmdl.cbuf),
        "%s %s\r", cmd, tail) - 1;
    tlen = strlen(tail) + 1;  // account for 'space'
  } else {
    s.cmdl.clen = snprintf(s.cmdl.cbuf, sizeof(s.cmdl.cbuf), "%s\r", cmd) - 1;
    tlen = 0;
  }

  r.d.eax = 0xae00;
  r.d.ecx = 0xff00 + tlen;
  r.d.edx = 0xffff;
  r.x.ds = __tb_segment;
  r.d.ebx = __tb_offset;
  r.d.esi = __tb_offset + sizeof(s.cmdl);
  r.d.edi = 0;
  dosmemput(&s, sizeof(s), __tb);
  set_env("PATH", getenv("PATH"));
  set_env_seg();
  __dpmi_int(0x2f, &r);
  set_env_sel();
  if (r.x.flags & CF)
    return -1;
  dosmemget(__tb, sizeof(s), &s);
  if (r.h.al != 0xff)
    return 1;
  rc = exec_ae01(&s);
  if (rc != -1)
    get_env();
  if (rc <= 0)
    return rc;
  /* dont trust nlen here as it contains the old value */
  memcpy(name, s.cmdn.nbuf, sizeof(s.cmdn.nbuf));
  name[sizeof(s.cmdn.nbuf)] = '\0';
  q = strchr(name, ' ');
  if (!q)
    {
    /* insert dot */
    memmove(name + 9, name + 8, 4);  // 4 includes \0
    name[8] = '.';
    }
  else
    {
    int spn;
    *q = '.';
    q++;
    spn = strspn(q, " ");
    if (spn)
      memmove(q, q + spn, strlen(q + spn) + 1);
    }
  return 1;
}
