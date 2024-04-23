/*
 *  COMMAND.COM-compatible command processor for DOS.
 *
 *  Copyright (C) 1997, CENTROID CORPORATION, HOWARD, PA 16841
 *  Copyright (C) Allen S. Cheung (RIP)
 *  Copyright (C) 2005-2006 FreeDOS-32 project
 *  Copyright (C) 2018-2024 stsp, dosemu2 project
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

/*
 * comcom32 project note:
 * The use of GPLv3+ license for this code was confirmed by Centroid Corp here:
 * https://github.com/stsp/comcom32/issues/12#issuecomment-472004939
 *
 * See also original copyrights below.
 */

/*
*   FILE NAME:
*       COMMAND.C
*
*   PROGRAMMER(S):
*       Allen S. Cheung (allencheung@fastmail.ca)
*
*   UPDATE:
*       15-Apr-2002
*
*   LICENSE:
*       GNU General Public License
*
*   COPYRIGHT (C) 1997  CENTROID CORPORATION, HOWARD, PA 16841
*
***/

/* WARNING:	This is not the original version.
 *			modified for FreeDOS-32 by Salvo Isaja and Hanzac Chen
 */

#include <dos.h>
#include <libc/dosio.h>
#include <io.h>
#include <libc/getdinfo.h>
#include <time.h>
#include <glob.h>
#include <utime.h>
#include <conio.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <stubinfo.h>
#include <process.h>
#include <sys/segments.h>
#include <sys/farptr.h>
#include <go32.h>

#include "cmdbuf.h"
#ifdef DJ64
#include <sys/fmemcpy.h>
#else
#include "fmemcpy.h"
#endif
#include "asm.h"
#include "version.h"
#include "ms.h"
#include "env.h"
#include "psp.h"
#include "umb.h"
#include "ae0x.h"
#include "compl.h"
#include "command.h"

/*
 * These declarations/definitions turn off some unwanted DJGPP features
 */

extern char **environ;
#include <crt0.h>

int _crt0_startup_flags =
       _CRT0_FLAG_USE_DOS_SLASHES |          // keep the backslashes
       _CRT0_FLAG_DISALLOW_RESPONSE_FILES |  // no response files (i.e. `@gcc.rf')
//       _CRT0_FLAG_NO_LFN |                   // disable long file names
       _CRT0_FLAG_PRESERVE_FILENAME_CASE;    // keep DOS names non-lowercased

static const char *version = "0.3";

#define DP(s, o) (__dpmi_paddr){ .selector = s, .offset32 = o, }

static int shell_mode = SHELL_NORMAL;
static int shell_permanent;
static int stepping;
static int mouse_en;
static int mouseopt_extctl;
static int mouseopt_enabled;

#define DEBUG 0

static __dpmi_raddr int0_vec;
static int int0_wa;

/*
 * Command parser defines/variables
 */
static char cmd_line[MAX_CMD_BUFLEN] = ""; // when this string is not "" it triggers command execution
static char cmd[MAX_CMD_BUFLEN] = "";
static char cmd_arg[MAX_CMD_BUFLEN] = "";
static char cmd_switch[MAX_CMD_BUFLEN] = "";
static char cmd_args[MAX_CMD_BUFLEN] = "";
static char goto_label[MAX_CMD_BUFLEN] = "";

/*
 * Pipe variables and defines
 */
static char pipe_file[2][MAX_CMD_BUFLEN] = {"",""};
static int pipe_file_redir_count[2];
static char pipe_to_cmd[MAX_CMD_BUFLEN] = "";
static int pipe_to_cmd_redir_count;
static FILE *bkp_stdin;

/*
 * Command interpreter/executor defines/variables
 */
#define MAX_STACK_LEVEL        20 // Max number of batch file call stack levels
#define MAX_BAT_ARGS           32 // Max number of batch file arguments

static int need_to_crlf_at_next_prompt;
static int stack_level = 0;
static int echo_on[MAX_STACK_LEVEL];
static char bat_file_path[MAX_STACK_LEVEL][FILENAME_MAX];  // when this string is not "" it triggers batch file execution
static char bat_arg[MAX_STACK_LEVEL][MAX_BAT_ARGS][MAX_CMD_BUFLEN];
static int bat_file_line_number[MAX_STACK_LEVEL];
static char pushd_stack[MAX_STACK_LEVEL][MAXPATH];
static int pushd_stack_level = 0;
static unsigned error_level = 0;  // Program execution return code
static char for_var;
static const char *for_val;
static int exiting;
static int break_on;
static int break_enabled;

/*
 * File attribute constants
 */
static const char attrib_letters[4] = {'R', 'A', 'S', 'H'};
static const unsigned attrib_values[4] = {_A_RDONLY, _A_ARCH, _A_SYSTEM, _A_HIDDEN};

/*
 * Some private prototypes
 */
static void perform_attrib(const char *arg);
static void perform_break(const char *arg);
static void perform_call(const char *arg);
static void perform_cd(const char *arg);
static void perform_choice(const char *arg);
static void perform_cls(const char *arg);
static void perform_copy(const char *arg);
static void perform_ctty(const char *arg);
static void perform_date(const char *arg);
static void perform_delete(const char *arg);
static void perform_deltree(const char *arg);
static void perform_dir(const char *arg);
static void perform_echo_dot(const char *arg);
static void perform_echo(const char *arg);
static void perform_exit(const char *arg);
static void perform_for(const char *arg);
static void perform_goto(const char *arg);
static void perform_help(const char *arg);
static void perform_loadhigh(const char *arg);
static void perform_license(const char *arg);
static void perform_loadfix(const char *arg);
static void perform_md(const char *arg);
static void perform_move(const char *arg);
static void perform_more(const char *arg);
static void perform_mouseopt(const char *arg);
static void perform_path(const char *arg);
static void perform_pause(const char *arg);
static void perform_popd(const char *arg);
static void perform_prompt(const char *arg);
static void perform_pushd(const char *arg);
static void perform_r200fix(const char *arg);
static void perform_rd(const char *arg);
static void perform_rename(const char *arg);
static void perform_shift(const char *arg);
static void perform_time(const char *arg);
static void perform_timeout(const char *arg);
static void perform_type(const char *arg);
static void perform_ver(const char *arg);
static void perform_xcopy(const char *arg);
static void parse_cmd_line(void);
static void perform_external_cmd(int call, int lh, char *ext_cmd);
static void exec_cmd(int call);
static void perform_set(const char *arg);

static void list_cmds(void);
//static void perform_unimplemented_cmd(void);
static void set_break(int on);

struct built_in_cmd cmd_table[] =
  {
    {"attrib", perform_attrib, "", "set file attributes"},
    {"break", perform_break, "", "set ^Break handling"},
    {"call", perform_call, "", "call batch file"},
    {"cd", perform_cd, "", "change directory"},
    {"chdir", perform_cd, "", "change directory"},
    {"choice", perform_choice, "", "choice prompt sets ERRORLEVEL"},
    {"cls", perform_cls, "", "clear screen"},
    {"copy", perform_copy, "", "copy file"},
    {"ctty", perform_ctty, "", "change tty"},
    {"date", perform_date, "", "display date"},
    {"del", perform_delete, "", "delete file"},
    {"deltree", perform_deltree, "", "delete directory recursively"},
    {"erase", perform_delete, "", "delete file"},
    {"dir", perform_dir, "", "directory listing"},
    {"echo.", perform_echo_dot, "", "terminal output"},  // before normal echo
    {"echo", perform_echo, "", "terminal output"},
    {"exit", perform_exit, "", "exit from interpreter"},
    {"for", perform_for, "", "FOR loop"},
    {"goto", perform_goto, "", "move to label"},
    {"help", perform_help, "", "display this help"},
    {"lh", perform_loadhigh, "", "load program to UMB"},
    {"license", perform_license, "", "show copyright information"},
    {"loadfix", perform_loadfix, "", "fix \"packed file is corrupt\""},
    {"loadhigh", perform_loadhigh, "", "load program to UMB"},
    {"md", perform_md, "", "create directory"},
    {"mkdir", perform_md, "", "create directory"},
    {"move", perform_move, "", "move file"},
    {"more", perform_more, "", "scroll-pause long output"},
    {"mouseopt", perform_mouseopt, "", "mouse options"},
    {"path", perform_path, "", "set search path"},
    {"pause", perform_pause, "", "wait for a keypress"},
    {"popd", perform_popd, "", "pop dir from stack and cd"},
    {"prompt", perform_prompt, "", "customize prompt string"},
    {"pushd", perform_pushd, "", "push cwd to stack and cd"},
    {"r200fix", perform_r200fix, "", "runtime error 200 fix"},
    {"rd", perform_rd, "", "remove directory"},
    {"rmdir", perform_rd, "", "remove directory"},
    {"rename", perform_rename, "", "rename with wildcards"},
    {"ren", perform_rename, "", "rename with wildcards"},
    {"set", perform_set, "", "set/unset environment variables"},
    {"shift", perform_shift, "", "shift arguments"},
    {"time", perform_time, "", "display time"},
    {"timeout", perform_timeout, "", "pause execution"},
    {"type", perform_type, "", "display file content"},
    {"ver", perform_ver, " [/r]", "display version"},
    {"xcopy", perform_xcopy, "", "copy large file"},
  };

/*
 * Count of the number of valid commands
 */
const int CMD_TABLE_COUNT = sizeof(cmd_table) / sizeof(struct built_in_cmd);

/***
*
*   FUNCTION:    conv_unix_path_to_ms_dos
*
*   PROGRAMMER:  Allen S. Cheung
*
*   UPDATE:      04-Jan-2001
*
*   PURPOSE:     Force the given filepath into MS-DOS style
*
*   CALL:        void conv_unix_path_to_ms_dos(char *path)
*
*   WHERE:       *path is a file path either in Unix or MS-DOS style,
*                    which will be converted to MS-dos style if
*                    it was in Unix format.  (I/R)
*
*   RETURN:       none
*
*   NOTES:
*
***/
static void conv_unix_path_to_ms_dos(char *path)
  {
  char *p = path;
  if (p != NULL)
    {
    while (*p != '\0')
      {
      if (*p == '/') *p = '\\'; // change slashes to backslashes
      /* *p = toupper(*p); change to uppercase */
      p++;
      }
    }
  }

static int is_drive_spec(char *s)    // check for form "A:"
  {
  if (!isalpha(s[0]))
    return false;
  if (s[1] != ':')
    return false;
  if (s[2] != '\0')
    return false;
  return true;
  }

static int is_drive_spec_with_slash(char *s)  // check for form "C:\"
  {
  if (!isalpha(s[0]))
    return false;
  if (s[1] != ':')
    return false;
  if (s[2] != '\\')
    return false;
  if (s[3] != '\0')
    return false;
  return true;
  }

static int has_trailing_slash(char *s)
  {
  if (*s == '\0')
    return false;
  s = strchr(s,'\0')-1;
  if (*s == '\\' || *s == '/')
    return true;
  return false;
  }

static int has_wildcard(char *s)
  {
  if (strchr(s, '*') != NULL)
    return true;
  if (strchr(s, '?') != NULL)
    return true;
  return false;
  }

static void reset_batfile_call_stack(void)
  {
  static int first_time = true;
  int ba;

  if (!first_time)
    {
    if (bat_file_path[stack_level][0] != '\0')
      {
      cprintf("Batch file aborted - %s, line %d\r\n",
        bat_file_path[stack_level], bat_file_line_number[stack_level]);
      }
    }
  first_time = false;
  // initialize stack
  for (stack_level = 0; stack_level < MAX_STACK_LEVEL; stack_level++)
    {
    bat_file_path[stack_level][0] = '\0';
    for (ba = 0; ba < MAX_BAT_ARGS; ba++)
      bat_arg[stack_level][ba][0] = '\0';
    bat_file_line_number[stack_level] = 0;
    echo_on[stack_level] = true;
    }
  stack_level = 0;
  }

static void output_prompt(void)
  {
  char cur_drive_and_path[MAXPATH];
  const char *promptvar = getenv("PROMPT");

  if (need_to_crlf_at_next_prompt)
    {
    if (wherex() > 1)
      cputs("\r\n");
    need_to_crlf_at_next_prompt = false;
    }

  if (promptvar == NULL)
    promptvar = "$p$g";
  getcwd(cur_drive_and_path, MAXPATH);
  /* The disk letter is changed to upper-case */
  cur_drive_and_path[0] = toupper(cur_drive_and_path[0]);
  conv_unix_path_to_ms_dos(cur_drive_and_path);
  while (*promptvar != '\0')
    {
    if (*promptvar == '$')
      {
      promptvar++;
      switch (toupper(*promptvar))
        {
        case '\0':
          promptvar--;
          break;
        case 'Q': //    = (equal sign)
          putch('=');
          break;
        case '$': //    $ (dollar sign)
          putch('$');
          break;
        case 'T': //    Current time (TODO: emulate centisecond)
          {
          time_t t = time(NULL);
          struct tm *loctime = localtime (&t);
          cprintf("%2d:%02d:%02d", loctime->tm_hour, loctime->tm_min, loctime->tm_sec);
          break;
          }
        case 'D': //    Current date
          {
          time_t t = time(NULL);
          struct tm *loctime = localtime (&t);
          cprintf("%02d-%02d-%04d", loctime->tm_mon+1, loctime->tm_mday, loctime->tm_year+1900);
          break;
          }
        case 'P': //   Current drive and path
          {
          cputs(cur_drive_and_path);
          break;
          }
        case 'N': //    Current drive
          {
          putch(*cur_drive_and_path);
          putch(':');
          break;
          }
        case 'G': //    > (greater-than sign)
          putch('>');
          break;
        case 'L': //    < (less-than sign)
          putch('<');
          break;
        case 'B': //    | (pipe)
          putch('|');
          break;
        case '_': //    ENTER-LINEFEED
          cputs("\r\n");
          break;
        case 'E': //    ASCII escape code
          putch(27);
          break;
        case 'H': //    Backspace
          putch(8);
          break;
        default:
          putch('$');
          putch(*promptvar);
          break;
        }
      }
    else
      putch(*promptvar);
    promptvar++;
    }
  }

static void extract_args(char *src)
  {
  char *dest, *saved_src = src;

  // scout ahead to see if there are really any arguments
  while (*src == ' ' || *src == '\t')
    src++;
  if (*src == '\0' || *src == ';')
    {
    cmd_arg[0] = '\0';
    cmd_switch[0] = '\0';
    return;
    }

  // extract combined arguments
  src = saved_src;
  if (*src == ' ' || *src == '\t')
    src++;
  memmove(cmd_args, src, strlen(src)+1);

  // extract first occurring single argument
  src = cmd_args;
  while (*src == ' ' || *src == '\t')
    src++;
  dest = cmd_arg;
  while (*src != ' ' && *src != '\t' && *src != '\0')
    {
    *dest = *src;
    dest++;
    src++;
    if (*src == '/' || *src == ';')
      break;
    }
  *dest = '\0';

  // copy the single argument to cmd_switch if it qualifies as a switch
  if (cmd_arg[0] == '/')
    strcpy(cmd_switch, cmd_arg);
  else
    cmd_switch[0] = '\0';
  return;
  }

static void advance_cmd_arg(void)
  {
  char *extr;

  extr = cmd_args;

  // skip over first argument
  while (*extr == ' ' || *extr == '\t')
    extr++;
  if (*extr == '\0')
    goto NoArgs;

  while (*extr != ' ' && *extr != '\t' && *extr != '\0')
    {
    extr++;
    if (*extr == '/' || *extr == ';')
      break;
    }
  if (*extr == '\0')
    goto NoArgs;
  if (*extr == ';')
    {
    memmove(cmd_args, extr, strlen(extr)+1);
    goto NoArgs;
    }

  // extract the rest
  extract_args(extr);
  return;

NoArgs:
  cmd_arg[0] = '\0';
  cmd_switch[0] = '\0';
  return;
  }

static unsigned short keyb_shift_states;
static unsigned short keyb_get_rawcode(void)
{
  unsigned short c = getch();

  if (c == 0x00/* || c == 0xE0*/)
    c = getch()<<8;

  if (c == KEY_INSERT)
    keyb_shift_states ^= KEYB_FLAG_INSERT;

  return c;
}
static unsigned short keyb_get_shift_states(void)
{
  return keyb_shift_states;
}

