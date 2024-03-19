/* Command-line buffer handling for FreeDOS-32's COMMAND
 *
 * Copyright (C) 2005 by Hanzac Chen
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <conio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dir.h>
#include "cmdbuf.h"

#ifdef __MINGW32__
#define cputs(s) _cputs(s)
#endif

#define KEYB_FLAG_INSERT    0x0080
#define KEY_ASCII(k)    (k & 0x00FF)
#define KEY_BACKSPACE    KEY_ASCII(0x0E08)

static unsigned int tail = 0;
static unsigned int cur = 0;

#define MAX_CMDQUEUE_LEN 0x10
static char cmdqueue[MAX_CMDQUEUE_LEN][MAX_CMD_BUFLEN];
static unsigned int cmdqueue_count = 0;
static unsigned int cmdqueue_index = 0;
static const char *hist_name = "cc.his";

static void _cmdbuf_clr_line(char *cmd_buf)
{
	unsigned int i, n;
	/* Clear the original command */
	for (i = 0, n = tail; i < n; i++) {
		putch(KEY_ASCII(KEY_BACKSPACE));
		cur--;
		cmdbuf_delch(cmd_buf);
	}
}

int cmdbuf_move(char *cmd_buf, int direction)
{
  int ret = 0;
  switch (direction)
  {
    case UP:
      if (cmdqueue_index && cmdqueue[cmdqueue_index - 1][0] != '\0')
        {
        cmdqueue_index--;
        ret++;
        }
      break;
    case LEFT:
      if (cur != 0) {
        putch(KEY_ASCII(KEY_BACKSPACE));
        cur--;
        ret++;
      }
      break;
    case RIGHT:
      if (cur < tail) {
        putch(cmd_buf[cur]);
        cur++;
        ret++;
      }
      break;
    case DOWN:
      if (cmdqueue[cmdqueue_index][0] == '\0' ||
          cmdqueue_index == cmdqueue_count)
        break;
      cmdqueue_index++;
      ret++;
      break;
    case HOME:
      while (cur != 0) {
        putch(KEY_ASCII(KEY_BACKSPACE));
        cur--;
        ret++;
      }
      break;
    case END:
      while (cur < tail) {
        putch(cmd_buf[cur]);
        cur++;
        ret++;
      }
      break;
    case PGUP:
      if (cmdqueue_index && cmdqueue[0][0] != '\0') {
        cmdqueue_index = 0;
        direction = UP;
        ret++;
      }
      break;
    case PGDN:
      if (cmdqueue_index < cmdqueue_count) {
        cmdqueue_index = cmdqueue_count;
        direction = DOWN;
        ret++;
      }
      break;
  }

  if (direction == UP || direction == DOWN) {
    _cmdbuf_clr_line(cmd_buf);
    if (cmdqueue[cmdqueue_index][0] != '\0') {
      /* Reinput the command from the queue */
      cputs(cmdqueue[cmdqueue_index]);
      strcpy(cmd_buf, cmdqueue[cmdqueue_index]);
      cur = tail = strlen(cmdqueue[cmdqueue_index]);
    }
  }

  return ret;
}

void cmdbuf_delch(char *cmd_buf)
{
  if (cur < tail) {
    unsigned int i;
    cmd_buf[cur] = 0;
    cmd_buf[tail] = 0;

    /* Move the left string to the current position */
    for (i = cur; i < tail-1; i++)
    {
      putch(cmd_buf[i+1]);
      cmd_buf[i] = cmd_buf[i+1];
    }
    putch(' ');

    /* Put cursor back to the current position */
    for (i = cur; i < tail; i++)
      putch(KEY_ASCII(KEY_BACKSPACE));

    /* Subtract the string 1 */
    tail--;
  }
}

int cmdbuf_bksp(char *cmd_buf)
{
  if (cur == 0)
    return 0;
  cur--;
  if (cur == tail - 1)
    {
    tail--;
    return 1;
    }
  putch(KEY_ASCII(KEY_BACKSPACE));
  cmdbuf_delch(cmd_buf);
  return 0;
}

void cmdbuf_clear(char *cmd_buf)
{
    _cmdbuf_clr_line(cmd_buf);
}

