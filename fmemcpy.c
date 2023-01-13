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

void fmemcpy1(unsigned dst_sel, unsigned dst_off, const void *src,
    unsigned len)
{
    int rc;
    unsigned long base;
    void *ptr;

    rc = __dpmi_get_segment_base_address(dst_sel, &base);
    assert(!rc);
    rc = __djgpp_nearptr_enable();
    assert(rc);
    ptr = (void *)(base + dst_off + __djgpp_conventional_base);
    memcpy(ptr, src, len);
    __djgpp_nearptr_disable();
}

void fmemcpy2(void *dst, unsigned src_sel, unsigned src_off, unsigned len)
{
    int rc;
    unsigned long base;
    const void *ptr;

    rc = __dpmi_get_segment_base_address(src_sel, &base);
    assert(!rc);
    rc = __djgpp_nearptr_enable();
    assert(rc);
    ptr = (const void *)(base + src_off + __djgpp_conventional_base);
    memcpy(dst, ptr, len);
    __djgpp_nearptr_disable();
}

/* similar to sys/movedata.h's movedata(), but the src/dst swapped! */
void fmemcpy12(unsigned dst_sel, unsigned dst_off, unsigned src_sel,
    unsigned src_off, unsigned len)
{
    int rc;
    unsigned long sbase, dbase;
    const void *sptr;
    void *dptr;

    rc = __dpmi_get_segment_base_address(src_sel, &sbase);
    assert(!rc);
    rc = __dpmi_get_segment_base_address(dst_sel, &dbase);
    assert(!rc);
    rc = __djgpp_nearptr_enable();
    assert(rc);
    sptr = (const void *)(sbase + src_off + __djgpp_conventional_base);
    dptr = (void *)(dbase + dst_off + __djgpp_conventional_base);
    memcpy(dptr, sptr, len);
    __djgpp_nearptr_disable();
}