static void prompt_for_and_get_cmd(void)
  {
  int flag = 0, key = 0, len, len1, need_store, got_tab;
  char conbuf[MAX_CMD_BUFLEN+1];

  output_prompt();
  /* Console initialize */
  flag = keyb_get_shift_states();
  if (!(flag&KEYB_FLAG_INSERT))
    _setcursortype(_NORMALCURSOR);
  else
    _setcursortype(_SOLIDCURSOR);

  need_store = 0;
  got_tab = 0;
  conbuf[0] = '\0';
  do {
    /* Wait and get raw key code */
    key = keyb_get_rawcode();
    flag = keyb_get_shift_states();

//    if (KEY_ASCII(key) == KEY_EXT)
//      key = KEY_EXTM(key);
//    else
    if (KEY_ASCII(key) != 0)
      key = KEY_ASCII(key);
    if (key != KEY_ENTER)
      need_store = 1;
    if (key != KEY_TAB)
      got_tab = 0;
    switch (key)
    {
      case 0:
        break;
      case 3:
      case 0x100:
        {
        int cur = cmdbuf_getcur();
        int tail = cmdbuf_gettail();
        if (tail > cur)
          cmdbuf_clreol(conbuf);
        else if (tail > 0)
          cmdbuf_clear(conbuf);
        break;
        }
      case KEY_ENTER:
        break;
      case KEY_BACKSPACE:
        if (cmdbuf_bksp(conbuf))
          {
          putch(KEY_ASCII(KEY_BACKSPACE));
          putch(' ');
          putch(KEY_ASCII(KEY_BACKSPACE));
          }
        break;
      case KEY_DELETE:
        cmdbuf_delch(conbuf);
        break;
      case KEY_INSERT:
        if (!(flag&KEYB_FLAG_INSERT))
          _setcursortype(_NORMALCURSOR);
        else
          _setcursortype(_SOLIDCURSOR);
        break;
      case KEY_UP:
        if (conbuf[0])
          {
          cmdbuf_trunc(conbuf);
          cmdbuf_store_tmp(conbuf);
          cmdbuf_clear(conbuf);
          }
        if (cmdbuf_move(conbuf, UP))
          need_store = 0;
        break;
      case KEY_LEFT:
        cmdbuf_move(conbuf, LEFT);
        break;
      case KEY_RIGHT:
        cmdbuf_move(conbuf, RIGHT);
        break;
      case KEY_DOWN:
        if (conbuf[0])
          {
          cmdbuf_trunc(conbuf);
          cmdbuf_store_tmp(conbuf);
          cmdbuf_clear(conbuf);
          }
        if (cmdbuf_move(conbuf, DOWN))
          need_store = 0;
        break;
      case KEY_PGUP:
        if (conbuf[0])
          {
          cmdbuf_trunc(conbuf);
          cmdbuf_store_tmp(conbuf);
          cmdbuf_clear(conbuf);
          }
        if (cmdbuf_move(conbuf, PGUP))
          need_store = 0;
        break;
      case KEY_PGDN:
        if (conbuf[0])
          {
          cmdbuf_trunc(conbuf);
          cmdbuf_store_tmp(conbuf);
          cmdbuf_clear(conbuf);
          }
        if (cmdbuf_move(conbuf, PGDN))
          need_store = 0;
        break;
      case KEY_HOME:
        cmdbuf_move(conbuf, HOME);
        break;
      case KEY_END:
        cmdbuf_move(conbuf, END);
        break;
      case KEY_TAB:
        {
        int rc, need_prn = got_tab, l = 0;
        char p[MAXPATH];
        const char *p1;

        cmdbuf_trunc(conbuf);
        if (need_prn)
          putchar('\n');
        if ((p1 = strrchr(conbuf, ' ')))
          {
          p1++;
          rc = compl_fname(p1, got_tab, &l, p);
          /* fixup for directories */
          if (rc == 1)
            {
            char buf[MAXPATH];
            struct stat sb;
            strcpy(buf, p1);
            strcat(buf, p);
            int err = stat(buf, &sb);
            if (!err && (sb.st_mode & S_IFMT) == S_IFDIR)
              {
              strcat(p, "\\");
              l++;
              rc = 0;
              }
            }
          }
        else
          rc = compl_cmds(conbuf, got_tab, &l, p);
        if (need_prn)
          output_prompt();
        got_tab = 0;
        switch (rc)
          {
          case -1:
            putchar('\a');
            break;
          case 0:
            if (!need_prn && !l)
              putchar('\a');
            if (strlen(conbuf))
              got_tab++;
            if (l)
              {
              printf("%.*s", l, p);
              strncat(conbuf, p, l);
              cmdbuf_puts(conbuf);
              }
            break;
          case 1:
            printf("%s ", p);
            strcat(conbuf, p);
            strcat(conbuf, " ");
            cmdbuf_puts(conbuf);
            break;
          }
        if (need_prn)
          printf("%s", conbuf);
        break;
        }
      default:
        if (KEY_ASCII(key) != 0x00/* && KEY_ASCII(key) != 0xE0*/) {
          char c = cmdbuf_putch(conbuf, MAX_CMD_BUFLEN-2, KEY_ASCII(key), flag);
          if (c)
            putch(c);
        }
        break;
    }
  } while (key != KEY_ENTER);

  if (need_store)
    {
    cmdbuf_trunc(conbuf);
    cmdbuf_eol();
    }
  else
    cmdbuf_reset();
  strcpy(cmd_line, conbuf);
  /* Get the size of typed string */
  len = strlen(conbuf);
  if (!len)
    {
    cputs("\r\n");
    return;
    }
  len1 = strspn(cmd_line, "\r\n\t ");
  if (len1 >= len)
    {
    /* whole cmd_line contains only junk */
    cputs("\r");
    return;
    }
  if (len1)
    {
    /* part of cmd_line contains junk, skip it */
    memmove(cmd_line, cmd_line + len1, len - len1 + 1);
    len -= len1;
    }
  if (need_store)
    cmdbuf_store(cmd_line);
  parse_cmd_line();
  cputs("\r\n");
  }

static int get_choice(const char *choices)
  {
  int choice, key;
//  strupr(choices);
  do
    {
    key = getch();
    if (key == 0)
      continue;
    } while (strchr(choices, toupper(key)) == NULL);
  choice = toupper(key);
  cprintf("%c", choice);
  cputs("\r\n");
  return choice;
  }

static void get_cmd_from_bat_file(void)
  {
  FILE *cmd_file;
  int line_num, c, ba;
  char *s;

  if (bat_file_line_number[stack_level] != MAXINT)
    bat_file_line_number[stack_level]++;

  cmd_file = fopen(bat_file_path[stack_level], "rt");
  if (cmd_file == NULL)
    {
    cprintf("Cannot open %s\r\n", bat_file_path[stack_level]);
    goto ErrorDone;
    }

  for (line_num = 0; line_num < bat_file_line_number[stack_level]; line_num++)
    {
    /* input as much of the line as the buffer can hold */
    s = fgets(cmd_line, MAX_CMD_BUFLEN, cmd_file);

    /* if s is null, investigate why */
    if (s == NULL)
      {
      /* check for error */
      if (ferror(cmd_file))
        {
        cprintf("Read error: %s, line %d\r\n", bat_file_path[stack_level], line_num+1);
        goto ErrorDone;
        }
      /* line is unavailable because of end of file */
      if (goto_label[0] != '\0')
        {
        cprintf("Label not found - %s\r\n", goto_label);
        goto_label[0] = '\0';
        goto ErrorDone;
        }
      goto FileDone;
      }

    /*
    *   Check for newline character;
    *   If present, we have successfully reached end of line
    *   If not present, continue getting line until newline or eof encountered
    */
    s = strchr(cmd_line, '\n');
    if (s != NULL)
      *s = '\0';
    else
      {
      do
        {
        c = fgetc(cmd_file);
        } while (c != '\n' && c != EOF);  // if eof occurs here, it needs
                                         // to be caught on the next iteration
                                         // but not now, and not here.
      if (ferror(cmd_file))
        {
        cprintf("Read error: %s, line %d\r\n", bat_file_path[stack_level], line_num+1);
        goto ErrorDone;
        }
      }

    // check for goto arrival at labeled destination
    if (goto_label[0] != '\0')
      {
      s = cmd_line;
      while (*s == ' ' || *s == '\t')
        s++;
      if (*s == ':')
        {
        s++;
        if (strnicmp(goto_label, s, strlen(goto_label)) == 0)
          {
          s += strlen(goto_label);
          if (*s == ' ' || *s == '\t' || *s == '\0')
            {
            // we have arrived... set line number, erase goto label
            bat_file_line_number[stack_level] = line_num + 1;
            goto_label[0] = '\0';
            break;
            }
          }
        }
      }
    }

  if (stepping)
    {
    int c;
    printf("%s [Y/N] ", cmd_line);
    c = getche();
    puts("");
    switch (c)
      {
      case 'Y':
      case 'y':
        break;
      case 'N':
      case 'n':
        return;
      case 0x1b:  // ESC
        stepping = 0;
        break;
      default:
        goto ErrorDone;
      }
    }

  // parse command
  parse_cmd_line();

  // deal with echo on/off and '@' at the beginning of the command line
  if (cmd[0] == '@')
    memmove(cmd, cmd+1, strlen(cmd));
  else
    {
    if (echo_on[stack_level] && !stepping)
      {
      output_prompt();
      cputs(cmd_line);
      cputs("\r\n");
      }
    }

  goto RoutineDone;

ErrorDone:
  reset_batfile_call_stack();
FileDone:
  cmd_line[0] = '\0';
  parse_cmd_line();  // this clears cmd[], cmd_arg[], cmd_switch[], and cmd_args[]
  bat_file_path[stack_level][0] = '\0';
  for (ba = 0; ba < MAX_BAT_ARGS; ba++)
    bat_arg[stack_level][ba][0] = '\0';
  bat_file_line_number[stack_level] = 0;
  echo_on[stack_level] = true;
  if (stack_level > 0)
    stack_level--;
  if (stack_level == 0 && bat_file_path[stack_level][0] == '\0')
    {
    stepping = 0;
    /* send empty cmd to update window title of dosemu2 */
    installable_command_check(cmd_line, "");
    }
RoutineDone:
  if (cmd_file != NULL)
    fclose(cmd_file);
  }

static int ensure_dir_existence(char *dir)
  {
  char *c;
  size_t len;
  char dir_path[MAXPATH];

  strcpy(dir_path, dir);
  len = strlen(dir_path);
  if (!len)
    return -1;
  if (dir_path[len - 1] == '\\')     // take away ending backslash
    dir_path[len - 1] = '\0';

  if (file_access(dir_path, D_OK) != 0)
    {
    c = strchr(dir_path, '\\');
    while (c != NULL)
      {
      c = strchr(c+1, '\\');
      if (c == NULL)
        printf("Creating directory - %s\\\n", dir_path);
      else
        *c = '\0';
      if (_mkdir(dir_path) != 0 && c == NULL)
        {
        cprintf("Unable to create directory - %s\\\r\n", dir_path);
        return -1;
        }
      if (c != NULL)
        *c = '\\';
      }
    }
  return 0;
  }

static int copy_single_file(char *source_file, char *dest_file,
    int transfer_type, int append)
  {
  FILE *source_stream;
  FILE *dest_stream;
  char transfer_buffer[32768];
  size_t byte_count;
  struct stat st;
  int err;

  if (stricmp(source_file, dest_file) == 0)
    {
    cprintf("Source and destination cannot match - %s\r\n", source_file);
    return -1;
    }
  err = lstat(source_file, &st);
  if (err)
    {
    cprintf("cannot stat %s\r\n", source_file);
    return -1;
    }
  if (st.st_mode & S_IFCHR)
    {
    source_stream = fopen(source_file, "rt");
    setbuf(source_stream, NULL);
    __file_handle_set(fileno(source_stream), O_BINARY);
    }
  else
    {
    /* Open file for copy */
    source_stream = fopen(source_file, "rb");
    }
  if (source_stream == NULL)
    {
    cprintf("Unable to open source file - %s\r\n", source_file);
    return -1;
    }
  dest_stream = fopen(dest_file, append ? "ab" : "wb");
  if (dest_stream == NULL)
    {
    cprintf("Unable to open destination file - %s\r\n", dest_file);
    fclose(source_stream);
    return -1;
    }

  /* Copy file contents*/
  do
    {
    byte_count = 0;
    if (st.st_mode & S_IFCHR)
      {
      char c;
      c = fgetc(source_stream);
      if (!(c == EOF || c == 0x1a || c == 3 || c == 0))
        {
        transfer_buffer[0] = c;
        byte_count = 1;
        }
      }
    else
      byte_count = fread(transfer_buffer, 1, 32768, source_stream);
    if (byte_count > 0)
      {
      if (fwrite(transfer_buffer, 1, byte_count, dest_stream) != byte_count)
        goto copy_error_close;
      }
    }
  while (byte_count > 0);

  /* Copy date and time */
  fflush(dest_stream);
  if (file_copytime (fileno(dest_stream), fileno(source_stream)) != 0)
    goto copy_error_close;

  /* Close source and dest files */
  fclose(source_stream);
  if (fclose(dest_stream) != 0)
    goto copy_error;
  return 0;

/*
*    Error routine
*/
copy_error_close:
  fclose(source_stream);
  fclose(dest_stream);

copy_error:
  remove(dest_file);          // erase the unfinished file
  if (transfer_type == FILE_XFER_MOVE)
    cprintf("Error occurred while moving file - %s\r\n", source_file);
  else
    cprintf("Error occurred while copying to file - %s\r\n", dest_file);
  return -1;
  }  /* copy_single_file */

static int verify_file(char *master_file, char *verify_file)
  {
  FILE *mstream;
  FILE *vstream;
  char mtransfer_buffer[32768];
  char vtransfer_buffer[32768];
  int b;
  size_t mbyte_count, vbyte_count;
  struct stat mfile_st, vfile_st;


  /* Open files */
  mstream = fopen(master_file, "rb");
  if (mstream == NULL)
    goto verify_error;
  vstream = fopen(verify_file, "rb");
  if (vstream == NULL)
    {
    fclose(mstream);
    goto verify_error;
    }

  /* Verify file contents*/
  do
    {
    mbyte_count = fread(mtransfer_buffer, 1, 32768, mstream);
    vbyte_count = fread(vtransfer_buffer, 1, 32768, vstream);
    if (mbyte_count != vbyte_count)
      goto verify_error_close;
    if (mbyte_count > 0)
      {
      for (b = 0; b < mbyte_count; b++)
        {
        if (mtransfer_buffer[b] != vtransfer_buffer[b])
          goto verify_error_close;
        }
      }
    }
  while (mbyte_count > 0);

  /* verify date and time */
  if (fstat(fileno(mstream), &mfile_st) != 0)
    goto verify_error_close;
  if (fstat(fileno(vstream), &vfile_st) != 0)
    goto verify_error_close;
  if (mfile_st.st_atime != vfile_st.st_atime || mfile_st.st_mtime != vfile_st.st_mtime)
    goto verify_error_close;

  /* Close source and dest files */
  fclose(mstream);
  fclose(vstream);
  return 0;

/*
*    Error routine
*/
verify_error_close:
  fclose(mstream);
  fclose(vstream);

verify_error:
  cprintf("Verify failed - %s\r\n", verify_file);
  return -1;
  }

static void expand_wildcard(char *spec, const char *fname, const char *fext)
  {
  char *p, *dot;
  const char *p1;
  int on_ext = 0;

  dot = strchr(spec, '.');
  /* first convert * to ? */
  while ((p = strchr(spec, '*')))
    {
    int len = ((dot && p > dot) ? 3 - (p - (dot + 1)) : 8 - (p - spec));
    int rem = strlen(p + 1) + 1;

    memmove(p + len, p + 1, rem);
    memset(p, '?', len);
    dot = strchr(spec, '.');
    }
  /* now expand it */
  p = spec;
  p1 = fname;
  while (*p)
    {
    if (*p == '?')
      {
      if (*p1)
        *p = *p1;
      else
        {
        memmove(p, p + 1, strlen(p + 1) + 1);
        p--;
        dot = strchr(spec, '.');
        }
      }
    p++;
    if (*p1)
      p1++;
    if (!*p1 && p > dot && !on_ext)
      {
      on_ext++;
      p1 = fext;
      }
    }
  }

