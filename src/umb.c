/*
 *  comcom64 - 64bit command.com
 *  umb.c: UMB handling routines
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

#include <dpmi.h>
#include "umb.h"

void link_umb(unsigned char strat)
{
  __dpmi_regs r = {};
  r.x.ax = 0x5803;
  r.x.bx = 1;
  __dpmi_int(0x21, &r);
  r.x.ax = 0x5801;
  r.x.bx = strat;
  __dpmi_int(0x21, &r);
}

void unlink_umb(void)
{
  __dpmi_regs r = {};
  r.x.ax = 0x5803;
  r.x.bx = 0;
  __dpmi_int(0x21, &r);
  r.x.ax = 0x5801;
  r.x.bx = 0;
  __dpmi_int(0x21, &r);
}
