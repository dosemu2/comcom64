/*
 *  FDPP - freedos port to modern C++
 *  Copyright (C) 2023  Stas Sergeev (stsp)
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
#include <dj64thnk.h>
#include "asm.h"
typedef uint32_t UDWORD;
typedef int32_t DWORD;
typedef uint8_t UBYTE;

#define fdprintf printf
#define _fail(...)

#define _ARG(n, t, ap) (*(t *)(ap + n))
#define _ARG_PTR(n, t, ap) ((t *)(ap + n))
#define _ARG_PTR_FAR(n, t, ap) // unimplemented, will create syntax error
#define _ARG_R(t) t
#define _RET(r) r
#define _RET_PTR(r) // unused

uint32_t DJ64_DISPATCH_FN(int fn, uint8_t *sp, enum DispStat *r_stat,
    int *r_len)
{
    UDWORD ret;
    UBYTE rsz = 0;

#define _SP sp
#define _DISPATCH(r, rv, rc, f, ...) { \
    rv _r = f(__VA_ARGS__); \
    ret = rc(_r); \
    rsz = (r); \
}

    switch (fn) {
        #include <thunk_calls.h>

        default:
            fdprintf("unknown fn %i\n", fn);
            _fail();
            return 0;
    }

    *r_stat = 0;
    *r_len = rsz;
    return ret;
}