static void general_file_transfer(int transfer_type, int append)
  {
  int xfer_count = 0;
  int ffrc;
  long ffhandle = 0;
  int traverse_subdirs = false;
  int copy_empty_subdirs = false;
  int do_file_verify;
  int s, subdir_level = 0;
  finddata_t ff[MAX_SUBDIR_LEVEL];
  char dir_name[MAX_SUBDIR_LEVEL][MAXPATH];
  int visitation_mode[MAX_SUBDIR_LEVEL]; // 4 = findfirst source_filespec for files;
                                         // 3 = findnext source_filespec for files;
                                         // 2 = findfirst *.* for subdirs;
                                         // 1 = findnext *.* for subdirs;
                                         // 0 = done
  unsigned attrib;
  char drivespec[MAXDRIVE], dirspec[MAXDIR], s_filespec[MAXFILE], s_extspec[MAXEXT];
  char d_filespec[MAXFILE], d_extspec[MAXEXT];
  char temp_path[MAXPATH];
  char source_path[MAXPATH] = "", source_filespec[MAXPATH];
  char dest_path[MAXPATH] = "", dest_filespec[MAXPATH];
  char full_source_filespec[MAXPATH];
  char full_dest_filespec[MAXPATH];
  char full_dest_dirspec[MAXPATH];

  if (transfer_type == FILE_XFER_MOVE)
    do_file_verify = true;
  else
    do_file_verify = false;

  while (*cmd_arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*source_path == '\0')
        {
        strncpy(source_path, cmd_arg, MAXPATH);
        source_path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(source_path);
        }
      else if (*dest_path == '\0')
        {
        strncpy(dest_path, cmd_arg, MAXPATH);
        dest_path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(dest_path);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      if (stricmp(cmd_switch,"/v") == 0)
        {
        if (transfer_type == FILE_XFER_COPY ||
            transfer_type == FILE_XFER_XCOPY)
          do_file_verify = true;
        else
          goto InvalidSwitch;
        }
      if (stricmp(cmd_switch,"/b") == 0)
        {
        /* ignore */
        }
      else
        {
        if (transfer_type == FILE_XFER_XCOPY)
          {
          if (stricmp(cmd_switch,"/s") == 0)
            traverse_subdirs = true;
          else if (stricmp(cmd_switch,"/e") == 0)
            copy_empty_subdirs = true;
          else
            goto InvalidSwitch;
          }
        else
          goto InvalidSwitch;
        }
      }
    advance_cmd_arg();
    }

  if (*source_path == '\0' ||
      (transfer_type == FILE_XFER_MOVE && *dest_path == '\0'))
    {
    cputs("Required parameter missing\r\n");
    reset_batfile_call_stack();
    return;
    }

  if (*dest_path == '\0')
    strcpy(dest_path, ".");

  // prepare source for fnsplit() -
  // attach a file specification if specified source doesn't have one
  if (is_drive_spec(source_path) || has_trailing_slash(source_path))
    strcat(source_path, "*.*");
  else
    {
    // see if source exists and is a directory; if so, attach a file spec
    if (file_access(source_path, D_OK) == 0)
      strcat(source_path, "\\*.*");
    }

  // parse source - create full source path and split into 2 components: path + file spec
  _fixpath(source_path, temp_path);
  fnsplit(temp_path, drivespec, dirspec, s_filespec, s_extspec);
  strcpy(source_path, drivespec);
  strcat(source_path, dirspec);
  conv_unix_path_to_ms_dos(source_path);
  strcpy(source_filespec, s_filespec);
  strcat(source_filespec, s_extspec);
  conv_unix_path_to_ms_dos(source_filespec);

  // prepare dest for fnsplit() -
  // attach a file specification if specified dest doesn't have one
  if (is_drive_spec(dest_path) || has_trailing_slash(dest_path))
    strcat(dest_path, "*.*");
  else
    {
    // see if dest exists and is a directory; if so, attach a file spec
    if (file_access(dest_path, D_OK) == 0)
      strcat(dest_path, "\\*.*");
    else  // else -- if dest does not exist or is not a directory...
      {
      if (transfer_type == FILE_XFER_XCOPY || transfer_type == FILE_XFER_MOVE)
        {
        // if source has a wildcard and dest does not, then treat dest as a dir ...
        if (has_wildcard(source_filespec) && !has_wildcard(dest_path))
          strcat(dest_path, "\\*.*");     // dest is a directory; attach a file spec
        }
      else
        {
        if (transfer_type == FILE_XFER_XCOPY)  // if we are doing xcopy, ask if target is a dir or a file
          {
          fnsplit(dest_path, NULL, NULL, d_filespec, d_extspec);
          cprintf("Does %s%s specify a file name\r\n", d_filespec, d_extspec);
          cputs("or directory name on the target\r\n");
          cputs("(F = file, D = directory)?");
          if (get_choice("FD") == 'D')
            strcat(dest_path, "\\*.*");
          }
        }
      }
    }

  // parse dest - create full dest path and split into 2 components: path + file spec
  _fixpath(dest_path, temp_path);
  fnsplit(temp_path, drivespec, dirspec, d_filespec, d_extspec);
  strcpy(dest_path, drivespec);
  strcat(dest_path, dirspec);
  conv_unix_path_to_ms_dos(dest_path);
  strcpy(dest_filespec, d_filespec);
  strcat(dest_filespec, d_extspec);
  conv_unix_path_to_ms_dos(dest_filespec);

  if (has_wildcard(dest_filespec))
    expand_wildcard(dest_filespec, s_filespec, s_extspec + 1);
  if (has_wildcard(dest_path))
    {
    cputs("Illegal wildcard on destination\r\n");
    reset_batfile_call_stack();
    return;
    }

  // Stuff for the move command only
  if (transfer_type == FILE_XFER_MOVE)
    {
    // if source and dest are both full directories in the same
    // tree and on the same drive, and the dest directory does not exist,
    // then just rename source directory to dest
    if (strcmp(source_filespec, "*.*") == 0 &&
        !is_drive_spec_with_slash(source_path) &&
        strcmp(dest_filespec, "*.*") == 0 &&
        !is_drive_spec_with_slash(dest_path))
      {
      char source_dirspec[MAXPATH];
      char dest_dirspec[MAXPATH];
      int dest_dir_exists;
      char *sbs, *dbs;  // backslash ptrs

      // check for both dirs to be at same tree and on the same drive
      strcpy(source_dirspec, source_path);
      strcpy(dest_dirspec, dest_path);
      *(strrchr(source_dirspec, '\\')) = '\0'; // get rid of trailing backslash
      *(strrchr(dest_dirspec, '\\')) = '\0'; // get rid of trailing backslash
      dest_dir_exists = (file_access(dest_dirspec, D_OK) == 0);
      sbs = strrchr(source_dirspec, '\\');
      *sbs = '\0'; // chop off source dir name, leaving source tree
      dbs = strrchr(dest_dirspec, '\\');
      *dbs = '\0'; // chop off dest dir name, leaving dest tree

      if (stricmp(source_dirspec, dest_dirspec) == 0) // if source tree == dest tree
        {
        if (!dest_dir_exists) // if dest dir does not exist..
          {
          *sbs = '\\'; // put the backslash back
          *dbs = '\\'; // put the backslash back
          if (rename(source_dirspec, dest_dirspec) == 0)
            {
            printf("%s renamed to %s\n", source_dirspec, dbs+1);
            return;
            }
          }
        }
      }
    }

  // visit each directory; perform transfer
  visitation_mode[0] = 4;
  dir_name[0][0] = '\0';
  while (subdir_level >= 0)
    {
    if (visitation_mode[subdir_level] == 4 || visitation_mode[subdir_level] == 2)
      {
      strcpy(full_source_filespec, source_path);
      for (s = 0; s <= subdir_level; s++)
        strcat(full_source_filespec, dir_name[s]);
      if (visitation_mode[subdir_level] == 4)
        {
        strcat(full_source_filespec, source_filespec);
        attrib = 0+FA_HIDDEN+FA_SYSTEM;
        }
      else
        {
        strcat(full_source_filespec, "*.*");
        attrib = 0+FA_DIREC+FA_HIDDEN+FA_SYSTEM;
        }
      ffrc = findfirst_f(full_source_filespec, &(ff[subdir_level]), attrib, &ffhandle);
      visitation_mode[subdir_level]--;
      }
    else
      ffrc = findnext_f(&(ff[subdir_level]), ffhandle);
    if (ffrc == 0)
      {
      conv_unix_path_to_ms_dos(FINDDATA_T_FILENAME(ff[subdir_level]));
      strcpy(full_source_filespec, source_path);
      strcpy(full_dest_filespec, dest_path);
      strcpy(full_dest_dirspec, dest_path);
      for (s = 0; s <= subdir_level; s++)
        {
        strcat(full_source_filespec, dir_name[s]);
        strcat(full_dest_filespec, dir_name[s]);
        strcat(full_dest_dirspec, dir_name[s]);
        }
      strcat(full_source_filespec, FINDDATA_T_FILENAME(ff[subdir_level]));
      if (strcmp(dest_filespec, "*.*") == 0)
        strcat(full_dest_filespec, FINDDATA_T_FILENAME(ff[subdir_level]));
      else
        strcat(full_dest_filespec, dest_filespec);

      if ((FINDDATA_T_ATTRIB(ff[subdir_level])&FA_DIREC) != 0)
        {
        if (visitation_mode[subdir_level] <= 2 &&
            traverse_subdirs &&
            strcmp(FINDDATA_T_FILENAME(ff[subdir_level]),".") != 0 &&
            strcmp(FINDDATA_T_FILENAME(ff[subdir_level]),"..") != 0)
          {
          subdir_level++;
          if (subdir_level >= MAX_SUBDIR_LEVEL)
            {
            cputs("Directory tree is too deep\r\n");
            reset_batfile_call_stack();
            goto ExitOperation;
            }
          if (copy_empty_subdirs)
            {
            if (ensure_dir_existence(full_dest_filespec) != 0)
              {
              reset_batfile_call_stack();
              goto ExitOperation;
              }
            }
          visitation_mode[subdir_level] = 4;
          strcpy(dir_name[subdir_level], FINDDATA_T_FILENAME(ff[subdir_level-1]));
          strcat(dir_name[subdir_level], "\\");
          }
        }
      else
        {
        if (visitation_mode[subdir_level] > 2)
          {
          if (transfer_type == FILE_XFER_XCOPY ||
              transfer_type == FILE_XFER_MOVE)
            {
            if (ensure_dir_existence(full_dest_dirspec) != 0)
              {
              reset_batfile_call_stack();
              goto ExitOperation;
              }
            }
          if (copy_single_file(full_source_filespec,
                               full_dest_filespec, transfer_type, append) != 0)
            {
            reset_batfile_call_stack();
            goto ExitOperation;
            }
          if (do_file_verify)
            {
            if (verify_file(full_source_filespec, full_dest_filespec) != 0)
              {
              reset_batfile_call_stack();
              goto ExitOperation;
              }
            }
          if (transfer_type == FILE_XFER_MOVE)
            {
            if (remove(full_source_filespec) != 0)
              {
              remove(full_dest_filespec);
              cprintf("Unable to move file - %s\r\n", full_source_filespec);
              reset_batfile_call_stack();
              goto ExitOperation;
              }
            }
          printf("%s %s to %s\n",
            FINDDATA_T_FILENAME(ff[subdir_level]),
            append ? "appended" : (
            transfer_type == FILE_XFER_MOVE?"moved":"copied"),
            strcmp(dest_filespec, "*.*")==0?full_dest_dirspec:full_dest_filespec);
          xfer_count++;
          }
        }
      }
    else
      {
      if (traverse_subdirs)
        visitation_mode[subdir_level]--;
      else
        visitation_mode[subdir_level] = 0;
      if (visitation_mode[subdir_level] <= 0)
        subdir_level--;
      }
    }
  if (xfer_count == 0)
    printf("File(s) not found - %s%s\n", source_path, source_filespec);
  else
    {
    if (transfer_type == FILE_XFER_MOVE)
      printf("%9d file(s) moved\n", xfer_count);
    else
      printf("%9d file(s) copied\n", xfer_count);
    }
ExitOperation:
  return;

InvalidSwitch:
  cprintf("Invalid switch - %s\r\n", cmd_switch);
  reset_batfile_call_stack();
  return;
  }

static int get_set_file_attribute(char *full_path_filespec, unsigned req_attrib, unsigned attrib_mask)
  {
  int a;
  unsigned actual_attrib;

  if (attrib_mask == 0)
    {
    if (getfileattr(full_path_filespec, &actual_attrib) != 0)
      {
      cprintf("Cannot read attribute - %s\r\n", full_path_filespec);
      return -1;
      }
    }
  else
    {
    if (getfileattr(full_path_filespec, &actual_attrib) != 0)
      actual_attrib = 0;
    actual_attrib &= (~attrib_mask);
    actual_attrib |= (req_attrib & attrib_mask);
    if (setfileattr(full_path_filespec, actual_attrib) != 0)
      goto CantSetAttr;
    printf("Attribute set to ");
    }

  for (a = 0; a < 4; a++)
    {
    if ((actual_attrib&attrib_values[a]) == 0)
      printf(" -%c",tolower(attrib_letters[a]));
    else
      printf(" +%c",toupper(attrib_letters[a]));
    }
  printf("  - %s\n", full_path_filespec);
  return 0;

CantSetAttr:
  cprintf("Cannot set attribute - %s\r\n", full_path_filespec);
  return -1;
  }

///////////////////////////////////////////////////////////////////////////////////

static void perform_attrib(const char *arg)
  {
  long ffhandle = 0;
  int ffrc;
  int file_count = 0;
  int traverse_subdirs = false;
  int s, subdir_level = 0;
  finddata_t ff[MAX_SUBDIR_LEVEL];
  char dir_name[MAX_SUBDIR_LEVEL][MAXPATH];
  int visitation_mode[MAX_SUBDIR_LEVEL]; // 4 = findfirst source_filespec for files;
                                         // 3 = findnext source_filespec for files;
                                         // 2 = findfirst *.* for subdirs;
                                         // 1 = findnext *.* for subdirs;
                                         // 0 = done
  char drivespec[MAXDRIVE], dirspec[MAXDIR], filename[MAXFILE], extspec[MAXEXT];
  char temp_path[MAXPATH];
  char path[MAXPATH] = "", filespec[MAXPATH];
  char full_path_filespec[MAXPATH];

  int a;
  unsigned req_attrib = 0, attrib_mask = 0;
  unsigned search_attrib;

  while (*arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (strlen(arg) == 2 && (arg[0] == '+' || arg[0] == '-'))
        {
        for (a = 0; a < 4; a++)
          {
          if (toupper(arg[1]) == toupper(attrib_letters[a]))
            {
            attrib_mask |= attrib_values[a];
            if (arg[0] == '+')
              req_attrib |= attrib_values[a];
            else
              req_attrib &= (~(attrib_values[a]));
            }
          }
        }
      else if (*path == '\0')
        {
        strncpy(path, arg, MAXPATH);
        path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(path);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      if (stricmp(cmd_switch,"/s") == 0)
        traverse_subdirs = true;
      else
        {
        cprintf("Invalid switch - %s\r\n", cmd_switch);
        reset_batfile_call_stack();
        return;
        }
      }
    advance_cmd_arg();
    }

  if (*path == '\0')
    strcpy(path, "*.*");

  // prepare path for fnsplit() -
  // attach a file specification if specified path doesn't have one
  if (is_drive_spec(path) || has_trailing_slash(path))
    strcat(path, "*.*");
  else
    {
    // see if path exists and is a directory; if so, attach a file spec
    if (file_access(path, D_OK) == 0)
      strcat(path, "\\*.*");
    }

  // parse path - create full path and split into 2 components: path + file spec
  _fixpath(path, temp_path);
  fnsplit(temp_path, drivespec, dirspec, filename, extspec);
  strcpy(path, drivespec);
  strcat(path, dirspec);
  conv_unix_path_to_ms_dos(path);
  strcpy(filespec, filename);
  strcat(filespec, extspec);
  conv_unix_path_to_ms_dos(filespec);

  // visit each directory; perform attrib get/set
  visitation_mode[0] = 4;
  dir_name[0][0] = '\0';
  while (subdir_level >= 0)
    {
    if (visitation_mode[subdir_level] == 4 || visitation_mode[subdir_level] == 2)
      {
      strcpy(full_path_filespec, path);
      for (s = 0; s <= subdir_level; s++)
        strcat(full_path_filespec, dir_name[s]);
      if (visitation_mode[subdir_level] == 4)
        {
        strcat(full_path_filespec, filespec);
        search_attrib = FA_RDONLY+FA_ARCH+FA_SYSTEM+FA_HIDDEN;
        }
      else
        {
        strcat(full_path_filespec, "*.*");
        search_attrib = FA_DIREC+FA_RDONLY+FA_ARCH+FA_SYSTEM+FA_HIDDEN;
        }
      ffrc = findfirst_f(full_path_filespec, &(ff[subdir_level]), search_attrib, &ffhandle);
      visitation_mode[subdir_level]--;
      }
    else
      ffrc = findnext_f(&(ff[subdir_level]), ffhandle);
    if (ffrc == 0)
      {
      conv_unix_path_to_ms_dos(FINDDATA_T_FILENAME(ff[subdir_level]));
      strcpy(full_path_filespec, path);
      for (s = 0; s <= subdir_level; s++)
        strcat(full_path_filespec, dir_name[s]);
      strcat(full_path_filespec, FINDDATA_T_FILENAME(ff[subdir_level]));

      if ((FINDDATA_T_ATTRIB(ff[subdir_level])&FA_DIREC) != 0)
        {
        if (visitation_mode[subdir_level] <= 2 &&
            traverse_subdirs &&
            strcmp(FINDDATA_T_FILENAME(ff[subdir_level]),".") != 0 &&
            strcmp(FINDDATA_T_FILENAME(ff[subdir_level]),"..") != 0)
          {
          subdir_level++;
          if (subdir_level >= MAX_SUBDIR_LEVEL)
            {
            cputs("Directory tree is too deep\r\n");
            reset_batfile_call_stack();
            return;
            }
          visitation_mode[subdir_level] = 4;
          strcpy(dir_name[subdir_level], FINDDATA_T_FILENAME(ff[subdir_level-1]));
          strcat(dir_name[subdir_level], "\\");
          }
        }
      else
        {
        if (visitation_mode[subdir_level] > 2)
          {
          if (get_set_file_attribute(full_path_filespec, req_attrib, attrib_mask) != 0)
            {
            reset_batfile_call_stack();
            return;
            }
          file_count++;
          }
        }
      }
    else
      {
      if (traverse_subdirs)
        visitation_mode[subdir_level]--;
      else
        visitation_mode[subdir_level] = 0;
      if (visitation_mode[subdir_level] <= 0)
        subdir_level--;
      }
    }
  if (file_count == 0)
    printf("File(s) not found - %s%s\n", path, filespec);
  }

