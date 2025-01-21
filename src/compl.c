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
#include <assert.h>
#include "command.h"
#include "cmdbuf.h"
#include "compl.h"

struct compl_s {
    void *opaque;
    const char *(*get_name)(int idx, void *arg);
    int num;
};

#define MAX_COMPLS 10
struct cmpl_s {
    int num;
    struct compl_s compls[MAX_COMPLS];
};

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
    assert(idx < CMD_TABLE_COUNT);
    return cmd[idx].cmd_name;
}

static const char *get_fname(int idx, void *arg)
{
    glob_t *gl = arg;
    assert(idx < gl->gl_pathc);
    return gl->gl_pathv[idx];
}

static const char *get_compl_name(int idx, void *arg)
{
    struct cmpl_s *cmpl = arg;
    int i;

    for (i = 0; i < cmpl->num; i++) {
	struct compl_s *c = &cmpl->compls[i];
	if (idx >= c->num) {
	    idx -= c->num;
	    continue;
	}
	return c->get_name(idx, c->opaque);
    }
    return NULL;
}

static int do_compl(const char *prefix, int print, int *r_len,
		    char *r_p, const char *(*get)(int idx, void *arg),
		    void *arg, int num)
{
    int i, cnt = 0, idx = -1, len = strlen(prefix);
    char suff[MAX_CMD_BUFLEN] = "";

    for (i = 0; i < num; i++) {
	const char *c = get(i, arg);
	/* Note: even though strncasecmp() is used, file-name completions
	 * are still case-sensitive because the completion candidates are
	 * added via glob() fn, which is case-sensitive. */
	if (strncasecmp(prefix, c, len) == 0) {
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

static void glb_add(struct cmpl_s *cmpl, glob_t *gl)
{
    struct compl_s *c = &cmpl->compls[cmpl->num++];

    c->opaque = gl;
    c->get_name = get_fname;
    c->num = gl->gl_pathc;
}

int compl_cmds(const char *prefix, int print, int *r_len, char *r_p)
{
    char buf[MAXPATH];
    struct cmpl_s cmpl = { };
    glob_t gl_bat, gl_exe, gl_com;
    int err, ret = -1, cnt = 0;
    const char *p;
    const char *suff = ((p = strchr(prefix, '.')) ? "" : "*.");

    if (p && p[1] != '\0')
	return compl_fname(prefix, print, r_len, r_p);
    snprintf(buf, MAXPATH, "%s%sbat", prefix, suff);
    err = glob(buf, GLOB_ERR, NULL, &gl_bat);
    if (err && err != GLOB_NOMATCH)
	return -1;
    if (!err) {
	glb_add(&cmpl, &gl_bat);
	cnt += gl_bat.gl_pathc;
    }
    snprintf(buf, MAXPATH, "%s%sexe", prefix, suff);
    err = glob(buf, GLOB_ERR, NULL, &gl_exe);
    if (err && err != GLOB_NOMATCH)
	goto err1;
    if (!err) {
	glb_add(&cmpl, &gl_exe);
	cnt += gl_exe.gl_pathc;
    }
    snprintf(buf, MAXPATH, "%s%scom", prefix, suff);
    err = glob(buf, GLOB_ERR, NULL, &gl_com);
    if (err && err != GLOB_NOMATCH)
	goto err2;
    if (!err) {
	glb_add(&cmpl, &gl_com);
	cnt += gl_com.gl_pathc;
    }

    if (!p) {
	cmpl.compls[cmpl.num].opaque = cmd_table;
	cmpl.compls[cmpl.num].get_name = get_cmd_name;
	cmpl.compls[cmpl.num].num = CMD_TABLE_COUNT;
	cmpl.num++;
	cnt += CMD_TABLE_COUNT;
    }

    ret = do_compl(prefix, print, r_len, r_p, get_compl_name, &cmpl, cnt);

    globfree(&gl_com);
  err2:
    globfree(&gl_exe);
  err1:
    globfree(&gl_bat);

    return ret;
}

int compl_fname(const char *prefix, int print, int *r_len, char *r_p)
{
    char buf[MAXPATH];
    glob_t gl;
    int err, ret;

    snprintf(buf, MAXPATH, "%s*", prefix);
    err = glob(buf, GLOB_ERR, NULL, &gl);
    if (err) {
	/* Try simplest case-insensitive match against all-upcased.
	 * There can be quake and Quake and QuAkE dirs simultaneously
	 * and we aren't going to iterate those. */
	strupr(buf);
	err = glob(buf, GLOB_ERR, NULL, &gl);
    }
    if (err)
	return -1;
    ret = do_compl(prefix, print, r_len, r_p, get_fname, &gl, gl.gl_pathc);
    globfree(&gl);
    return ret;
}
