#ifndef __CMDBUF_H__
#define __CMDBUF_H__

#define LEFT  (0xFFFFFFFF)
#define RIGHT (0x00000001)

unsigned int cmdbuf_get_tail(void);
void cmdbuf_move(unsigned char *cmd_buf, int direction);
void cmdbuf_delch(unsigned char *cmd_buf);
void cmdbuf_putch(unsigned char *cmd_buf, unsigned int buf_size, char ch, unsigned short flag);
unsigned char *cmdbuf_gets(unsigned char *cmd_buf);

#endif