static void perform_call(const char *arg)
  {
  while (*cmd_switch)  // skip switches
    advance_cmd_arg();
  strcpy(cmd, arg);
  advance_cmd_arg();
  exec_cmd(true);
  }

static void perform_license(const char *arg)
  {
  const char *license =
    "comcom64 - COMMAND.COM-compatible command processor for DOS.\n\n"
    "Copyright (C) 1997, CENTROID CORPORATION, HOWARD, PA 16841\n"
    "Copyright (C) Allen S. Cheung (allencheung@fastmail.ca)\n"
    "Copyright (C) 2005, Hanzac Chen\n"
    "Copyright (C) 2019, C. Masloch <pushbx@38.de>\n"
    "Copyright (C) 2018-2024, @stsp <stsp2@yandex.ru>\n"
    "\n"
    "This program is free software: you can redistribute it and/or modify\n"
    "it under the terms of the GNU General Public License as published by\n"
    "the Free Software Foundation, either version 3 of the License, or\n"
    "(at your option) any later version.\n"
    "\n"
    "This program is distributed in the hope that it will be useful,\n"
    "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
    "GNU General Public License for more details.\n";
  printf("%s\n", license);
  }

static void perform_loadhigh(const char *arg)
  {
  if (!*arg)
    {
    cprintf("loadhigh: command name missing\r\n");
    reset_batfile_call_stack();
    return;
    }
  while (*cmd_switch)  // skip switches
    advance_cmd_arg();
  strcpy(cmd, arg);
  advance_cmd_arg();

  perform_external_cmd(false, true, cmd);
	  /* Should we set this to true? Only affects batch files anyway,
	   * which shouldn't be loaded with LOADHIGH to begin with. */
  }


#define LOADFIX_NUMBLOCKS 16
static const int loadfix_numblocks = LOADFIX_NUMBLOCKS;
static unsigned short loadfix_allocations[LOADFIX_NUMBLOCKS];
static int loadfix_ii;
static unsigned loadfix_initialised;

static void loadfix_init(void)
{
  int allocated;
  unsigned short allocation, to_64kib, max, size;
  __dpmi_regs r = {};

  loadfix_ii = 0;
  do
    {
    r.h.ah = 0x48;
    r.x.bx = 1;
    __dpmi_int(0x21, &r);		/* allocate one-paragraph block */
    if ((r.x.flags & 1) == 0)		/* if NC */
      {
      allocated = 1;
      allocation = r.x.ax;
#if DEBUG
        printf("LOADFIX: allocated block at %04Xh\n", allocation);
#endif
      if (allocation >= 0x1000) 	/* does it start above 64 KiB ? */
        {
        r.h.ah = 0x49;
        r.x.es = allocation;
        __dpmi_int(0x21, &r);		/* free */
#if DEBUG
          printf("LOADFIX: too high, freeing block at %04Xh\n", allocation);
#endif
        break;				/* and done */
        }
      if (loadfix_ii >= loadfix_numblocks)
        {
        printf("LOADFIX: too many blocks allocated!\n");
        break;
        }
      loadfix_allocations[loadfix_ii] = allocation;
      ++loadfix_ii;
      r.h.ah = 0x4A;
      r.x.bx = -1;
      r.x.es = allocation;
      __dpmi_int(0x21, &r);		/* resize and get maximum block size */
		/* Note that this expands the block to the maximum
		 * available size. */
      max = r.x.bx;
      to_64kib = 0x1000 - allocation;	/* note: does not underflow */
      size = to_64kib < max ? to_64kib : max;
      r.x.bx = size;
      r.h.ah = 0x4A;
      __dpmi_int(0x21, &r);		/* resize */
		/* If to_64kib is the lower value, this shortens the block
		 * to that size. Else it does nothing. */
#if DEBUG
        printf("LOADFIX: resizing block at %04Xh to %04Xh paragraphs (%u bytes)\n",
		allocation, (int)size, (int)size * 16);
#endif
      }
    else
      {
#if DEBUG
        printf("LOADFIX: could not allocate another block\n");
#endif
      allocated = 0;
      }
    }
  while (allocated);

  link_umb(0);
}


static void loadfix_exit(void)
{
  __dpmi_regs r = {};

  unlink_umb();

  while (loadfix_ii != 0)
    {
    --loadfix_ii;
    r.h.ah = 0x49;
    r.x.es = loadfix_allocations[loadfix_ii];
    __dpmi_int(0x21, &r);		/* free */
#if DEBUG
      printf("LOADFIX: afterwards freeing block at %04Xh\n",
	loadfix_allocations[loadfix_ii]);
#endif
    }
}

static void perform_loadfix(const char *arg)
  {
  if (!*arg)
    {
    cprintf("loadfix: command name missing\r\n");
    reset_batfile_call_stack();
    return;
    }
  strcpy(cmd, arg);
  advance_cmd_arg();

  loadfix_init();
  loadfix_initialised = 1;

  perform_external_cmd(false, false, cmd);
	  /* Should we set this to true? Only affects batch files anyway,
	   * which shouldn't be loaded with LOADFIX to begin with. */

  loadfix_exit();
  loadfix_initialised = 0;
  }

static void perform_cd(const char *arg)
  {
  while (*cmd_switch)  // skip switches
    advance_cmd_arg();
  if (*arg)
    {
    unsigned cur_drive, dummy;
    int rc;

    if (arg[1] == ':')
      getdrive(&cur_drive);
    rc = chdir(arg);
    if (arg[1] == ':' && toupper(arg[0]) != 'A' + cur_drive - 1)
      setdrive(cur_drive, &dummy);
    if (rc != 0)
      {
      cprintf("Directory does not exist - %s\r\n",arg);
      error_level = 1;
      return;
      }
    }
  else
    {
    char cur_drive_and_path[MAXPATH];
    getcwd(cur_drive_and_path, MAXPATH);
    conv_unix_path_to_ms_dos(cur_drive_and_path);
    puts(cur_drive_and_path);
    }
  }

static void perform_change_drive(void)
  {
  char cur_drive_and_path[MAXPATH];
  unsigned int drive_set, cur_drive = 0, old_drive, dummy;
  drive_set = toupper(cmd[0])-'A'+1;
  getdrive(&old_drive);
  setdrive(drive_set, &dummy);
  getdrive(&cur_drive);
  if (cur_drive != drive_set)
    {
    cprintf("Invalid drive specification - %s\r\n", cmd);
    error_level = 1;
    return;
    }
  if (!getcwd(cur_drive_and_path, MAXPATH))
    {
    cprintf("Drive not ready - %s\r\n", cmd);
    setdrive(old_drive, &dummy);
    error_level = 1;
    return;
    }
  }

static void perform_choice(const char *arg)
  {
  const char *choices = "YN";  // Y,N are the default choices
  const char *text = "";
  int supress_prompt = false;
  int choice;

  while (*arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      text = cmd_args;
      break;
      }
    else
      {
      if (strnicmp(cmd_switch,"/c:", 3) == 0 && strlen(cmd_switch) > 3)
        choices = cmd_switch+3;
      else if (stricmp(cmd_switch,"/n") == 0)
        supress_prompt = true;
      else
        {
        cprintf("Invalid switch - %s\r\n", cmd_switch);
        reset_batfile_call_stack();
        return;
        }
      }
    advance_cmd_arg();
    }

  cputs(text);
  if (!supress_prompt)
    {
    int first = true;
    const char *c;

    putch('[');
    c = choices;
    while (*c != '\0')
      {
      if (first)
        first = false;
      else
        putch(',');
      putch(toupper(*c));
      c++;
      }
    cputs("]?");
    }
  choice = get_choice(choices);
  error_level = strchr(choices, choice) - choices + 1;
  return;
  }

static int expand_pluses(void)
{
  char cmd_args_bkp[MAX_CMD_BUFLEN];
  int len;
  char *p, *p2, *last_arg;

  strcpy(cmd_args_bkp, cmd_args);
  last_arg = strrchr(cmd_args_bkp, ' ');
  if (!last_arg)
    {
    cprintf("syntax error\n");
    return -1;
    }
  len = 0;
  cmd_args[0] = '\0';
  for (p2 = cmd_args_bkp, p = strchr(p2, '+'); p;
       p2 = p + 1, p = strchr(p2, '+'))
    {
    len += snprintf(cmd_args + len, sizeof(cmd_args) - len, "%.*s %s;",
        p - p2, p2, last_arg);
    }
  strlcat(cmd_args, p2, sizeof(cmd_args));
  return 0;
}

static void perform_copy(const char *arg)
  {
  int err = expand_pluses();
  if (err)
    return;
  general_file_transfer(FILE_XFER_COPY, 0);
  /* expand_pluses() delimited the arg groups with ; */
  while (cmd_args[0] == ';')
    {
    cmd_args[0] = ' ';
    extract_args(cmd_args);
    if (cmd_arg[0] != '\0')
      general_file_transfer(FILE_XFER_COPY, 1);
    }
  }

static void perform_xcopy(const char *arg)
  {
  general_file_transfer(FILE_XFER_XCOPY, 0);
  }

#define IS_CHRDEV(d) ((d) & _DEV_CDEV)

static void perform_ctty(const char *arg)
  {
  __dpmi_regs r = {};
  int dinfo;
  int fd;
  if (*arg == '\0')
    {
    cprintf("ctty: device name missing\r\n");
    return;
    }
  fd = open(arg, O_RDWR | O_TEXT);
  if (fd == -1)
    {
    cprintf("ctty: cannot open %s\r\n", arg);
    return;
    }
  dinfo = _get_dev_info(fd);
  if (dinfo == -1)
    {
    cprintf("ctty: %s is not a device\r\n", arg);
    goto err_close;
    }
  if (!IS_CHRDEV(dinfo))
    {
    cprintf("ctty: %s is not a char device\r\n", arg);
    goto err_close;
    }
  if ((dinfo & 3) != 0)
    {
    cprintf("ctty: %s is already a tty\r\n", arg);
    goto err_close;
    }
  if ((dinfo & 0x3c) != 0)
    {
    cprintf("ctty: %s has wrong type %x\r\n", arg, r.x.dx);
    goto err_close;
    }

  r.x.ax = 0x4401;
  r.x.bx = fd;
  r.x.dx = (dinfo & 0xff) | (_DEV_STDIN | _DEV_STDOUT);  // set ctty
  __dpmi_int(0x21, &r);
  if (r.x.flags & 1)
    {
    cprintf("ctty: failed to set ctty on %s\r\n", arg);
    goto err_close;
    }

  dinfo = _get_dev_info(0);
  if (dinfo != -1)
    {
    r.x.ax = 0x4401;
    r.x.bx = 0;
    r.x.dx = dinfo & 0xff & ~(_DEV_STDIN | _DEV_STDOUT); // clear ctty from old
    __dpmi_int(0x21, &r);
    if (r.x.flags & 1)
      cprintf("ctty: stdin clear ctty failed with %x\r\n", r.x.ax);
    }
  else
    cprintf("ctty: stdin ioctl failed\r\n");

  close(0);
  close(1);
  close(2);
  dup(fd);
  dup(fd);
  dup(fd);

err_close:
  close(fd);
  }

