#ifndef __CMDBUF_H__
#define __CMDBUF_H__

/*
 * Command parser defines
 */
#define MAX_CMD_BUFLEN 512        // Define max command length

enum { UP, LEFT, RIGHT, DOWN, HOME, END };

int cmdbuf_move(char *cmd_buf, int direction);
void cmdbuf_delch(char *cmd_buf);
int cmdbuf_bksp(char *cmd_buf);
void cmdbuf_clear(char *cmd_buf);
char cmdbuf_putch(char *cmd_buf, unsigned int buf_size, char ch, unsigned short flag);
void cmdbuf_store(char *cmd_buf);
void cmdbuf_reset(void);
void cmdbuf_init(void);

#endif
