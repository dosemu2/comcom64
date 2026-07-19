/*
 *  comcom64 - 64bit command.com
 *  Copyright (C) 2023-2026  @stsp
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
#include <conio.h>
#include <stdint.h>
#include <stdlib.h>
#include <dpmi.h>
#include <sys/farptr.h>
#include <go32.h>
#include <termios.h>
#include "asm.h"
#include "command.h"
#include "djansi.h"

#define CF 1

#ifdef DJ64
static unsigned int int21_regs;
#else
static __dpmi_regs *int21_regs;
#endif

static __dpmi_raddr old_int21;
static __dpmi_raddr new_int21;

static unsigned short popw(__dpmi_regs *r)
{
  unsigned lina = (r->x.ss << 4) + r->x.sp;
  unsigned short ret = _farpeekw(_dos_ds, lina);
  r->x.sp += 2;
  return ret;
}

static void do_iret(__dpmi_regs *r, uint16_t f_mask)
{
  r->x.ip = popw(r);
  r->x.cs = popw(r);
  r->x.flags = popw(r) & f_mask;
}

static void do_ljmp(__dpmi_regs *r, __dpmi_raddr addr)
{
  r->x.ip = addr.offset16;
  r->x.cs = addr.segment;
}

void do_int21(void)
{
  static char buf[1024];  // static because of small stack
  __dpmi_regs _r;
  __dpmi_regs *r = &_r;

  /* call-back can re-enter, so we need to copy regs */
#ifdef DJ64
  _r = * (__dpmi_regs *) DATA_PTR(int21_regs);
#else
  _r = *int21_regs;
#endif
  if (r->h.ah == 0x40 && r->x.bx == STDOUT_FILENO)
    {
    int done = 0;
    int len = r->x.cx;
    djansi_disable();
    while (len)
      {
      int todo = _min(sizeof(buf), len);
      dosmemget((r->x.ds << 4) + r->x.dx + done, todo, buf);
      buf[todo] = '\0';
      write(r->x.bx, buf, todo);
      len -= todo;
      done += todo;
      }
    djansi_enable();

    r->x.ax = done;
    do_iret(r, ~CF);
    }
  else
    do_ljmp(r, old_int21);

#ifdef DJ64
  * (__dpmi_regs *) DATA_PTR(int21_regs) = _r;
#else
  *int21_regs = _r;
#endif
}

void do_int21_next(void)
{
  __dpmi_regs *r;

#ifdef DJ64
  r = (__dpmi_regs *) DATA_PTR(int21_regs);
#else
  r = int21_regs;
#endif
  do_ljmp(r, old_int21);
}

void djansi_init(void)
{
#ifdef DJ64
  int21_regs = malloc32(sizeof(__dpmi_regs));
#else
  int21_regs = (__dpmi_regs *) malloc(sizeof(__dpmi_regs));
#endif
  __dpmi_allocate_real_mode_callback(my_int21_handler, int21_regs,
				       &new_int21);
  __dpmi_get_real_mode_interrupt_vector(0x21, &old_int21);
  __dpmi_set_real_mode_interrupt_vector(0x21, &new_int21);

  tcdrain(STDOUT_FILENO);
}

void djansi_done(void)
{
  djansi_disable();
  __dpmi_set_real_mode_interrupt_vector(0x21, &old_int21);
  __dpmi_free_real_mode_callback(&new_int21);
#ifdef DJ64
  free32(int21_regs);
#else
  free(int21_regs);
#endif
}

void djansi_enable(void)
{
  int21_enabled = 1;
}

void djansi_disable(void)
{
  int21_enabled = 0;
}