static void perform_date(const char *arg)
  {
  time_t t = time(NULL);
  struct tm loctime;
  const char *day_of_week[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  localtime_r(&t, &loctime);
  if (*arg != '\0')
    {
    struct timeval tv;
    unsigned int m, d, y;
    int rc = sscanf(arg, "%d-%d-%d", &m, &d, &y);
    if (rc != 3 || m == 0 || m > 12 || d == 0 || d > 31)
      {
      cprintf("Invalid date\r\n");
      reset_batfile_call_stack();
      return;
      }
    if (y < 100)
      {
      if (y < 80)
        y += 2000;
      else
        y += 1900;
      }
    else if (y < 1900)
      {
      cprintf("Invalid year\r\n");
      reset_batfile_call_stack();
      return;
      }
    if (y >= 2038)
      {
      cprintf("Invalid year: Y2K38\r\n");
      reset_batfile_call_stack();
      return;
      }
    loctime.tm_year = y - 1900;
    loctime.tm_mon = m - 1;
    loctime.tm_mday = d;
    tv.tv_sec = mktime(&loctime);
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    }
  else
    printf("Current date is %s %02d-%02d-%04d\n", day_of_week[loctime.tm_wday],
                             loctime.tm_mon+1, loctime.tm_mday, loctime.tm_year+1900);
  }

static void perform_delete(const char *arg)
  {
  long fhandle;
  finddata_t ff;
  char filespec[MAXPATH] = "";
  char full_filespec[MAXPATH] = "";
  char drive[MAXDRIVE], dir[MAXDIR];
  int done = 0;

  while (*arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*filespec == '\0')
        {
        strncpy(filespec, arg, MAXPATH);
        filespec[MAXPATH-1] = '\0';
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    advance_cmd_arg();
    }
  if (*filespec == '\0')
    {
    cprintf("filespec not specified\r\n");
    reset_batfile_call_stack();
    return;
    }

  _fixpath(filespec, full_filespec);
  fnsplit(full_filespec, drive, dir, NULL, NULL);
  conv_unix_path_to_ms_dos(drive);
  conv_unix_path_to_ms_dos(dir);

  if (findfirst_f(full_filespec, &ff, 0, &fhandle) != 0)
    {
    printf("File(s) not found - %s\n", filespec);  // informational msg; not an error
    return;
    }
  while (!done)
    {
    char individual_filespec[MAXPATH];

    conv_unix_path_to_ms_dos(FINDDATA_T_FILENAME(ff));
    strcpy(individual_filespec, drive);
    strcat(individual_filespec, dir);
    strcat(individual_filespec, FINDDATA_T_FILENAME(ff));

    done = (findnext_f(&ff, fhandle) != 0);
    if (remove(individual_filespec) == 0)
      printf("%s erased\n", individual_filespec);
    else
      {
      cprintf("Access denied - %s\r\n", individual_filespec);
      error_level = 1;
      return;
      }
    }
  }

static void perform_deltree(const char *arg)
  {
  long ffhandle = 0;
  int ffrc;
  int file_count = 0, dir_count = 0;
  int s, subdir_level = 0;
  int confirm_before_delete = true;
  finddata_t ff[MAX_SUBDIR_LEVEL];
  char dir_name[MAX_SUBDIR_LEVEL][MAXPATH];
  int remove_level1_dir;
  int visitation_mode[MAX_SUBDIR_LEVEL]; // 4 = findfirst source_filespec for files;
                                         // 3 = findnext source_filespec for files;
                                         // 2 = findfirst *.* for subdirs;
                                         // 1 = findnext *.* for subdirs;
                                         // 0 = done
  char drivespec[MAXDRIVE], dirspec[MAXDIR], filename[MAXFILE], extspec[MAXEXT];
  char temp_path[MAXPATH];
  char path[MAXPATH] = "", filespec[MAXPATH];
  char full_path_filespec[MAXPATH];
  int choice;
  unsigned search_attrib;

  while (*arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*path == '\0')
        {
        strncpy(path, arg, MAXPATH);
        path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(path);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      if (stricmp(cmd_switch,"/Y") == 0)
        confirm_before_delete = false;
      else
        {
        cprintf("Invalid switch - %s\r\n", cmd_switch);
        reset_batfile_call_stack();
        return;
        }
      }
    advance_cmd_arg();
    }
  if (*path == '\0')
    {
    cprintf("filespec not specified\r\n");
    reset_batfile_call_stack();
    return;
    }

  // prepare path for fnsplit() -
  // attach a file specification if specified path doesn't have one
  if (is_drive_spec(path) || has_trailing_slash(path))
    strcat(path, "*.*");

  // parse path - create full path and split into 2 components: path + file spec
  _fixpath(path, temp_path);
  fnsplit(temp_path, drivespec, dirspec, filename, extspec);
  strcpy(path, drivespec);
  strcat(path, dirspec);
  conv_unix_path_to_ms_dos(path);
  strcpy(filespec, filename);
  strcat(filespec, extspec);
  conv_unix_path_to_ms_dos(filespec);

  // visit each directory; delete files and subdirs
  visitation_mode[0] = 4;
  dir_name[0][0] = '\0';
  remove_level1_dir = false;
  while (subdir_level >= 0)
    {
    if (visitation_mode[subdir_level] == 4 || visitation_mode[subdir_level] == 2)
      {
      strcpy(full_path_filespec, path);
      for (s = 0; s <= subdir_level; s++)
        strcat(full_path_filespec, dir_name[s]);
      if (subdir_level == 0)
        strcat(full_path_filespec, filespec);
      else
        strcat(full_path_filespec, "*.*");
      if (visitation_mode[subdir_level] == 4)
        search_attrib = FA_RDONLY+FA_ARCH+FA_SYSTEM+FA_HIDDEN;
      else
        search_attrib = FA_DIREC+FA_RDONLY+FA_ARCH+FA_SYSTEM+FA_HIDDEN;
      ffrc = findfirst_f(full_path_filespec, &(ff[subdir_level]), search_attrib, &ffhandle);
      visitation_mode[subdir_level]--;
      }
    else
      ffrc = findnext_f(&(ff[subdir_level]), ffhandle);
    if (ffrc == 0)
      {
      conv_unix_path_to_ms_dos(FINDDATA_T_FILENAME(ff[subdir_level]));
      strcpy(full_path_filespec, path);
      for (s = 0; s <= subdir_level; s++)
        strcat(full_path_filespec, dir_name[s]);
      strcat(full_path_filespec, FINDDATA_T_FILENAME(ff[subdir_level]));

      if ((FINDDATA_T_ATTRIB(ff[subdir_level])&FA_DIREC) != 0)
        {
        if (visitation_mode[subdir_level] <= 2 &&
            strcmp(FINDDATA_T_FILENAME(ff[subdir_level]),".") != 0 &&
            strcmp(FINDDATA_T_FILENAME(ff[subdir_level]),"..") != 0)
          {
          if (subdir_level == 0)
            {
            if (confirm_before_delete)
              {
              cprintf("Delete directory %s and all its subdirectories? [Y/N] ", full_path_filespec);
              remove_level1_dir = (get_choice("YN") == 'Y');
              }
            else
              remove_level1_dir = true;
            }
          if (subdir_level > 0 || remove_level1_dir)
            {
            subdir_level++;
            if (subdir_level >= MAX_SUBDIR_LEVEL)
              {
              cputs("Directory tree is too deep\r\n");
              reset_batfile_call_stack();
              return;
              }
            visitation_mode[subdir_level] = 4;
            strcpy(dir_name[subdir_level], FINDDATA_T_FILENAME(ff[subdir_level-1]));
            strcat(dir_name[subdir_level], "\\");
            }
          }
        }
      else
        {
        if (visitation_mode[subdir_level] > 2)
          {
          if (confirm_before_delete && subdir_level == 0)
            {
            cprintf("Delete file %s ? [Y/N] ", full_path_filespec);
            choice = get_choice("YN");
            }
          else
            choice = 'Y';
          if (choice == 'Y')
            {
            if (remove(full_path_filespec) != 0)
              {
              cprintf("Unable to delete file - %s\r\n", full_path_filespec);
              error_level = 1;
              return;
              }
            if (subdir_level == 0)
              printf("%s deleted\n", full_path_filespec);
            file_count++;
            }
          }
        }
      }
    else
      {
      visitation_mode[subdir_level]--;
      if (visitation_mode[subdir_level] <= 0)
        {
        if (subdir_level > 0)
          {
          strcpy(full_path_filespec, path);
          for (s = 0; s <= subdir_level; s++)
            strcat(full_path_filespec, dir_name[s]);
          *(strrchr(full_path_filespec,'\\')) = '\0';
          if (subdir_level > 1 || remove_level1_dir)
            {
            if (rmdir(full_path_filespec) != 0)
              {
              cprintf("Unable to remove directory - %s\\\r\n", full_path_filespec);
              error_level = 1;
              return;
              }
            if (subdir_level == 1)
              printf("%s removed\n", full_path_filespec);
            dir_count++;
            }
          }
        subdir_level--;
        if (subdir_level >= 0)
          visitation_mode[subdir_level] = 4;  // restart from findfirst
        }
      }
    }
  printf("%9d file(s) deleted, ", file_count);
  if (dir_count == 1)
    printf("%9d directory removed\n", dir_count);
  else
    printf("%9d (sub)directories removed\n", dir_count);
  }

static void perform_dir(const char *arg)
  {
  long ffhandle;
  int ffrc;
  int wide_column_countdown = -1;
  int use_pause = 0;
  unsigned long long avail; //was double avail; --Salvo
  finddata_t ff;
  struct statfs sf;
  int rc;
  unsigned int attrib = FA_DIREC+FA_RDONLY+FA_ARCH+FA_SYSTEM+FA_HIDDEN, first;
  unsigned long filecount = 0, dircount = 0, bytecount = 0;
  char dirspec[MAXPATH];
  char volspec[7] = "X:\\*.*";
  char full_filespec[MAXPATH];
  char filespec[MAXPATH] = "";
  struct text_info txinfo;

  gettextinfo(&txinfo);

  while (*arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*filespec == '\0')
        {
        strncpy(filespec, arg, MAXPATH);
        filespec[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(filespec);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      if (stricmp(cmd_switch,"/w")==0)
        wide_column_countdown = 5;
      if (stricmp(cmd_switch,"/p")==0)
        {
        use_pause = 1;
        clrscr();
        }
      }
    advance_cmd_arg();
    }

  if (!has_trailing_slash(filespec) && !has_wildcard(filespec) &&
      findfirst_f(filespec, &ff, FA_DIREC, NULL) == 0)
    strcat(filespec, "\\");

  if (*filespec == '\0' || is_drive_spec(filespec) ||
      has_trailing_slash(filespec))
    strcat(filespec, "*.*");
  _fixpath(filespec, full_filespec);
  conv_unix_path_to_ms_dos(full_filespec);

  volspec[0] = full_filespec[0];
  if (findfirst_f(volspec, &ff, FA_LABEL, NULL) == 0)
    {
    char *p = strchr(FINDDATA_T_FILENAME(ff), '.');
    if (p)
      memmove(p, p + 1, strlen(p + 1) + 1);
    printf(" Volume in drive %c is %s\n", volspec[0], FINDDATA_T_FILENAME(ff));
    }
  else
    {
    struct _DOSERROR derr;
    int err = _dosexterr(&derr);
    if (err == 3)
      {
      puts("Invalid drive specification");
      return;
      }
    puts(" Volume has no label");
    }

  fnsplit (full_filespec, NULL, dirspec, NULL, NULL);
  printf(" Directory of %c:%s\n\n", full_filespec[0], dirspec);

  first = true;
  for (;;)
    {
    if (use_pause && wherey() == txinfo.winbottom)
      {
      printf("Press any key to continue...");
      getch();
      clrscr();
      }
    if (first)
      {
      if ((ffrc = findfirst_f(full_filespec, &ff, attrib, &ffhandle)) != 0)
        {
        puts("File not found");  // informational message -- not an error
        return;
        }
      first = false;
      }
    else
      {
      if ((ffrc = findnext_f(&ff, ffhandle)) != 0)
        break;
      }
    conv_unix_path_to_ms_dos(FINDDATA_T_FILENAME(ff));
    if (wide_column_countdown < 0)
      {
      printf("%04d-%02d-%02d ", FINDDATA_T_WDATE_YEAR(ff), FINDDATA_T_WDATE_MON(ff), FINDDATA_T_WDATE_DAY(ff));
      printf("%02d:%02d ", FINDDATA_T_WTIME_HOUR(ff), FINDDATA_T_WTIME_MIN(ff));
      if ((FINDDATA_T_ATTRIB(ff)&FA_DIREC) == 0)
        printf("%13u", FINDDATA_T_SIZE(ff));
      else
        printf("<DIR>%8s", "");
      printf(" %s\n", FINDDATA_T_FILENAME(ff));
      }
    else
      {
      if ((FINDDATA_T_ATTRIB(ff)&FA_DIREC) == 0)
        printf("%-14s", FINDDATA_T_FILENAME(ff));
      else
        {
        int len = strlen(FINDDATA_T_FILENAME(ff)) + 2;
        printf("[%s]", FINDDATA_T_FILENAME(ff));
        while (len < 14)
          {
          printf(" ");
          len++;
          }
        }
      wide_column_countdown--;
      if (wide_column_countdown == 0)
        {
        puts("");
        wide_column_countdown = 5;
        }
      else
        printf("  ");
      }

    if ((FINDDATA_T_ATTRIB(ff)&FA_DIREC) == 0)
      {
      filecount++;
      bytecount += FINDDATA_T_SIZE(ff);
      }
    else
      dircount++;
    }
  if (wide_column_countdown >= 0 && wide_column_countdown < 5)
    puts("");
  printf("%10lu file(s) %14lu bytes\n", filecount, bytecount);
  printf("%10lu dir(s) ", dircount);

  rc = statfs(full_filespec, &sf);
  if (rc == 0) {
    avail = (unsigned long long)sf.f_bavail * sf.f_bsize;
    if (avail < 1048576)
      printf("%15lli byte(s) free\n", avail);
    else if (avail < 1073741824)
      printf("%15lli KB free\n", avail / 1024);
    else if (avail < 2147483648ULL)
      printf("%15lli MB free\n", avail / 1024 / 1024);
    else
      printf("%15.1f GB free\n", avail / 1024.0 / 1024.0 / 1024.0);
    }
  }

static void perform_echo(const char *arg)
  {
  if (stricmp(arg, "off") == 0)
    echo_on[stack_level] = false;
  else if (stricmp(arg, "on") == 0)
    echo_on[stack_level] = true;
  else if (arg[0] == '\0')
    {
    if (echo_on[stack_level])
      puts("ECHO is on");
    else
      puts("ECHO is off");
    }
  else
    puts(cmd_args);
  }

static void perform_break(const char *arg)
  {
  if (stricmp(arg, "off") == 0)
    break_on = false;
  else if (stricmp(arg, "on") == 0)
    break_on = true;
  else if (arg[0] == '\0')
    printf("BREAK is %s\n", break_on ? "on" : "off");
  else
    {
    cprintf("Syntax error\r\n");
    reset_batfile_call_stack();
    }
  }

static void perform_echo_dot(const char *arg)
  {
  if (arg[0] == '\0')
    puts("");
  else
    puts(cmd_args);
  }

static void perform_exit(const char *arg)
  {
  int ba;
  int is_bat = bat_file_path[stack_level][0];
  bat_file_path[stack_level][0] = '\0';
  for (ba = 0; ba < MAX_BAT_ARGS; ba++)
    bat_arg[stack_level][ba][0] = '\0';
  bat_file_line_number[stack_level] = 0;
  echo_on[stack_level] = true;
  if (stack_level > 0)
    stack_level--;
  else
    {
    if (!shell_permanent || (getenv("SHELL_ALLOW_EXIT") && !is_bat))
      {
      exiting++;
      if (arg)
        {
        if (arg[0])
          error_level = atoi(arg);
        else
          error_level = 0;
        }
      }
    }
  }

struct for_iter {
  char *token;
  int glob_idx;
  int glob_state;
  glob_t gl;
  const char *end;
  char *sptr;
};

static void advance_iter(struct for_iter *iter)
  {
  char *tok = strtok_r(NULL, " )", &iter->sptr);
  iter->token = ((tok && tok < iter->end) ? tok : NULL);
  }

static const char *extract_token(struct for_iter *iter)
  {
  const char *tok;

  if (iter->glob_state == 2)
    {
    globfree(&iter->gl);
    iter->glob_state = 0;
    }

again:
  if (!iter->token)
    return NULL; // no more tokens
  if (iter->glob_state)
    {
    tok = iter->gl.gl_pathv[iter->glob_idx++];
    if (iter->glob_idx >= iter->gl.gl_pathc)
      {
      iter->glob_state = 2;
      advance_iter(iter);
      }
    return tok;
    }
  if (!has_wildcard(iter->token))
    {
    tok = iter->token;
    advance_iter(iter);
    return tok;
    }
  if (glob(iter->token, 0, NULL, &iter->gl))
    {
    advance_iter(iter);
    goto again;
    }
  tok = iter->gl.gl_pathv[0];
  if (iter->gl.gl_pathc > 1)
    {
    iter->glob_state = 1;
    iter->glob_idx = 1;
    }
  else
    {
    iter->glob_state = 2;
    advance_iter(iter);
    }
  return tok;
  }

static void perform_for(const char *arg)
  {
  const char *tok;
  char cmd_args2[MAX_CMD_BUFLEN];
  struct for_iter iter = {};
  char *p, *p1, *d0, *d1, *d, *c;
  const char *v;

  strcpy(cmd_args2, cmd_args);
  p = strchr(cmd_args2, '(');
  p1 = strchr(cmd_args2, ')');
  d0 = strstr(cmd_args2, " DO ");
  d1 = strstr(cmd_args2, " do ");
  d = d0 ?: d1;
  if (!p || !p1 || p1 < p || !d)
    {
    cprintf("Syntax error\r\n");
    reset_batfile_call_stack();
    return;
    }
  v = arg;
  if (*v == '%')
    v++;
  for_var = *v;
  p++;
  iter.token = strtok_r(p, " )", &iter.sptr);
  iter.end = p1;
  c = d + 4;
  while (*c == ' ')
    c++;
  while ((tok = extract_token(&iter)))
    {
    strlcpy(cmd_line, c, sizeof(cmd_line));
    for_val = tok;
    parse_cmd_line();
    exec_cmd(false);
    }
  for_var = '\0';
  }

#define VIDADDR(r,c) (0xb8000 + 2*(((r) * txinfo.screenwidth) + (c)))
#if 0
static void reset_text_attrs(void)
{
  char attr = 7;
  struct text_info txinfo;
  int row, col;

  gettextinfo(&txinfo);
  for (row=txinfo.wintop-1; row < txinfo.winbottom; row++)
    {
    for (col=0; col < txinfo.winright - txinfo.winleft + 1; col++)
      dosmemput(&attr, 1, VIDADDR(row, txinfo.winleft - 1 + col) + 1);
    }
}
#endif

