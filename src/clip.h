#ifndef CLIP_H
#define CLIP_H

int clip_read(int type, void (*cbk)(const char *buf, int len));
int clip_write(int type, int (*cbk)(char *buf, int len));

#endif
