#ifndef __CMDBUF_H__
#define __CMDBUF_H__

/*
 * Command parser defines
 */
#define MAX_CMD_BUFLEN 128        // Define max command length

#define UP    (0x00000000)
#define LEFT  (0x00000001)
#define RIGHT (0x00000002)
#define DOWN  (0x00000003)

unsigned int cmdbuf_get_tail(void);
void cmdbuf_move(char *cmd_buf, int direction);
void cmdbuf_delch(char *cmd_buf);
void cmdbuf_putch(char *cmd_buf, unsigned int buf_size, char ch, unsigned short flag);
char *cmdbuf_gets(char *cmd_buf);

#endif