static int orig_strat, orig_umblink;

static void loadhigh_init(void)
  {
  __dpmi_regs r = {};

  r.x.ax = 0x5800;
  __dpmi_int(0x21, &r);
  orig_strat = r.x.ax;
  r.x.ax = 0x5802;
  __dpmi_int(0x21, &r);
  orig_umblink = r.h.al;
  }

static void loadhigh_done(void)
  {
  __dpmi_regs r = {};

  r.x.ax = 0x5801;
  r.x.bx = orig_strat;
  __dpmi_int(0x21, &r);
  r.x.ax = 0x5803;
  r.x.bx = orig_umblink;
  __dpmi_int(0x21, &r);
  }

static int is_HMA_enabled(void)
  {
  __dpmi_regs r = {};

  r.x.ax = 0x3306;
  __dpmi_int(0x21, &r);
  return !!(r.h.dh & 0x10);
  }

static void perform_external_cmd(int call, int lh, char *ext_cmd)
  {
  finddata_t ff;
  long ffhandle;
  char cmd_name[MAX_CMD_BUFLEN];
  char *pathvar, pathlist[200];
  char full_cmd[MAXPATH+MAX_CMD_BUFLEN] = "";
  char temp_cmd[MAXPATH+MAX_CMD_BUFLEN];
  int rc;
  int exec_type, e, ba;
  const char *exec_ext[3] = {".COM",".EXE",".BAT"};
  char *s;

  // No wildcards allowed -- reject them
  if (has_wildcard(ext_cmd))
    goto BadCommand;

  // Assemble a path list, and also extract the command name without the path.
  // For commands that already specify a path, the path list will consist of
  // only that path.
  s = strrchr(ext_cmd, '\\');
  if (s != NULL)
    {
    s++;
    strncpy(pathlist, ext_cmd, s-ext_cmd);
    pathlist[s-ext_cmd] = '\0';
    }
  else
    {
    s = strchr(ext_cmd, ':');
    if (s != NULL)
      {
      s++;
      strncpy(pathlist, ext_cmd, s-ext_cmd);
      pathlist[s-ext_cmd] = '\0';
      }
    else
      {
      strcpy(pathlist, ".\\;");
      s = ext_cmd;
      pathvar = getenv("PATH");
      if(pathvar != NULL)
        {
        strncat(pathlist, pathvar, 200 - strlen(pathlist) - 1);
        pathlist[sizeof(pathlist)-1] = '\0';
        }
      }
    }
  strcpy(cmd_name, s);

  // Search for the command
  exec_type = -1;
  pathvar = strtok(pathlist, "; ");
  while (pathvar != NULL)
    {
    // start to build full command name (sort of, because it still could be missing .exe, .com, or .bat)
    strcpy(full_cmd, pathvar);
    s = strchr(full_cmd, '\0');
    if (strlen(full_cmd) > 0)
      {
      if(*(s-1)!=':'&&*(s-1)!='\\')
        {
        *s = '\\';
        s++;
        *s = '\0';
        }
      }
    if (stricmp(full_cmd,".\\") == 0)
      full_cmd[0] = '\0';
    strcat(full_cmd, cmd_name);
    _fixpath(full_cmd, temp_cmd);
    strcpy(full_cmd, temp_cmd);
    conv_unix_path_to_ms_dos(full_cmd);

    // check validity for each executable type
    for (e = 0; e < 3; e++)
      {
      s = strchr(cmd_name, '.');
      if (s == NULL)  // no file type mentioned
        {
        s = strchr(full_cmd, '\0');  // save position of the nul terminator
        strcat(full_cmd, exec_ext[e]);
        if (findfirst_f(full_cmd, &ff, 0, &ffhandle) == 0)
          {
          exec_type = e;
          break;
          }
        *s = '\0'; // restore nul terminator
        }
      else
        {
        if (stricmp(s, exec_ext[e]) == 0)
          {
          if (findfirst_f(full_cmd, &ff, 0, &ffhandle) == 0)
            {
            exec_type = e;
            break;
            }
          }
        }
      }

    if (exec_type < 0)  // if exec file not found yet...
      pathvar = strtok(NULL, "; ");  // try next path in path list
    else
      pathvar = NULL;   // command found...
    }

  if (exec_type < 0)
    goto BadCommand;

  strupr(full_cmd);

  if (exec_type == 2)  // if command is a batch file
    {
    if (call || getenv("SHELL_CALL_DEFAULT"))
      {
      stack_level++;
      if (stack_level >= MAX_STACK_LEVEL)
        goto StackOverflow;
      }
    else
      bat_file_line_number[stack_level] = 0;
    strcpy(bat_file_path[stack_level], full_cmd);
    ba = 0;
    /* keep last entry empty to simplify shifting */
    while (ba < MAX_BAT_ARGS - 1 && *cmd_arg != '\0')
      {
      strcpy(bat_arg[stack_level][ba], cmd_arg);
      advance_cmd_arg();
      ba++;
      }
    }
  else
    {
    unsigned do_auto_loadfix = 0;
    char el[16];
    int alen;
    FILE * exefile;
    char *lh_d;

    if (mouse_en && !mouseopt_extctl)
      mouse_disable();
#if SYNC_ENV
    /* the below is disabled because it seems we don't need
     * to update our copy of env. djgpp creates the env segment
     * for the child process from the prot-mode environment anyway.
     * Disabling allows to pass much fewer memory to /E.
     * But we still need the /E because some programs (msetenv)
     * may set the env strings on their parent (shell) to make
     * them permanent. */
    put_env();
#else
    set_env("PATH", getenv("PATH"));
#endif
    _control87(0x033f, 0xffff);
#ifdef __DJGPP__
    __djgpp_exception_toggle();
#endif
    set_env_seg();
    /* prepend command tail with space */
    alen = strlen(cmd_args);
    if (alen)
      {
      alen++;  // \0
      if (alen >= MAX_CMD_BUFLEN)
        {
        alen = MAX_CMD_BUFLEN - 1;
        cmd_args[alen - 1] = '\0';
        }
      memmove(cmd_args + 1, cmd_args, alen);
      cmd_args[0] = ' ';
      }
    if (!loadfix_initialised && is_HMA_enabled() &&
        (exefile = fopen(full_cmd,"rb")))
      {
      /* from https://github.com/dosemu2/comcom32/issues/59#issuecomment-1179566783 */
      unsigned char exebuffer[256] = { 0 };
      unsigned is_mz_exe = 0;
      fread(exebuffer, 1, 256, exefile);
      if (exebuffer[0] == 'M' && exebuffer[1] == 'Z')
        is_mz_exe = 1;
      if (exebuffer[0] == 'Z' && exebuffer[1] == 'M')
        is_mz_exe = 1;
      if (is_mz_exe && exebuffer[16] == 128 && exebuffer[17] == 0
        && (exebuffer[20] == 16 || exebuffer[20] == 18) && exebuffer[21] == 0) {
        unsigned headersize = (exebuffer[8] + exebuffer[9] * 256UL) * 16UL;
        short codesegment = exebuffer[22] + exebuffer[23] * 256UL;
        unsigned checkoffset = headersize + ((int)codesegment * 16UL);
        unsigned char entrybuffer[18] = { 0 };
        fseek(exefile, checkoffset, SEEK_SET);
        fread(entrybuffer, 1, 18, exefile);
        if (entrybuffer[exebuffer[20] - 2UL] == 'R'
          && entrybuffer[exebuffer[20] - 1UL] == 'B') {
          do_auto_loadfix = 1;
        }
      }
      if (is_mz_exe      && !memcmp(&exebuffer[30], "PKLITE", 6))
        do_auto_loadfix = 1;
      else if (is_mz_exe && !memcmp(&exebuffer[30], "PKlite", 6))
        do_auto_loadfix = 1;
      else if (!is_mz_exe && !memcmp(&exebuffer[46], "PKLITE", 6))
        do_auto_loadfix = 1;
      else if (!is_mz_exe && !memcmp(&exebuffer[48], "PKLITE", 6))
        do_auto_loadfix = 1;
      else if (!is_mz_exe && !memcmp(&exebuffer[46], "PKlite", 6))
        do_auto_loadfix = 1;
      else if (!is_mz_exe && !memcmp(&exebuffer[48], "PKlite", 6))
        do_auto_loadfix = 1;
      else if (!is_mz_exe && !memcmp(&exebuffer[38], "PK Copyr", 8))
        do_auto_loadfix = 1;
      fclose(exefile);
      exefile = NULL;
      }

    if (do_auto_loadfix)
      loadfix_init();
    lh_d = getenv("SHELL_LOADHIGH_DEFAULT");
    if ((lh_d && lh_d[0] == '1'))
      lh++;
    if (lh_d)
      unsetenv("SHELL_LOADHIGH_DEFAULT");
    if (lh)
      link_umb(0x80);
    set_break(1);
#ifdef HAVE_DOS_EXEC5
    rc = _dos_exec5(full_cmd, cmd_args, environ, NULL, lh ? 0x80 : 0);
#else
    rc = _dos_exec(full_cmd, cmd_args, environ, NULL);
#endif
    set_break(0);
    if (rc == -1)
      cprintf("Error: unable to execute %s\r\n", full_cmd);
    else
      error_level = rc & 0xff;
    if (lh)
      unlink_umb();
    if (do_auto_loadfix)
      loadfix_exit();
    set_env_sel();
    get_env();
#ifdef __DJGPP__
    __djgpp_exception_toggle();
#endif
    _control87(0x033f, 0xffff);
    _clear87();
    _fpreset();
//    reset_text_attrs();
    gppconio_init();  /* video mode could change */
    if (mouse_en && mouseopt_enabled)
      mouse_enable();

    sprintf(el, "%d", error_level);
    setenv("ERRORLEVEL", el, 1);
    }
  return;

BadCommand:
  cprintf("Bad command or file name - %s\r\n", ext_cmd);
  //reset_batfile_call_stack();  -- even if error becuase the command is not found, don't clean up
  return;

StackOverflow:
  cputs("Call stack overflow\r\n");
  reset_batfile_call_stack();
  return;
  }

static void perform_goto(const char *arg)
  {
  if (bat_file_path[stack_level][0] != '\0')
    {
    strcpy(goto_label, arg);
    bat_file_line_number[stack_level] = MAXINT;
    }
  else
    cputs("Goto not valid in immediate mode.\r\n");
  }

static void perform_help(const char *arg)
  {
  list_cmds();
  }

static void perform_if(void)
  {
  long ffhandle;
  int not_flag = false;
  int condition_fulfilled = false;

  if (cmd_arg[0] == '\0')
    goto SyntaxError;

  if (stricmp(cmd_arg, "not") == 0)
    {
    not_flag = true;
    advance_cmd_arg();
    }

  if (stricmp(cmd_arg, "exist") == 0)  // conditional is "exist <filename>"
    {
    char *s;
    finddata_t ff;

    advance_cmd_arg();
    s = strrchr(cmd_arg, '\\');
    if (s != NULL)
      {
      if (stricmp(s,"\\nul") == 0)
        *s = '\0';
      }
    if (file_access(cmd_arg, F_OK) == 0 || file_access(cmd_arg, D_OK) == 0)
      condition_fulfilled = true;
    else if (findfirst_f(cmd_arg, &ff, 0, &ffhandle) == 0)
      {
      findclose_f(ffhandle);
      condition_fulfilled = true;
      }
    }
  else if (strnicmp(cmd_args, "errorlevel", 10) == 0) //conditional is "errolevel x"
    {
    char *s;
    unsigned ecomp;

    s = cmd_args+10;
    if (*s != ' ' && *s != '\t' && *s != '=')
      goto SyntaxError;
    while (*s == ' ' || *s == '\t' || *s == '=')
      {
      *s = 'x';
      s++;
      }
    if (sscanf(s, "%u", &ecomp) == 1)
      {
      if (error_level >= ecomp)
        {
        while (*s != ' ' && *s != '\t')
          {
          *s = 'x';
          s++;
          }
        condition_fulfilled = true;
        }
      }
    }
  else // assume the conditional is in the form " xxxxxxxx  ==  yyyyy "
    {  //                                      op1^ op1end^ ^eq ^op2 ^op2end
    int len;
    char *op1, *op1end, *eq, *op2, *op2end;

    op1 = cmd_args;
    while (*op1 == ' ' || *op1 == '\t')
      op1++;

    if (op1[0] == '\"')
      {
      op1end = strchr(op1 + 1, '\"');
      if (op1end)
        op1end++;
      else
        goto SyntaxError;
      }
    else
      {
      op1end = op1;
      while (*op1end != ' ' && *op1end != '\t' && *op1end != '\0' && *op1end != '=')
        op1end++;
      }
    if (*op1end == '\0')
      goto SyntaxError;
    len = op1end - op1;
    if (len == 0)
      goto SyntaxError;

    eq = op1end;
    while (*eq != '\0' && *eq != '=')
      eq++;
    if (*eq != '=')
      goto SyntaxError;
    eq++;
    if (*eq != '=')
      goto SyntaxError;
    while (*eq == '=')
      eq++;

    op2 = eq;
    while (*op2 == ' ' || *op2 == '\t')
      op2++;

    if (op2[0] == '\"')
      {
      op2end = strchr(op2 + 1, '\"');
      if (op2end)
        op2end++;
      else
        goto SyntaxError;
      }
    else
      {
      op2end = op2;
      while (*op2end != ' ' && *op2end != '\t' && *op2end != '\0')
        op2end++;
      }
    if (op2 == op2end)
      goto SyntaxError;

    if (len == (op2end - op2))
      {
      if (strnicmp(op1, op2, len) == 0)
        condition_fulfilled = true;
      }

    while (op1 != op2end)
      {
      *op1 = 'x';
      op1++;
      }

    }

  advance_cmd_arg();

  if ((not_flag && !condition_fulfilled) ||
      (!not_flag && condition_fulfilled))
    {
    strcpy(cmd, cmd_arg);
    advance_cmd_arg();
    }
  else
    cmd[0] = '\0';

  return;

SyntaxError:
  cputs("Syntax error\r\n");
  reset_batfile_call_stack();
  cmd_line[0] = '\0';
  parse_cmd_line();  // this clears cmd[], cmd_arg[], cmd_switch[], and cmd_args[]
  return;
  }

static void perform_md(const char *arg)
  {
  while (*cmd_switch)  // skip switches
    advance_cmd_arg();
  if (*arg)
    {
    if (file_access(arg, D_OK) != 0)
      {
      if (_mkdir(arg) != 0)
        {
        cprintf("Could not create directory - %s\r\n", arg);
        error_level = 1;
        }
      }
    else
      {
      cprintf("Directory already exists - %s\r\n", arg);
      error_level = 1;
      }
    }
  else
    {
    cputs("Required parameter missing");
    reset_batfile_call_stack();
    }
  }

static void perform_more(const char *arg)
  {
  struct text_info txinfo;

  gettextinfo(&txinfo);
  int c, cnt = 0;
  while ((c = getchar()) != EOF)
    {
    putchar(c);
    if (c == '\n')
      {
      if (++cnt == txinfo.winbottom - 1)
        {
        cnt = 0;
        printf("--More--");
        fgetc(bkp_stdin);
        }
      }
    }
  }

static void perform_mouseopt(const char *arg)
  {
  if (arg[0] != '\0')
    {
    int opt = 1;
    if (isdigit(arg[2]))
      opt = arg[2] - '0';

    if (strnicmp(arg, "/C", 2) == 0 && mouseopt_extctl != opt)
      mouseopt_extctl = opt;
    if (strnicmp(arg, "/E", 2) == 0 && mouseopt_enabled != opt)
      {
      if (mouse_en)
        {
        if (opt)
          {
          mouse_enable();
          mouseopt_enabled = 1;
          }
        else
          {
          mouseopt_enabled = 0;
          mouse_disable();
          }
        }
      }
    if (stricmp(arg, "/M") == 0 && opt == 1)
      {
      if (!mouse_en)
        mouse_en = mouse_init();
      }
    }
  else
    {
    printf("mouseopt [/M] [/C[0|1]] [/E[1|0]]\n\n");
    printf("mouse initialized (/M):\t\t%i\n", mouse_en);
    printf("mouse enabled (/E):\t\t%i\n", mouseopt_enabled);
    printf("mouse external control (/C):\t%i\n", mouseopt_extctl);
    }
  }

