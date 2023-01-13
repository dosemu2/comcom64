#ifndef FMEMCPY_H
#define FMEMCPY_H

void fmemcpy1(unsigned dst_sel, unsigned dst_off, const void *src,
    unsigned len);
void fmemcpy2(void *dst, unsigned src_sel, unsigned src_off, unsigned len);
void fmemcpy12(unsigned dst_sel, unsigned dst_off, unsigned src_sel,
    unsigned src_off, unsigned len);

#endif
