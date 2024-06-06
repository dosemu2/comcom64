/*
 *  comcom64 - 64bit command.com
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
#include <conio.h>
#include <stdlib.h>
#include <dpmi.h>
#include <sys/nearptr.h>
#include <sys/farptr.h>
#include <go32.h>
#include "asm.h"
#include "ms.h"

#define CF 1

#ifdef DJ64
static unsigned int mouse_regs;
#else
static __dpmi_regs *mouse_regs;
#endif

static __dpmi_raddr newm;
static __dpmi_raddr oldm;
static unsigned old_mask;
#define MEV_MASK 0xab

static unsigned short popw(__dpmi_regs *r)
{
    unsigned lina = (r->x.ss << 4) + r->x.sp;
    unsigned short ret = _farpeekw(_dos_ds, lina);
    r->x.sp += 2;
    return ret;
}

static void do_retf(__dpmi_regs *r)
{
    r->x.ip = popw(r);
    r->x.cs = popw(r);
}

static void mvxl(int d)
{
    __dpmi_regs r = { };

    while (d--) {
	r.x.ax = 0x500;
	r.x.cx = 0x4BE0;
	__dpmi_int(0x16, &r);
    }
}

static void mvxr(int d)
{
    __dpmi_regs r = { };

    while (d--) {
	r.x.ax = 0x500;
	r.x.cx = 0x4DE0;
	__dpmi_int(0x16, &r);
    }
}

static void mlb(int alt_fn, int x, int y)
{
    __dpmi_regs r = { };
    short c;

    if (alt_fn) {
	int cx = wherex();
	if (cx == x)
	    return;
	if (x < cx)
	    mvxl(cx - x);
	else
	    mvxr(x - cx);
	return;
    }

    _conio_gettext(x, y, x, y, &c);

    r.x.ax = 0x500;
    r.x.cx = c & 0xff;
    __dpmi_int(0x16, &r);
}

static void mrb(int alt_fn)
{
    __dpmi_regs r = { };

    r.x.ax = 0x500;
    r.x.cx = alt_fn ? 0x3 : 0x0F09;	// ^C or TAB
    __dpmi_int(0x16, &r);
}

static void mmb(int alt_fn)
{
    __dpmi_regs r = { };

    r.x.ax = 0x500;
    r.x.cx = alt_fn ? 0x0E08 : 0x1c0d;	// BkSp or ENTER
    __dpmi_int(0x16, &r);
}

static void mw(int delta)
{
    __dpmi_regs r = { };

    r.x.ax = 0x500;
    if (delta < 0)
	r.x.cx = 0x48E0;	// UP
    else
	r.x.cx = 0x50E0;	// DOWN
    __dpmi_int(0x16, &r);
}

void do_mouse(void)
{
    __dpmi_regs *r;
    unsigned char rows = wherey();
    static unsigned char prev_col, prev_row;
    unsigned char col, row;
    int dragged;

#ifdef DJ64
    r = (__dpmi_regs *) DATA_PTR(mouse_regs);
#else
    r = mouse_regs;
#endif
    do_retf(r);

    col = r->x.cx / 8 + 1;
    row = r->x.dx / 8 + 1;
    dragged = (r->x.ax & r->x.bx & 1) && (col != prev_col
					  || row != prev_row);

    if ((r->x.ax & 2) || dragged)
	mlb(row == rows, col, row);
    if (r->x.ax & 8)
	mrb(row == rows);
    if (r->x.ax & 0x20)
	mmb(row == rows);
    if (r->x.ax & 0x80)
	mw((char) r->h.bh);

    prev_col = r->x.cx / 8 + 1;
    prev_row = r->x.dx / 8 + 1;
}

int mouse_init(void)
{
    __dpmi_regs r = { };

    __dpmi_int(0x33, &r);
    if ((r.x.flags & CF) || r.x.ax != 0xffff || r.x.bx != 3) {
	puts("mouse not detected");
	return 0;
    }
    /* check the wheel */
    r.x.ax = 0x11;
    __dpmi_int(0x33, &r);
    if ((r.x.flags & CF) || r.x.ax != 0x574d || (r.x.cx & 1) == 0) {
	puts("mouse wheel not supported");
//    return 0;
    }

#ifdef DJ64
    mouse_regs = malloc32(sizeof(__dpmi_regs));
#else
    mouse_regs = (__dpmi_regs *) malloc(sizeof(__dpmi_regs));
#endif
    __dpmi_allocate_real_mode_callback(my_mouse_handler, mouse_regs,
				       &newm);
    r.x.ax = 0x14;
    r.x.cx = MEV_MASK;
    r.x.es = newm.segment;
    r.x.dx = newm.offset16;
    __dpmi_int(0x33, &r);
    oldm.segment = r.x.es;
    oldm.offset16 = r.x.dx;
    old_mask = r.x.cx;

    mouse_show();
    return 1;
}

void mouse_enable(void)
{
    __dpmi_regs r = { };

    __dpmi_int(0x33, &r);  // reset the visibility counter
    if ((r.x.flags & CF) || r.x.ax != 0xffff || r.x.bx != 3) {
	puts("mouse not detected");
	return;
    }

    r.x.ax = 0x0c;
    r.x.cx = MEV_MASK;
    r.x.es = newm.segment;
    r.x.dx = newm.offset16;
    __dpmi_int(0x33, &r);

    mouse_show();
}

void mouse_disable(void)
{
    __dpmi_regs r = { };

    mouse_hide();

    r.x.ax = 0x0c;
    r.x.cx = old_mask;
    r.x.es = oldm.segment;
    r.x.dx = oldm.offset16;
    __dpmi_int(0x33, &r);
}

void mouse_done(void)
{
    mouse_disable();
    __dpmi_free_real_mode_callback(&newm);
#ifdef DJ64
    free32(mouse_regs);
#else
    free(mouse_regs);
#endif
}

void mouse_show(void)
{
    __dpmi_regs r = { };

    r.x.ax = 1;
    __dpmi_int(0x33, &r);
}

void mouse_hide(void)
{
    __dpmi_regs r = { };

    r.x.ax = 2;
    __dpmi_int(0x33, &r);
}