static void perform_move(const char *arg)
  {
  general_file_transfer(FILE_XFER_MOVE, 0);
  }

static void perform_null_cmd(void)
  {
  }

static void perform_path(const char *arg)
  {
  int off = 0;
  if (*cmd_args == '\0')
    {
    char *pathvar = getenv("PATH");
    printf("PATH=");
    if (pathvar == NULL)
      puts("");
    else
      puts(pathvar);
    }
  else
    {
    if (*cmd_args == '=')    /* support PATH= syntax */
      off++;
    memmove(cmd_args+5, cmd_args + off, strlen(cmd_args)+1);
    memcpy(cmd_args, "PATH=", 5);
    perform_set(cmd_args);
    }
  }

static void perform_pause(const char *arg)
  {
  cputs("Press any key to continue . . .\r\n");
  getch();
  }

static void perform_popd(const char *arg)
  {
  if (pushd_stack_level == 0)
    {
    printf("pushd stack empty\n");
    reset_batfile_call_stack();
    return;
    }
  if (chdir(pushd_stack[--pushd_stack_level]) != 0)
    {
    cprintf("Directory does not exist - %s\r\n",arg);
    error_level = 1;
    return;
    }
  }

static void perform_prompt(const char *arg)
  {
  if (!arg[0])
    {
    char *promptvar = getenv("PROMPT");
    if (promptvar)
      printf("%s\n", promptvar);
    return;
    }
  memmove(cmd_args+7, cmd_args, strlen(cmd_args)+1);
  memcpy(cmd_args, "PROMPT=", 7);
  perform_set(arg);
  }

static void perform_pushd(const char *arg)
  {
  if (pushd_stack_level >= MAX_STACK_LEVEL)
    {
    printf("pushd stack overflow\n");
    reset_batfile_call_stack();
    return;
    }
  getcwd(pushd_stack[pushd_stack_level], MAXPATH);
  if (arg && arg[0])
    {
    if (chdir(arg) != 0)
      {
      cprintf("Directory does not exist - %s\r\n",arg);
      error_level = 1;
      return;
      }
    }
  pushd_stack_level++;
  }

static void perform_r200fix(const char *arg)
  {
  if (!*arg)
    {
    cprintf("r200fix: command name missing\r\n");
    reset_batfile_call_stack();
    return;
    }
  strcpy(cmd, arg);
  advance_cmd_arg();

  int0_wa = 1;
  perform_external_cmd(false, false, cmd);
  int0_wa = 0;
  }

static void perform_rd(const char *arg)
  {
  while (*cmd_switch)  // skip switches
    advance_cmd_arg();
  if (*cmd_arg)
    {
    if (rmdir(arg) != 0)
      {
      cprintf("Could not remove directory - %s\r\n", arg);
      error_level = 1;
      }
    }
  else
    {
    cputs("Required parameter missing");
    reset_batfile_call_stack();
    }
  }

static void perform_rename(const char *arg)
  {
  long ffhandle;
  int ffrc;
  int first, zfill;
  finddata_t ff;
  unsigned attrib = FA_RDONLY+FA_ARCH+FA_SYSTEM;
  char from_path[MAXPATH] = "", to_path[MAXPATH] = "";
  char from_drive[MAXDRIVE], to_drive[MAXDRIVE];
  char from_dir[MAXDIR], to_dir[MAXDIR];
  char from_name[MAXFILE], to_name[MAXFILE], new_to_name[MAXFILE];
  char from_ext[MAXEXT], to_ext[MAXEXT], new_to_ext[MAXEXT];
  char full_from_filespec[MAXPATH], full_to_filespec[MAXPATH];
  char *w;

  while (*arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*from_path == '\0')
        {
        strncpy(from_path, arg, MAXPATH);
        from_path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(from_path);
        }
      else if (*to_path == '\0')
        {
        strncpy(to_path, arg, MAXPATH);
        to_path[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(to_path);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      cprintf("Invalid switch - %s\r\n", cmd_args);
      reset_batfile_call_stack();
      return;
      }
    advance_cmd_arg();
    }

  _fixpath(from_path, full_from_filespec);
  conv_unix_path_to_ms_dos(full_from_filespec);
  fnsplit(full_from_filespec, from_drive, from_dir, from_name, from_ext);
  if (has_wildcard(from_dir) || has_trailing_slash(from_path))
    {
    cprintf("Invalid parameter - %s\r\n", from_path);
    reset_batfile_call_stack();
    return;
    }

  _fixpath(to_path, full_to_filespec);
  conv_unix_path_to_ms_dos(full_to_filespec);
  fnsplit(full_to_filespec, to_drive, to_dir, to_name, to_ext);
  if (stricmp(from_drive, to_drive) != 0 ||
      stricmp(from_dir, to_dir) != 0 ||
      has_trailing_slash(to_path))
    {
    cprintf("Invalid parameter - %s\r\n", to_path);
    reset_batfile_call_stack();
    return;
    }

  first = true;
  for (;;)
    {
    if (first)
      {
      if ((ffrc = findfirst_f(full_from_filespec, &ff, attrib, &ffhandle)) != 0)
        {
        cprintf("File not found - %s\r\n", from_path);
        error_level = 1;
        return;
        }
      first = false;
      }
    else
      {
      if ((ffrc = findnext_f(&ff, ffhandle)) != 0)
        break;
      }

    strcpy(full_from_filespec, from_drive);
    strcat(full_from_filespec, from_dir);
    strcat(full_from_filespec, FINDDATA_T_FILENAME(ff));
    conv_unix_path_to_ms_dos(full_from_filespec);
    fnsplit(full_from_filespec, NULL, NULL, from_name, from_ext);
    for (zfill = strlen(from_name); zfill < MAXFILE; zfill++)
      from_name[zfill] = '\0';
    for (zfill = strlen(from_ext); zfill < MAXEXT; zfill++)
      from_ext[zfill] = '\0';

    strcpy(full_to_filespec, from_drive);
    strcat(full_to_filespec, from_dir);
    strcpy(new_to_name, to_name);
    strcpy(new_to_ext, to_ext);
    while ((w = strchr(new_to_name, '?')) != NULL)
      *w = from_name[w-new_to_name];
    if ((w = strchr(new_to_name, '*')) != NULL)
      strcpy(w, &(from_name[w-new_to_name]));
    while ((w = strchr(new_to_ext, '?')) != NULL)
      *w = from_ext[w-new_to_ext];
    if ((w = strchr(new_to_ext, '*')) != NULL)
      strcpy(w, &(from_ext[w-new_to_ext]));
    fnmerge(full_to_filespec, to_drive, to_dir, new_to_name, new_to_ext);
    conv_unix_path_to_ms_dos(full_to_filespec);
    if (stricmp(full_from_filespec, full_to_filespec) != 0)
      {
      conv_unix_path_to_ms_dos(new_to_name);
      conv_unix_path_to_ms_dos(new_to_ext);
      if (rename(full_from_filespec, full_to_filespec) == 0)
        printf("%s renamed to %s%s\n", full_from_filespec, new_to_name, new_to_ext);
      else
        {
        cprintf("Unable to rename %s to %s%s\n", full_from_filespec, new_to_name, new_to_ext);
        error_level = 1;
        return;
        }
      }
    }
  }

static void perform_set(const char *arg)
  {
  const char *var_name;
  char *vname;
  int err;

  if (*arg == '\0')
    {
    int i = 0;
    while (environ[i])
      printf("%s\n", environ[i++]);
    }
  else
    {
    char *s;
    int is_p = 0;
    if (strnicmp(cmd_switch,"/p", 2) == 0)
      {
      is_p++;
      advance_cmd_arg();
      }
    var_name = cmd_args;
    if (strlen(var_name) == 0)
      {
      cputs("Syntax error\r\n");
      reset_batfile_call_stack();
      return;
      }
    vname = strdup(var_name);
    s = strchr(vname, '=');
    if (s)
      {
      *s = '\0';
      s++;
      }
    strupr(vname);
    if (is_p)
      {
      char buf[128];
      char *p;
      cputs(s);
      p = fgets(buf, sizeof(buf), stdin);
      if (p)
        {
        p = strpbrk(buf, "\r\n");
        if (p)
          *p = '\0';
        err = setenv(vname, buf, 1);
        }
      else
        err = -1;
      }
    else
      {
      if (!s || !*s)
        err = unsetenv(vname);
      else
        err = setenv(vname, s, 1);
      }
    free(vname);
    if (err != 0)
      {
      cprintf("Error setting environment variable - %s\r\n", var_name);
      error_level = 1;
      return;
      }
    }
  }

static void perform_shift(const char *arg)
  {
  int i;
  for (i = 0; i < MAX_BAT_ARGS - 1; i++)
    {
    strcpy(bat_arg[stack_level][i], bat_arg[stack_level][i + 1]);
    /* check _after_ copy to zero out last entry */
    if (!bat_arg[stack_level][i + 1][0])
      break;
    }
  }

static void perform_time(const char *arg)
  {
  time_t t = time(NULL);
  struct tm loctime;
  unsigned int hour, min, sec = 0;
  int rc, exp_rc;

  localtime_r(&t, &loctime);
  if (*arg != '\0')
    {
    struct timeval tv;
    char *p1 = strchr(arg, ':'), *p2 = strrchr(arg, ':');
    if (!p1)
      {
      cprintf("Invalid time format\r\n");
      reset_batfile_call_stack();
      return;
      }
    if (p1 == p2)
      {
      rc = sscanf(arg, "%d:%d", &hour, &min);
      exp_rc = 2;
      }
    else
      {
      rc = sscanf(arg, "%d:%d:%d", &hour, &min, &sec);
      exp_rc = 3;
      }
    if (rc != exp_rc || hour > 23 || min > 59 || sec > 59)
      {
      cprintf("Invalid time\r\n");
      reset_batfile_call_stack();
      return;
      }
    loctime.tm_hour = hour;
    loctime.tm_min = min;
    loctime.tm_sec = sec;
    tv.tv_sec = mktime(&loctime);
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    }
  else
    printf("Current time is %d:%02d:%02d\n",
         loctime.tm_hour, loctime.tm_min, loctime.tm_sec);

  }

static void perform_timeout(const char *arg)
  {
  int t = 0;
  while (*arg != '\0')
    {
    if (stricmp(cmd_switch, "/t") == 0) // just ignore
      {
      advance_cmd_arg();
      continue;
      }
    t = atoi(arg);
    advance_cmd_arg();
    }
  if (t)
    sleep(t);
  }

static void perform_type(const char *arg)
  {
  FILE *textfile;
  char filespec[MAXPATH] = "";
  int c;

  while (*arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      if (*filespec == '\0')
        {
        strncpy(filespec, arg, MAXPATH);
        filespec[MAXPATH-1] = '\0';
        conv_unix_path_to_ms_dos(filespec);
        }
      else
        {
        cprintf("Too many parameters - %s\r\n", cmd_args);
        reset_batfile_call_stack();
        return;
        }
      }
    else
      {
      }
    advance_cmd_arg();
    }
  /* HACK: open in text mode for dos, but then set binary mode for djgpp.
   * djgpp otherwise doesn't pass 0x1a to us (at least from device). */
  textfile = fopen(filespec,"rt");
  setbuf(textfile, NULL);
  __file_handle_set(fileno(textfile), O_BINARY);
  if (textfile != NULL)
    {
    while (1)
      {
      c = fgetc(textfile);
      if (c == EOF || c == 0x1a || c == 3 || c == 0)
        break;
      putchar(c);
      }
    fclose(textfile);
    }
  else
    {
    cprintf("Unable to open file - %s\r\n", filespec);
    error_level = 1;
    }
  }

static int is_blank(const char cc)
  {
  if (cc == 32 || cc == 9 || cc == 13 || cc == 10)
    {
    return 1;
    }
  else
    {
    return 0;
    }
  }

static void perform_ver(const char *arg)
  {
  int is_r = 0;
  while (*arg != '\0')
    {
    if (*cmd_switch == '\0') // if not a command switch ...
      {
      cprintf("Invalid parameter - %s\r\n", cmd_args);
      reset_batfile_call_stack();
      return;
      }
    else
      {
      if (stricmp(cmd_switch,"/r")==0)
        {
        is_r = 1;
        }
      else
        {
        cprintf("Invalid switch - %s\r\n", cmd_switch);
        reset_batfile_call_stack();
        return;
        }
      }
    advance_cmd_arg();
    }

  printf("comcom"
#ifdef DJ64
  "64"
#else
  "32"
#endif
  " v%s, %.16s\n", version, _stubinfo->magic);
#if 0
  if (strlen(revisionid))
    {
    printf(" Source Control Revision ID: %s\n", revisionid);
    }
#endif
  if (is_r)
    {
    const int buffersize = 256;
    int ver_major, ver_minor, true_ver_major, true_ver_minor, oem_id;
    unsigned ver_string, ii;
    char *pc, *buffer = malloc(buffersize);
    __dpmi_regs r = {};
    r.x.ax = 0x3000;
    __dpmi_int(0x21, &r);
    ver_major = r.h.al;
    ver_minor = r.h.ah;
    oem_id = r.h.bh;
    printf("\nReported DOS version (Int21.3000): %u.%02u OEM: %02Xh\n",
		ver_major, ver_minor, oem_id);
    r.x.ax = 0x3306;
    r.x.bx = 0;
    __dpmi_int(0x21, &r);
    if (! r.x.bx)
      {
      printf("Reported true DOS version (Int21.3306): (none)\n");
      }
    else
      {
      true_ver_major = r.h.bl;
      true_ver_minor = r.h.bh;
      printf("Reported true DOS version (Int21.3306): %u.%02u\n",
		true_ver_major, true_ver_minor);
      }
    r.x.ax = 0x33FF;
    r.x.dx = 0;
    __dpmi_int(0x21, &r);
    if (! r.x.dx)
      {
      printf("Version string (Int21.33FF): (none)\n");
      }
    else
      {
      if (! buffer)
        {
        printf("Version string (Int21.33FF): (buffer allocation failure)\n");
        }
      else
        {
        ver_string = (r.x.dx << 4) + r.x.ax;
        dosmemget(ver_string, buffersize - 1, buffer);
        buffer[buffersize - 1] = 0;
        pc = buffer;
        while (is_blank(*pc))
          {
          ++pc;
          }
        ii = strlen(pc);
        while (ii > 1 && is_blank(pc[ii - 1]))
          {
          --ii;
          }
        pc[ii] = 0;
        printf("Version string (Int21.33FF): %s\n", pc);
        }
      }
    free(buffer);
    }
  }

static void perform_cls(const char *arg)
  {
  clrscr();
  }

#if 0
static void perform_unimplemented_cmd(void)
  {
  cputs("Command not implemented\r\n");
  reset_batfile_call_stack();
  }
#endif

//////////////////////////////////////////////////////////////////////////////////

static void list_cmds(void)
  {
  int i, j;

  printf("\tAvailable commands:\n");
  for (i = 0; i < CMD_TABLE_COUNT; i++) {
    int num = printf("%s%s - %s", cmd_table[i].cmd_name, cmd_table[i].opts,
        cmd_table[i].help);
    if (!(i & 1))
      {
      for (j = num; j < 40; j++)
        printf(" ");
      }
      else
        printf("\n");
  }
  printf("\n");
  }

static bool is_valid_DOS_char(int c)
{
  unsigned char u=(unsigned char)c; /* convert to ascii */
  if (!u) return false;
  if (u >= 128 || isalnum(u)) return true;

  /* now we add some extra special chars  */
  if(strchr("_^$~!#%&-{}()@'`",c)!=0) return true; /* general for
                                                    any codepage */
  /* no match is found, then    */
  return false;
}

