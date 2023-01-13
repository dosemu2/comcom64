#ifndef FMEMCPY_H
#define FMEMCPY_H

#include <dpmi.h>

void fmemcpy1(__dpmi_paddr dst, const void *src, unsigned len);
void fmemcpy2(void *dst, __dpmi_paddr src, unsigned len);
void fmemcpy12(__dpmi_paddr dst, __dpmi_paddr src, unsigned len);

#endif
