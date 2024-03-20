/*
 *  comcom64 - 64bit command.com
 *  compl.c: command completion machinery
 *  Copyright (C) 2024  @stsp
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
#include <string.h>
#include <glob.h>
#include "command.h"
#include "cmdbuf.h"
#include "compl.h"

static int cmpstr(const char *s1, const char *s2)
{
  int cnt = 0;
  while (s1[cnt] && s2[cnt] && s1[cnt] == s2[cnt])
    cnt++;
  return cnt;
}

static const char *get_cmd_name(int idx, void *arg)
{
  struct built_in_cmd *cmd = arg;
  return cmd[idx].cmd_name;
}

static const char *get_fname(int idx, void *arg)
{
  glob_t *gl = arg;
  if (idx >= gl->gl_pathc)
   return NULL;
  return gl->gl_pathv[idx];
}

static int do_compl(const char *prefix, int print, int *r_len,
    char *r_p, const char *(*get)(int idx, void *arg), void *arg,
    int num)
  {
  int i, cnt = 0, idx = -1, len = strlen(prefix);
  char suff[MAX_CMD_BUFLEN] = "";

  for (i = 0; i < num; i++)
    {
    const char *c = get(i, arg);
    if (strncmp(prefix, c, len) == 0)
      {
      const char *p = c + len;
      int l = cmpstr(p, suff);

      strcpy(suff, p);
      if (cnt)
        suff[l] = '\0';
      cnt++;
      idx = i;
      if (print)
        puts(c);
      }
    }
  if (cnt == 0)
    return -1;
  *r_len = strlen(suff);
  strcpy(r_p, get(idx, arg) + len);
  if (cnt == 1)
    return 1;
  return 0;
  }

int compl_cmds(const char *prefix, int print, int *r_len, char *r_p)
  {
  return do_compl(prefix, print, r_len, r_p, get_cmd_name, cmd_table,
      CMD_TABLE_COUNT);
  }

int compl_fname(const char *prefix, int print, int *r_len, char *r_p)
  {
  char buf[MAXPATH];
  glob_t gl;
  int err, ret;

  snprintf(buf, MAXPATH, "%s*", prefix);
  err = glob(buf, GLOB_ERR, NULL, &gl);
  if (err)
    return -1;
  ret = do_compl(prefix, print, r_len, r_p, get_fname, &gl, gl.gl_pathc);
  globfree(&gl);
  return ret;
  }