static void parse_cmd_line(void)
  {
  int c, cmd_len, *pipe_count_addr;
  char *extr, *dest, *saved_extr, *delim;
  char new_cmd_line[MAX_CMD_BUFLEN], *end;
  const char *v;

  // substitute in variable values before parsing
  extr = strchr(cmd_line, '%');
  if (extr != NULL)
    {
    dest = new_cmd_line;
    extr = cmd_line;
    v = NULL;
    while ((v != NULL || *extr != '\0') && dest < new_cmd_line+MAX_CMD_BUFLEN-1)
      {
      if (v == NULL)
        {
        if (*extr == '%')
          {
          extr++;
          if (*extr == '0')                   //  '%0'
            {
            extr++;
            v = bat_file_path[stack_level];
            continue;
            }
          if (*extr >= '1' && *extr <= '9')       //  '%1' to '%9'
            {
            v = bat_arg[stack_level][(*extr)-'1'];
            extr++;
            continue;
            }
          end = strchr(extr, '%');             // find ending '%'
          delim = strchr(extr, ' ');
          if (end == NULL || (delim && end > delim))  // if '%' found, but no ending '%' ...
            {
            if (*extr && *extr == for_var)
              {
              v = for_val;
              extr++;
              continue;
              }
            else
              {
              *dest = '%';
              dest++;
              continue;
              }
            }
          else                              // else ending '%' is found too
            {
            if (extr == end)                   //   if "%%", replace with single '%'
              {
              extr++;
              *dest = '%';
              dest++;
              continue;
              }
            else
              {
              *end = '\0';
              strupr(extr);
              v = getenv(extr);
              extr = end + 1;
              }
            }
          }
        else
          {
          *dest = *extr;
          dest++;
          extr++;
          }
        }
      else
        {
        if (*v != '\0')
          {
          *dest = *v;
          dest++;
          v++;
          }
        if (*v == '\0')
          v = NULL;
        }
      }
    *dest = '\0';
    strcpy(cmd_line, new_cmd_line);
    }

  // extract pipe specs....
  pipe_file[STDIN_INDEX][0] = '\0';   //  <
  pipe_file_redir_count[STDIN_INDEX] = 0;   // count of '<' characters

  pipe_file[STDOUT_INDEX][0] = '\0';  //  > or >>
  pipe_file_redir_count[STDOUT_INDEX] = 0;   // count of '>' characters

  pipe_to_cmd[0] = '\0';      // |
  pipe_to_cmd_redir_count = 0; // count of '|' characters

  extr = cmd_line;
  while (*extr != '\0')
    {
    c = *extr;
    switch (*extr)
      {
      case '<':
        dest =  pipe_file[STDIN_INDEX];
        pipe_count_addr = &(pipe_file_redir_count[STDIN_INDEX]);
        break;
      case '>':
        dest = pipe_file[STDOUT_INDEX];
        pipe_count_addr = &(pipe_file_redir_count[STDOUT_INDEX]);
        break;
      case '|':
        dest = pipe_to_cmd;
        pipe_count_addr = &pipe_to_cmd_redir_count;
        break;
      default:
        c = 0;
        break;
      }

    if (c == 0)
      extr++;
    else
      {
      // count redirection characters
      saved_extr = extr;
      while (*extr == c)
        {
        (*pipe_count_addr)++;
        extr++;
        }

      // skip over spaces
      while (*extr == ' ' || *extr == '\t')
        extr++;

      // extract pipe destinations
      if (c == '|')     // "pipe to" command
        {
        while (*extr != '\0')
          {
          *dest = *extr;
          dest++;
          extr++;
          }
        }
      else             // pipe in or out file
        {
        while (*extr != ' ' && *extr != '\t' && *extr != '\0')
          {
          *dest = *extr;
          dest++;
          extr++;
          }
        }
      *dest = '\0';

      // snip out pipe spec from the cmd_line[] string
      memmove(saved_extr, extr, strlen(extr)+1);
      extr = saved_extr;
      }
    }
  conv_unix_path_to_ms_dos(pipe_file[STDIN_INDEX]);
  conv_unix_path_to_ms_dos(pipe_file[STDOUT_INDEX]);

  // done with variables and pipes -- now, skip leading spaces
  extr = cmd_line;
  while (*extr == ' ' || *extr == '\t')
    extr++;
  if (*extr == '\0')
    {
    cmd[0] = '\0';
    cmd_arg[0] = '\0';
    cmd_switch[0] = '\0';
    cmd_args[0] = '\0';
    return;
    }

  // extract built-in command if command line contains one
  for (c = 0; c < CMD_TABLE_COUNT; c++)
    {
    cmd_len = strlen(cmd_table[c].cmd_name);
    if (strnicmp(extr, cmd_table[c].cmd_name, cmd_len) == 0)
      {
      delim = extr+cmd_len;
      if (!is_valid_DOS_char(delim[0]) || !is_valid_DOS_char(delim[-1]))
        {
        // ok, we have a built-in command; extract it
        strcpy(cmd, cmd_table[c].cmd_name);
        extr = delim;
        break;
        }
      }
    }

  // if not built-in command, extract as an external command
  if (c >= CMD_TABLE_COUNT)
    {
    dest = cmd;
    while (*extr != ' ' && *extr != '\t' && *extr != '/' && *extr != '\0')
      {
      *dest = *extr;
      dest++;
      extr++;
      }
    *dest = '\0';
    }

  // extract the rest as arguments
  cmd_args[0] = '\0';
  extract_args(extr);
  return;
  }

static void exec_cmd(int call)
  {
  int c;
  int pipe_index, pipe_fno[2], old_std_fno[2], redir_result[2];

  for (pipe_index = 0; pipe_index < 2; pipe_index++)
    {
    pipe_fno[pipe_index] = -1;
    old_std_fno[pipe_index] = -1;
    redir_result[pipe_index] = -1;
    }

  if (pipe_to_cmd_redir_count > 0)
    {
      pipe(pipe_fno);
    /* cputs("Piping between 2 commands is not supported\r\n");
       reset_batfile_call_stack();
       goto Exit; */
    }
  else // open the pipe file
    {
    if (pipe_file_redir_count[STDIN_INDEX] > 0)
      pipe_fno[STDIN_INDEX] = open(pipe_file[STDIN_INDEX], O_TEXT|O_RDONLY, S_IRUSR);

    if (pipe_file_redir_count[STDOUT_INDEX] > 1)
      pipe_fno[STDOUT_INDEX] = open(pipe_file[STDOUT_INDEX], O_BINARY|O_WRONLY|O_APPEND|O_CREAT, S_IWUSR); // open for append
    else if (pipe_file_redir_count[STDOUT_INDEX] == 1)
      pipe_fno[STDOUT_INDEX] = open(pipe_file[STDOUT_INDEX], O_BINARY|O_WRONLY|O_TRUNC|O_CREAT, S_IWUSR);  // open as new file

      /* check for error
      if (pipe_fno[pipe_index] < 0 ||
          old_std_fno[pipe_index] == -1 ||
          redir_result[pipe_index] == -1)
        {
        if (pipe_index == pipe_index)
          cprintf("Unable to pipe standard input from file - %s\r\n", pipe_file[pipe_index]);
        else
          cprintf("Unable to pipe standard output to file - %s\r\n", pipe_file[pipe_index]);
        reset_batfile_call_stack();
        goto Exit;
        } */
    }

  for (pipe_index = 0; pipe_index < 2; pipe_index++)
    {
      // save a reference to the old standard in/out file handle
      if (pipe_fno[pipe_index] >= 0)
        old_std_fno[pipe_index] = dup(pipe_index);

      // redirect pipe file to standard in/out
      if (pipe_fno[pipe_index] >= 0 && old_std_fno[pipe_index] != -1)
        redir_result[pipe_index] = dup2(pipe_fno[pipe_index], pipe_index);

      // close pipe file handle
      if (pipe_fno[pipe_index] >= 0)
        {
        close(pipe_fno[pipe_index]);
        pipe_fno[pipe_index] = -1;
        }
    }
  if (old_std_fno[STDIN_INDEX] >= 0)
    bkp_stdin = fdopen(old_std_fno[STDIN_INDEX], "r");

  while (cmd[0] != '\0')
    {
    if (stricmp(cmd, "if") == 0)
      {
      perform_if();
      continue;
      }
    else if (strnicmp(cmd, "rem", 3) == 0)     // rem statement
      perform_null_cmd();
    else if (cmd[0] == ':')                   // goto label
      perform_null_cmd();
    else if (is_drive_spec(cmd) ||
             is_drive_spec_with_slash(cmd))  // drive letter
      perform_change_drive();
    else
      {
      if (!call && installable_command_check(cmd, cmd_args) == 0)
        {
        cmd[0] = '\0';
        break;
        }
      for (c = 0; c < CMD_TABLE_COUNT; c++)
        {
        if (stricmp(cmd, cmd_table[c].cmd_name) == 0)
          {
          cmd_table[c].cmd_fn(cmd_arg);
          break;
          }
        }
      if (c >= CMD_TABLE_COUNT)
        {
          need_to_crlf_at_next_prompt = true;
          perform_external_cmd(call, false, cmd);
        }
      }
    cmd[0] = '\0';
    }

  /* Recover the stdout stream */
  if (redir_result[STDOUT_INDEX] != -1) {
    dup2(old_std_fno[STDOUT_INDEX], STDOUT_INDEX);
    close(old_std_fno[STDOUT_INDEX]);
    clearerr(stdout);
  }

  if (pipe_to_cmd_redir_count > 0)
    {
    strcpy(cmd_line, pipe_to_cmd);
    parse_cmd_line();
    exec_cmd(true);
    }

/* Exit: */
  cmd_line[0] = '\0';
  if (redir_result[STDIN_INDEX] != -1) {
    dup2(old_std_fno[STDIN_INDEX], STDIN_INDEX);
    fclose(bkp_stdin);  // closes also fd
    clearerr(stdin);
    }
  }

int do_int23(void)
{
  return break_enabled;
}

static void set_break(int on)
{
  __dpmi_regs r = {};

  r.x.ax = 0x3301;          // set break handling
  r.x.dx = on;              // to "on"
  __dpmi_int(0x21, &r);

  break_enabled = on;
}

static void setup_break_handling(void)
{
  __dpmi_paddr pa;

  __djgpp_set_ctrl_c(0);    // disable SIGINT on ^C
  _go32_want_ctrl_break(1); // disable SIGINT on ^Break

  set_break(0);

  pa.selector = _my_cs();
  pa.offset32 = (uintptr_t)my_int23_handler;
  __dpmi_set_protected_mode_interrupt_vector(0x23, &pa);

  break_on = true;
}

static int break_pressed(void)
{
  int ret = _farpeekb(_dos_ds, 0x471);
  _farpokeb(_dos_ds, 0x471, ret & ~0x80);
  return (ret & 0x80);
}

static void setup_int0_handling(void)
{
  __dpmi_paddr pa;

  __dpmi_get_real_mode_interrupt_vector(0, &int0_vec);
  __dpmi_get_extended_exception_handler_vector_rm(0, &pa);
  _prev0_eip = pa.offset32;
  _prev0_cs = pa.selector;

  pa.selector = _my_cs();
  pa.offset32 = (uintptr_t)my_int0_handler;
  __dpmi_set_extended_exception_handler_vector_rm(0, &pa);
}

void do_int0(void)
{
  if (int0_wa)
    __dpmi_set_real_mode_interrupt_vector(0, &int0_vec);
}

int main(int argc, const char *argv[], const char *envp[])
  {
  int a;
  char *cmd_path, *v;
  int disable_autoexec = 0;
  int inited = 0;
  // initialize the cmd data ...

  // reset fpu
  _clear87();
  _fpreset();
  _ds = _my_ds();
  loadhigh_init();	// save initial umb link and strat
  unlink_umb();		// in case we loaded with shellhigh or lh
  set_env_size();
//  reset_text_attrs();

#ifdef __spawn_leak_workaround
  __spawn_flags &= ~__spawn_leak_workaround;
#endif
  if (_osmajor < 7)
    _osmajor = 7;  // fake _osmajor to enable extended functionality
  setup_break_handling();
  setup_int0_handling();

  // unbuffer stdin and stdout
  setbuf(stdin, NULL);
  setbuf(stdout, NULL);

  // init bat file stack
  reset_batfile_call_stack();

  cmd_path = strdup(argv[0]);
  strupr(cmd_path);
  conv_unix_path_to_ms_dos(cmd_path);
  setenv("COMSPEC", cmd_path, 1);
  free(cmd_path);
  setenv("COMCOM_VER", version, 1);
  setenv("ERRORLEVEL", "0", 1);

  // process arguments
  for (a = 1; a < argc; a++)
    {
    // check for permanent shell
    if (stricmp(argv[a], "/P") == 0)
      {
      shell_permanent = 1;
      }

    if (strnicmp(argv[a], "/E:", 3) == 0)
      {
      unsigned new_size, old_size = get_env_size();

      if (argv[a][3] == '+')
        new_size = old_size + atoi(argv[a] + 4);
      else
        new_size = atoi(argv[a] + 3);
      new_size &= ~0xf;
      if (new_size > old_size)
        {
        int err = realloc_env(new_size);
        if (err)
          return EXIT_FAILURE;
        }
      }

    if (stricmp(argv[a], "/D") == 0)
      {
      disable_autoexec = 1;
      }

    if (stricmp(argv[a], "/Y") == 0)
      {
      stepping = 1;
      }

    if (strnicmp(argv[a], "/M", 2) == 0)
      {
      int opt = 1;
      char copt[2];
      if (isdigit(argv[a][2]))
        opt = argv[a][2] - '0';
      copt[0] = opt + '0';
      copt[1] = '\0';
      unsetenv("COMCOM_MOUSE");
      switch (opt)
        {
        case 0:
          break;
        default:
          mouse_en = mouse_init();
          if (mouse_en)
            {
            mouseopt_enabled = 1;
            mouseopt_extctl = (opt == 2);
            setenv("COMCOM_MOUSE", copt, 1);
            }
        break;
        }
      }

    // check for command in arguments
    if (stricmp(argv[a], "/K") == 0)
      {
      shell_mode = SHELL_STARTUP_WITH_CMD;
      a++;
      strncat(cmd_line, argv[a], MAX_CMD_BUFLEN-1);
      parse_cmd_line();
      }

    if (stricmp(argv[a], "/C") == 0)
      {
      int cmd_buf_remaining;

      shell_mode = SHELL_SINGLE_CMD;

      // build command from rest of the arguments
      a++;
      cmd_buf_remaining = MAX_CMD_BUFLEN-1;
      while (a < argc)
        {
        strncat(cmd_line, argv[a], cmd_buf_remaining);
        cmd_buf_remaining -= strlen(argv[a]);
        if (cmd_buf_remaining < 0)
          cmd_buf_remaining = 0;
        strncat(cmd_line, " ", cmd_buf_remaining);
        cmd_buf_remaining--;
        if (cmd_buf_remaining < 0)
          cmd_buf_remaining = 0;
        a++;
        }
      parse_cmd_line();
      }
    }

  if (shell_permanent) {
    set_psp_parent();
#if !SYNC_ENV
    /* some progs (Word Perfect) look for COMSPEC in parent env, rather
     * than their own. We need to sync it down to DOS. */
    sync_env();
#endif
  }

  if (!mouse_en && (v = getenv("COMCOM_MOUSE")) && v[0] >= '1')
    {
    mouse_en = mouse_init();
    if (mouse_en)
      {
      mouseopt_enabled = 1;
      if (v[0] == '2')
        mouseopt_extctl = 1;
      }
    }

  if (shell_permanent && !disable_autoexec)
    {
    unsigned int drive;
    strcpy(bat_file_path[0], "X:\\AUTOEXEC.BAT");  // trigger execution of autoexec.bat
    getdrive(&drive);
    drive += ('A' - 1);
    bat_file_path[0][0] = drive;
    // no arguments for batch file
    }

  // Main command parsing/interpretation/execution loop
  while (!exiting)
    {
    if (cmd_line[0] == '\0')
      {
      if (bat_file_path[stack_level][0] == '\0')
        {
        if (shell_mode == SHELL_SINGLE_CMD)
          {
          perform_exit(NULL);
          continue;
          }
        if (!inited)
          {
          inited++;
          cmdbuf_init();
          }
        set_env_seg();
        prompt_for_and_get_cmd();
        set_env_sel();
        }
      else
        {
        if (break_on && break_pressed())
          reset_batfile_call_stack();
        else
          get_cmd_from_bat_file();
        }
      }
    exec_cmd(false);
    }

  loadhigh_done();
  if (mouse_en)
    mouse_done();
  if (shell_permanent)
    restore_psp_parent();
  return error_level;
  }
