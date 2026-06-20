/*
 *  Copyright (C) 2023  stsp
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
#include <sys/nearptr.h>
#include <dpmi.h>
#include <string.h>
#include <assert.h>
#include "fmemcpy.h"

static inline int get_segment_base_address(int selector, unsigned *addr)
{
#ifdef __LP64__
    return __dpmi_get_segment_base_address(selector, addr);
#else
    return __dpmi_get_segment_base_address(selector, (unsigned long *)addr);
#endif
}

void fmemcpy1(__dpmi_paddr dst, const void *src, unsigned len)
{
    int rc;
    unsigned base;
    void *ptr;

    rc = get_segment_base_address(dst.selector, &base);
    assert(!rc);
    rc = __djgpp_nearptr_enable();
    assert(rc);
    ptr = (void *)(base + dst.offset32 + __djgpp_conventional_base);
    memcpy(ptr, src, len);
    __djgpp_nearptr_disable();
}

void fmemcpy2(void *dst, __dpmi_paddr src, unsigned len)
{
    int rc;
    unsigned base;
    const void *ptr;

    rc = get_segment_base_address(src.selector, &base);
    assert(!rc);
    rc = __djgpp_nearptr_enable();
    assert(rc);
    ptr = (const void *)(base + src.offset32 + __djgpp_conventional_base);
    memcpy(dst, ptr, len);
    __djgpp_nearptr_disable();
}

/* similar to sys/movedata.h's movedata(), but the src/dst swapped! */
void fmemcpy12(__dpmi_paddr dst, __dpmi_paddr src, unsigned len)
{
    int rc;
    unsigned sbase, dbase;
    const void *sptr;
    void *dptr;

    rc = get_segment_base_address(src.selector, &sbase);
    assert(!rc);
    rc = get_segment_base_address(dst.selector, &dbase);
    assert(!rc);
    rc = __djgpp_nearptr_enable();
    assert(rc);
    sptr = (const void *)(sbase + src.offset32 + __djgpp_conventional_base);
    dptr = (void *)(dbase + dst.offset32 + __djgpp_conventional_base);
    memcpy(dptr, sptr, len);
    __djgpp_nearptr_disable();
}
