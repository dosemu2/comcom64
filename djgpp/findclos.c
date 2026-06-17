/*
 *  dj64 - 64bit djgpp-compatible tool-chain
 *  Copyright (C) 2021-2024  @stsp
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

#include <errno.h>
#include <fcntl.h>
#include <dpmi.h>
#include <libc/dosio.h>
#include "findclos.h"

int
findclose(int handle)
{
  __dpmi_regs r;
  int use_lfn = _USE_LFN;

  if (use_lfn)
  {
    r.x.flags |= 1;  /* Always set CF before calling a 0x71NN function. */
    r.x.bx = handle;
    r.x.ax = 0x71a1;
    __dpmi_int(0x21, &r);
    if (!(r.x.flags & 1))
      return 0;
    errno = __doserr_to_errno(r.x.ax);
    return errno;
  }
  return 0;
}

int __attribute__((alias("findclose")))
__findclose(int handle);
