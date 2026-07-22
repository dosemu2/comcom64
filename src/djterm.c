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
#include <unistd.h>
#include "asm.h"
#include "command.h"
#include "djterm.h"

#define CF 1
#define DOS_HELPER_INT 0xe6
#define DOS_HELPER_TERM_HANDLER 0x15
#define DOS_SUBHELPER_TERM_HANDLER_UNSET 0
#define DOS_SUBHELPER_TERM_HANDLER_SET 1

#ifdef DJ64
static unsigned int int21_regs;
static unsigned int term_regs;
#else
static __dpmi_regs *int21_regs;
static __dpmi_regs *term_regs;
#endif

static __dpmi_raddr old_int21;
static __dpmi_raddr new_int21;
static __dpmi_raddr term_cb;
static int int21_hooked;
static int term_hooked;

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

static void do_retf(__dpmi_regs *r)
{
  r->x.ip = popw(r);
  r->x.cs = popw(r);
}

static void do_ljmp(__dpmi_regs *r, __dpmi_raddr addr)
{
  r->x.ip = addr.offset16;
  r->x.cs = addr.segment;
}

static int term_write(unsigned addr, int len)
{
  static char buf[1024];  // static because of small stack
  struct termios term, old_term;
  int done = 0;

  tcgetattr(STDOUT_FILENO, &old_term);
  term = old_term;
  term.c_oflag &= ~(ONLCR | OCRNL);
  tcsetattr(STDOUT_FILENO, TCSADRAIN, &term);
  while (len)
    {
    int todo = _min(sizeof(buf), len);
    dosmemget(addr + done, todo, buf);
    write(STDOUT_FILENO, buf, todo);
    len -= todo;
    done += todo;
    }
  tcsetattr(STDOUT_FILENO, TCSADRAIN, &old_term);
  return done;
}

void do_int21(void)
{
  __dpmi_regs _r;
  __dpmi_regs *r = &_r;
  int proceed = 0;

  /* call-back can re-enter, so we need to copy regs */
#ifdef DJ64
  _r = * (__dpmi_regs *) DATA_PTR(int21_regs);
#else
  _r = *int21_regs;
#endif
  if (r->h.ah == 0x40 && r->x.bx == STDOUT_FILENO)
    {
    djterm_disable();
    proceed = isatty(STDOUT_FILENO);
    djterm_enable();
    }
  if (proceed)
    {
    djterm_disable();
    r->x.ax = term_write((r->x.ds << 4) + r->x.dx, r->x.cx);
    djterm_enable();
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

void do_term(void)
{
  __dpmi_regs *r;

#ifdef DJ64
  r = (__dpmi_regs *) DATA_PTR(term_regs);
#else
  r = term_regs;
#endif
  r->x.ax = term_write((r->x.ds << 4) + r->x.dx, r->x.cx);
  do_retf(r);
}

static void free_term_cb(void)
{
  __dpmi_free_real_mode_callback(&term_cb);
#ifdef DJ64
  free32(term_regs);
#else
  free(term_regs);
#endif
}

int djterm_init(void)
{
  int ret = 1;
  __dpmi_raddr inte6;
  __dpmi_regs r = {};

  /* under dosemu we can use helper instead of hooking int21 */
  __dpmi_get_real_mode_interrupt_vector(DOS_HELPER_INT, &inte6);
  if (inte6.segment)
    {
#ifdef DJ64
    term_regs = malloc32(sizeof(__dpmi_regs));
#else
    term_regs = (__dpmi_regs *) malloc(sizeof(__dpmi_regs));
#endif
    __dpmi_allocate_real_mode_callback(my_term_handler, term_regs,
				       &term_cb);
    r.x.ax = DOS_HELPER_TERM_HANDLER;
    r.x.bx = DOS_SUBHELPER_TERM_HANDLER_SET;
    r.x.si = term_cb.segment;
    r.x.di = term_cb.offset16;
    __dpmi_int(DOS_HELPER_INT, &r);
    if (!(r.x.flags & CF) && !r.h.al)
      {
      ret |= 2;
      term_hooked = 1;
      }
    else
      free_term_cb();
    }

  tcdrain(STDOUT_FILENO);
  return ret;
}

void djterm_hook_int21(void)
{
  if (int21_hooked)
    return;
#ifdef DJ64
  int21_regs = malloc32(sizeof(__dpmi_regs));
#else
  int21_regs = (__dpmi_regs *) malloc(sizeof(__dpmi_regs));
#endif
  __dpmi_allocate_real_mode_callback(my_int21_handler, int21_regs,
				       &new_int21);
  __dpmi_get_real_mode_interrupt_vector(0x21, &old_int21);
  __dpmi_set_real_mode_interrupt_vector(0x21, &new_int21);
  int21_hooked = 1;
}

static void unhook_int21(void)
{
  djterm_disable();
  __dpmi_set_real_mode_interrupt_vector(0x21, &old_int21);
  __dpmi_free_real_mode_callback(&new_int21);
#ifdef DJ64
  free32(int21_regs);
#else
  free(int21_regs);
#endif
  int21_hooked = 0;
}

void djterm_done(void)
{
  if (int21_hooked)
    unhook_int21();
  if (term_hooked)
    {
    __dpmi_regs r = {};
    r.x.ax = DOS_HELPER_TERM_HANDLER;
    r.x.bx = DOS_SUBHELPER_TERM_HANDLER_UNSET;  // bh must also be 0
    __dpmi_int(DOS_HELPER_INT, &r);
    free_term_cb();
    }
  int21_hooked = 0;
  term_hooked = 0;
}

void djterm_enable(void)
{
  int21_enabled = 1;
}

void djterm_disable(void)
{
  int21_enabled = 0;
}