char cmdbuf_putch(char *cmd_buf, unsigned int buf_size, char ch, unsigned short flag)
{
  unsigned int i;

  if (cur < buf_size) {
    /* Reflect the insert method */
    if (!(flag&KEYB_FLAG_INSERT)) {
      for (i = tail; i > cur; i--)
        cmd_buf[i] = cmd_buf[i-1];
    }
    /* Put into cmdline buffer */
    cmd_buf[cur++] = ch;
    if ((!(flag&KEYB_FLAG_INSERT) && tail < buf_size) || cur > tail)
      tail++;
    /* Update the string on screen */
    for (i = cur-1; i < tail-1; i++)
      putch(cmd_buf[i]);
    if (cur == tail)
      return cmd_buf[tail - 1];
    putch(cmd_buf[tail - 1]);

    /* Put cursor back to the current position */
    for (i = cur; i < tail; i++)
      putch(KEY_ASCII(KEY_BACKSPACE));
  }
  return 0;
}

void cmdbuf_reset(void)
{
  cmdqueue_index = cmdqueue_count;
  cur = tail = 0;
}

void cmdbuf_trunc(char *cmd_buf)
{
  cmd_buf[tail] = '\0';
}

void cmdbuf_puts(const char *cmd_buf)
{
  cur = tail = strlen(cmd_buf);
}

void cmdbuf_eol(void)
{
  /* Reset the cmdbuf */
  cur = tail = 0;
}

void cmdbuf_store_tmp(const char *cmd_buf)
{
  strcpy(cmdqueue[cmdqueue_index], cmd_buf);
}

void cmdbuf_store(const char *cmd_buf)
{
  if (cmd_buf[0] == '\0')
    return;
  if (!cmdqueue_count || strcmp(cmd_buf, cmdqueue[cmdqueue_count - 1]) != 0)
    {
    const char *tmp;
    /* Enqueue the cmdbuf and save the current index */
    strcpy(cmdqueue[cmdqueue_count], cmd_buf);
    cmdqueue_count++;
    if (cmdqueue_count == MAX_CMDQUEUE_LEN)
      {
      int i;
      for (i = 1; i < cmdqueue_count; i++)
        strcpy(cmdqueue[i - 1], cmdqueue[i]);
      cmdqueue_count--;
      cmdqueue[cmdqueue_count][0] = '\0';
      }
    tmp = getenv("TEMP");
    if (tmp)
      {
      char pathbuf[MAXPATH];
      FILE *his;
      snprintf(pathbuf, MAXPATH, "%s\\%s", tmp, hist_name);
      his = fopen(pathbuf, "a");
      if (his)
        {
        fputs(cmd_buf, his);
        fputs("\n", his);  // actually puts \r\n
        fclose(his);
        }
      }
    }
  cmdqueue_index = cmdqueue_count;
}

static int count_lines(FILE *f)
{
  int c;
  int cnt = 0;
  while ((c = fgetc(f)) != EOF)
    {
    if (c == '\n')
      cnt++;
    }
  rewind(f);
  return cnt;
}

static int seek_to_line(FILE *f, int n)
{
  int c;
  int cnt = 0;
  if (!n)
    return 0;
  while ((c = fgetc(f)) != EOF)
    {
    if (c == '\n')
      cnt++;
    if (cnt == n)
      return 0;
    }
  rewind(f);
  return -1;
}

void cmdbuf_init(void)
{
  const char *tmp = getenv("TEMP");
  if (tmp)
    {
    char pathbuf[MAXPATH];
    FILE *his;
    snprintf(pathbuf, MAXPATH, "%s\\%s", tmp, hist_name);
    his = fopen(pathbuf, "r");
    if (his)
      {
      int cnt = count_lines(his);
      int cnt1 = cnt;
      /* always leave 1 empty slot */
      if (cnt > (MAX_CMDQUEUE_LEN - 1))
        {
        seek_to_line(his, cnt - (MAX_CMDQUEUE_LEN - 1));
        cnt = (MAX_CMDQUEUE_LEN - 1);
        }
      for (cmdqueue_count = 0; cmdqueue_count < cnt; cmdqueue_count++)
        {
        char *got = fgets(cmdqueue[cmdqueue_count], MAX_CMD_BUFLEN, his);
        if (!got)
          break;
        /* strip \n */
        cmdqueue[cmdqueue_count][strlen(cmdqueue[cmdqueue_count]) - 1] = '\0';
        }
      fclose(his);
      cmdqueue_index = cmdqueue_count;
      /* if history is too long, rewrite the file */
      if (cnt1 > cnt)
        {
        his = fopen(pathbuf, "w");
        if (his)
          {
          int i;
          for (i = 0; i < cnt; i++)
            {
            fputs(cmdqueue[i], his);
            fputs("\n", his);  // actually puts \r\n
            }
          fclose(his);
          }
        }
      }
    }
}
