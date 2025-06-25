/*
 *  comcom64 - 64bit command.com
 *  env.c: environment handling routines
 *  Copyright (C) 2023-2024  @stsp
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
#include <stdlib.h>
#include <string.h>
#ifdef DJ64
#include <sys/fmemcpy.h>
#else
#include "fmemcpy.h"
#include "memmem.h"
#endif
#include <stubinfo.h>
#include <sys/movedata.h>
#include "command.h"
#include "umb.h"
#include "env.h"

#define DP(s, o) (__dpmi_paddr){ .selector = s, .offset32 = o, }

extern char **environ;

static unsigned short env_selector;
static unsigned short env_segment;
static unsigned short env_size;

struct MCB {
        char id;                        /* 0 */
        unsigned short owner_psp;       /* 1 */
        unsigned short size;            /* 3 */
        char align8[3];                 /* 5 */
        char name[8];                   /* 8 */
} __attribute__((packed));

void set_env_seg(void)
{
  unsigned short psp = _stubinfo->psp_selector;
  fmemcpy1(DP(psp, 0x2c), &env_segment, 2);
}

void set_env_sel(void)
{
  unsigned short psp = _stubinfo->psp_selector;
  fmemcpy1(DP(psp, 0x2c), &env_selector, 2);
}

void set_env_size(void)
{
  unsigned short psp = _stubinfo->psp_selector;
  unsigned short env_sel;
  unsigned env_addr;
  struct MCB mcb;
  unsigned old_env_size;
  int err;

  fmemcpy2(&env_sel, DP(psp, 0x2c), 2);
  err = get_segment_base_address(env_sel, &env_addr);
  old_env_size = __dpmi_get_segment_limit(env_sel) + 1;
  env_size = old_env_size;
  if (!err && !(env_addr & 0xf) && env_addr < 0x110000 && old_env_size == 0x10000) {
    dosmemget(env_addr - sizeof(mcb), sizeof(mcb), &mcb);
    env_size = mcb.size * 16;
    __dpmi_set_segment_limit(env_sel, env_size - 1);
  }

  env_selector = env_sel;
  env_segment = env_addr >> 4;
}

void get_env(void)
{
  char *dos_environ = alloca(env_size);
  char *cp;

  fmemcpy2(dos_environ, DP(env_selector, 0), env_size);
  dos_environ[env_size] = 0;
  cp = dos_environ;
  do {
    if (*cp) {
      char *env = strdup(cp);
      putenv(env);
      cp += strlen(env);
    }
    cp++; /* skip to next character */
  } while (*cp); /* repeat until two NULs */
}

/* this function replaces RM env (pointed to with env_sel) with
 * PM env (environ[]), leaving tail intact */
static void _put_env(unsigned short env_sel)
{
  int env_count;
  int env_offs = 0;
  char *env;
  char *tail;
  int tail_sz = 3;

  env = alloca(env_size + tail_sz);
  /* back up full env, just for getting its tail */
  fmemcpy2(env, DP(env_sel, 0), env_size);
  memset(&env[env_size], 0, tail_sz);
  tail = memchr(env, 1, env_size);
  if (tail && tail[1] == '\0') {
    tail_sz += strlen(tail + 2) + 1;
    tail--;
  } else {
    tail = memmem(env, env_size, "\x0\x0", 2);
    if (!tail) {
      printf("ENV block corrupted\n");
      return;
    }
    tail++;
    if (tail - env + tail_sz > env_size || memcmp(tail, "\x0\x0\x0", 3) != 0)
      tail_sz = 1;  /* DOS-2.0 terminator */
  }
  /* now put entire environ[] down, overwriting prev content */
  for (env_count = 0; environ[env_count]; env_count++) {
    int l = strlen(environ[env_count]) + 1;
    if (env_offs + l >= env_size - tail_sz) {
      printf("ENV buffer overflow (size %u, need %u, tail %i)\n",
          env_size, env_offs + l, tail_sz);
      break;
    }
    fmemcpy1(DP(env_sel, env_offs), environ[env_count], l);
    env_offs += l;
  }
  /* and preserve tail */
  if (env_offs + tail_sz <= env_size)
    fmemcpy1(DP(env_sel, env_offs), tail, tail_sz);
}

void put_env(void)
{
  _put_env(env_selector);
}

#if !SYNC_ENV
static void _set_env(const char *variable, const char *value,
    unsigned short env_sel, unsigned env_size)
{
  char *env;
  char *tail;
  char *cp;
  char *env2;
  int l;
  int len;
  int tail_sz = 3;

  /* allocate tmp buffer for env and copy them there */
  env = alloca(env_size + tail_sz);
  fmemcpy2(env, DP(env_sel, 0), env_size);
  memset(&env[env_size], 0, tail_sz);
  cp = env2 = env;
  l = strlen(variable);
  /*
     Delete any existing variable with the name (var).
  */
  while (*env2 && (env2 - env) < env_size) {
    if ((strncmp(variable, env2, l) == 0) && (env2[l] == '=')) {
      cp = env2 + strlen(env2) + 1;
      memmove(env2, cp, env_size - (cp - env));
    } else {
      env2 += strlen(env2) + 1;
    }
  }

  tail = env2;
  cp = tail + 1;
  if (cp[0] == '\1' && cp[1] == '\0')
    tail_sz += strlen(cp + 2) + 1;

  /*
     If the variable fits, shovel it in at the end of the envrionment.
  */
  len = l + (value ? strlen(value) : 0) + 2;
  if (value && value[0] && (env_size - (env2 - env) - tail_sz >= len)) {
    memmove(env2 + len, env2, tail_sz);
    strcpy(env2, variable);
    strcat(env2, "=");
    strcat(env2, value);
  }

  /* now put it back */
  fmemcpy1(DP(env_sel, 0), env, env_size);
}

void set_env(const char *variable, const char *value)
{
  _set_env(variable, value, env_selector, env_size);
}

void sync_env(void)
{
  unsigned short sel;
  unsigned short psp = _stubinfo->psp_selector;
  fmemcpy2(&sel, DP(psp, 0x2c), 2);
  _put_env(sel);
}
#endif

int realloc_env(unsigned new_size)
{
  int seg, sel = 0;
  unsigned int old_size = env_size;

  link_umb(0x80);
  seg = __dpmi_allocate_dos_memory(new_size >> 4, &sel);
  unlink_umb();
  if (seg != -1) {
    unsigned short psp = _stubinfo->psp_selector;
    fmemcpy1(DP(psp, 0x2c), &sel, 2);
    /* copy old content to preserve tail (and maybe COMSPEC) */
    fmemcpy12(DP(sel, 0), DP(env_selector, 0), old_size);
    __dpmi_free_dos_memory(env_selector);
    env_selector = sel;
    env_segment = seg;
    env_size = new_size;
  } else {
    printf("ERROR: env allocation of %i bytes failed!\n", env_size);
    return -1;
  }
  return 0;
}

int get_env_size(void)
{
  return env_size;
}
