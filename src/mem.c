/*
 *  comcom64 - 64bit command.com
 *  mem.c: FreeDOS-like MEM command implementation
 *  Copyright (C) 2026  @stsp/JulesAI, @stuaxo/Claude
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
#include <ctype.h>
#include <stdint.h>
#include <conio.h>
#include <dpmi.h>
#include <sys/farptr.h>
#include <go32.h>
#include <libc/dosio.h>

#include "asm.h"
#include "command.h"
#include "mem.h"

/*
 * Everything here is accounted for, and displayed, in KB - the same unit
 * real FreeDOS MEM reports. Each source of numbers is converted to KB
 * exactly once, right where it is read: INT 12h and XMS/INT 15h already
 * speak KB, EMS counts 16KB pages, and the MCB chain counts 16-byte
 * paragraphs. Nothing is converted from KB to bytes and back again on
 * its way to the screen.
 */
#define PARAS_PER_KB 64
#define EMS_PAGE_KB 16

/*
 * Conventional memory ends at the 640K line - segment 0xA000 - and
 * everything at or above it is upper memory. Nothing we report as
 * conventional may exceed that, whatever the BIOS or the MCB chain
 * says.
 */
#define UMB_START_SEG 0xA000
#define CONV_MAX_KB (UMB_START_SEG / PARAS_PER_KB)

/*
 * Above conventional memory, FreeDOS MEM splits the first megabyte the
 * way MS-DOS MEM did: 0xA000-0xDFFF is "Upper", and 0xE000-0xFFFF is
 * "Reserved" - the BIOS and option ROM area, reported as entirely in
 * use. Both are fixed-size regions, not sums of the blocks found in
 * them, and free UMBs count towards upper memory wherever they sit.
 * Report them the same way, so our numbers can be compared with
 * MEM.EXE's directly.
 */
#define RESERVED_START_SEG 0xE000
#define UMB_TOTAL_KB ((RESERVED_START_SEG - UMB_START_SEG) / PARAS_PER_KB)
#define RESERVED_TOTAL_KB ((0x10000 - RESERVED_START_SEG) / PARAS_PER_KB)

/* Rounds to nearest, as FreeDOS MEM does. */
static uint32_t paras_to_kb(uint32_t paras)
{
  return (paras + PARAS_PER_KB / 2) / PARAS_PER_KB;
}

struct MCB {
  char id;                  /* 'M' (0x4D) or 'Z' (0x5A) */
  uint16_t owner_psp;       /* 0 = free, 8 = system, else PSP segment */
  uint16_t size;            /* Size in 16-byte paragraphs */
  char reserved[3];
  char name[8];             /* Owner/program name in DOS 4+ */
} __attribute__((packed));

struct module_entry {
  uint16_t owner_psp;
  char name[16]; /* matches get_mcb_name()'s name_out buffer size */
  uint32_t conv_paras;
  uint32_t umb_paras;
};

static int opt_classify = 0;
static int opt_free = 0;
static int opt_debug = 0;
static int opt_page = 0;
static int page_bottom = 0;
static int page_stop = 0;

static void reset_page(void)
{
  struct text_info txinfo;

  page_stop = 0;
  gettextinfo(&txinfo);
  page_bottom = txinfo.winbottom;
  /* start the report on a screen of its own, as "dir /p" does, so the
   * first page is a full one wherever the command line left the cursor */
  if (opt_page)
    clrscr();
}

/*
 * Pause on reaching the last line of the window, rather than counting
 * the lines printed so far: our output starts wherever the command line
 * left the cursor and long lines wrap, so a counter that starts at zero
 * pauses in the wrong place - or, for the short default report, never
 * pauses at all while the earlier lines scroll away. This is what
 * "dir /p" does, so /PAGE now behaves the same in both commands.
 */
static void print_line(const char *str)
{
  int c;

  if (page_stop)
    return;
  printf("%s\n", str);
  if (!opt_page)
    return;

  /* the cursor only moves once stdout reaches the screen */
  fflush(stdout);
  if (wherey() < page_bottom)
    return;

  printf("Press any key to continue, or q to stop...");
  fflush(stdout);
  c = getch();
  if (c == 3 || c == 27 || toupper(c) == 'Q')
  {
    printf("\n");
    page_stop = 1;
    return;
  }
  clrscr();
}

static void format_num(uint32_t val, char *buf, size_t buf_size)
{
  char tmp[32];
  snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)val);
  int len = strlen(tmp);
  int dest_idx = 0;
  int i;

  for (i = 0; i < len; i++)
  {
    if (i > 0 && (len - i) % 3 == 0)
    {
      if (dest_idx < (int)buf_size - 1)
        buf[dest_idx++] = ',';
    }
    if (dest_idx < (int)buf_size - 1)
      buf[dest_idx++] = tmp[i];
  }
  buf[dest_idx] = '\0';
}

static void format_kb(uint32_t kb, char *buf, size_t buf_size)
{
  size_t len;

  format_num(kb, buf, buf_size);
  len = strlen(buf);
  if (len < buf_size - 1)
  {
    buf[len] = 'K';
    buf[len + 1] = '\0';
  }
}

/*
 * MEM.EXE prints the largest blocks and the EMS totals in KB with the
 * byte count next to it, and the dosemu2 test suite parses that byte
 * count, so print both. This is the only place a KB figure is turned
 * back into bytes, and only for display.
 */
static void print_kb_and_bytes(const char *label, uint32_t kb, uint32_t bytes)
{
  char kb_str[24], b_str[24], buf[160];

  format_kb(kb, kb_str, sizeof(kb_str));
  format_num(bytes, b_str, sizeof(b_str));
  snprintf(buf, sizeof(buf), "%-36s%8s (%s bytes)", label, kb_str, b_str);
  print_line(buf);
}

/* DPMI fn 0500h fills fields the host can't tell with -1 */
#define DPMI_UNKNOWN 0xFFFFFFFFu
#define DPMI_PAGE_KB 4

static void print_dpmi_pages(const char *label, uint32_t pages)
{
  char kb_str[24], buf[160];

  /* past 4GB the byte count no longer fits, so give KB alone */
  if (pages < 0x100000)
  {
    print_kb_and_bytes(label, pages * DPMI_PAGE_KB, pages * 4096);
    return;
  }
  format_kb(pages * DPMI_PAGE_KB, kb_str, sizeof(kb_str));
  snprintf(buf, sizeof(buf), "%-36s%8s", label, kb_str);
  print_line(buf);
}

struct psp_name {
  uint16_t psp;
  char name[9];
};

#define MAX_PSP_NAMES 64

/* Copies the MCB's owner name out, if DOS put a usable one there. */
static int copy_mcb_name(const struct MCB *mcb, char *out, size_t out_size)
{
  int i;

  for (i = 0; i < 8; i++)
  {
    if (mcb->name[i] == 0 || mcb->name[i] == ' ')
      break;
    if (!isprint((unsigned char)mcb->name[i]) || (size_t)i >= out_size - 1)
      return 0;
    out[i] = mcb->name[i];
  }
  out[i] = '\0';
  return i > 0;
}

/*
 * DOS 4+ stores the owner's name in the MCB of the block holding the
 * program itself - the one whose PSP is its first paragraph. A
 * program's other blocks (its environment, anything it allocates later)
 * carry no usable name, so collect the names up front and then look
 * them up by owner. Naming each block on its own instead is what made
 * MEM /CLASSIFY list one module twice, once as COMMAND and once as
 * PSP-xxxx.
 */
static int collect_psp_names(uint16_t first_mcb_seg, struct psp_name *names,
                             int max_names)
{
  uint16_t seg = first_mcb_seg;
  int num = 0;

  while (seg != 0 && num < max_names)
  {
    struct MCB mcb;

    dosmemget(seg << 4, sizeof(mcb), &mcb);
    if (mcb.id != 'M' && mcb.id != 'Z')
      break;

    if (mcb.owner_psp == seg + 1 &&
        copy_mcb_name(&mcb, names[num].name, sizeof(names[num].name)))
      names[num++].psp = mcb.owner_psp;

    if (mcb.id == 'Z')
      break;
    if (seg + 1 + mcb.size > 0xffff)
      break;
    seg += 1 + mcb.size;
  }
  return num;
}

/*
 * DOS keeps the upper memory chain linked across the areas that are not
 * RAM at all - the video memory and the option ROMs - with placeholder
 * blocks owned by the kernel and named "SC". They are holes in the
 * address space rather than memory in use, and neither MEM.EXE nor
 * dosemu2's own debugger counts them, so skip them too.
 */
static int mcb_is_link(const struct MCB *mcb)
{
  char name[9];

  return mcb->owner_psp == 8 && copy_mcb_name(mcb, name, sizeof(name)) &&
    strcmp(name, "SC") == 0;
}

static void get_mcb_name(const struct MCB *mcb, const struct psp_name *names,
                         int num_names, char *name_out, size_t name_out_size)
{
  int i;

  memset(name_out, 0, name_out_size);

  if (mcb->owner_psp == 0)
  {
    strncpy(name_out, "Free", name_out_size - 1);
    return;
  }

  /*
   * Blocks owned by DOS carry the name of the driver loaded into them,
   * where there is one - that is how MEM.EXE tells a driver apart from
   * the kernel's own memory. "SC" and "SD" are not drivers though, they
   * are the kernel's own markers for its link blocks and its data.
   */
  if (mcb->owner_psp == 8)
  {
    if (mcb_is_link(mcb))
      strncpy(name_out, "LINK", name_out_size - 1);
    else if (!copy_mcb_name(mcb, name_out, name_out_size) ||
             strcmp(name_out, "SD") == 0)
      strncpy(name_out, "SYSTEM", name_out_size - 1);
    return;
  }

  for (i = 0; i < num_names; i++)
  {
    if (names[i].psp == mcb->owner_psp)
    {
      strncpy(name_out, names[i].name, name_out_size - 1);
      return;
    }
  }

  if (copy_mcb_name(mcb, name_out, name_out_size))
    return;

  /* Try extracting program name from owner PSP environment block */
  uint16_t env_seg = _farpeekw(_dos_ds, (mcb->owner_psp << 4) + 0x2c);
  if (env_seg != 0)
  {
    uint32_t env_addr = env_seg << 4;
    uint32_t offset = 0;
    uint8_t b1 = _farpeekb(_dos_ds, env_addr + offset);
    uint8_t b2 = _farpeekb(_dos_ds, env_addr + offset + 1);

    while (offset < 0x7fff)
    {
      if (b1 == 0 && b2 == 0)
      {
        offset += 2;
        uint16_t count = _farpeekw(_dos_ds, env_addr + offset);
        if (count == 1)
        {
          offset += 2;
          char path[128];
          int p_idx = 0;
          while (p_idx < sizeof(path) - 1)
          {
            char ch = _farpeekb(_dos_ds, env_addr + offset + p_idx);
            if (ch == 0)
              break;
            path[p_idx++] = ch;
          }
          path[p_idx] = '\0';

          char *fname = strrchr(path, '\\');
          if (!fname)
            fname = strrchr(path, '/');
          if (!fname)
            fname = path;
          else
            fname++;

          char *dot = strchr(fname, '.');
          if (dot)
            *dot = '\0';

          if (strlen(fname) > 0)
          {
            strncpy(name_out, fname, name_out_size - 1);
            return;
          }
        }
        break;
      }
      offset++;
      b1 = b2;
      b2 = _farpeekb(_dos_ds, env_addr + offset + 1);
    }
  }

  snprintf(name_out, name_out_size, "PSP-%04X", mcb->owner_psp);
}

struct e820_entry {
  uint64_t base;
  uint64_t length;
  uint32_t type;
  uint32_t ext_attr;
} __attribute__((packed));

#define E820_TYPE_USABLE 1
#define E820_MAX_ENTRIES 64

/*
 * XMS only ever reports free extended memory (function 08h gives the
 * largest free block and total free, nothing else) - there is no XMS
 * call for total installed extended memory, so total has to come from
 * the BIOS. Prefer the INT 15h/E820h system memory map, same as real
 * FreeDOS's own MEM: under dosemu2 the older E801h/88h calls can report
 * a much smaller "compatibility" figure than the extended memory pool
 * that's actually configured, while E820h reflects it accurately. Fall
 * back to E801h (covers configurations above 64MB) and then the even
 * older AH=88h (limited to 64MB) only if E820h isn't supported.
 */
static uint32_t query_ext_mem_total_kb(void)
{
  uint32_t total_kb = 0;
  uint32_t continuation = 0;
  int i;

  for (i = 0; i < E820_MAX_ENTRIES; i++)
  {
    struct e820_entry entry = {};
    __dpmi_regs r = {};

    r.d.eax = 0xe820;
    r.d.edx = 0x534d4150; /* 'SMAP' */
    r.d.ecx = sizeof(entry);
    r.d.ebx = continuation;
    r.x.es = __tb_segment;
    r.d.edi = __tb_offset;
    __dpmi_int(0x15, &r);

    if ((r.x.flags & 1) || r.d.eax != 0x534d4150 || r.d.ecx == 0)
      break;

    dosmemget(__tb, r.d.ecx < sizeof(entry) ? r.d.ecx : sizeof(entry), &entry);

    if (entry.type == E820_TYPE_USABLE && entry.base >= 0x100000)
      total_kb += (uint32_t)(entry.length / 1024);

    continuation = r.d.ebx;
    if (continuation == 0)
      break;
  }

  if (total_kb > 0)
    return total_kb;

  __dpmi_regs r = {};
  r.x.ax = 0xe801;
  __dpmi_int(0x15, &r);
  if (!(r.x.flags & 1))
  {
    uint32_t below_16m = r.x.ax ? r.x.ax : r.x.cx;
    uint32_t above_16m_64k_blocks = r.x.ax ? r.x.bx : r.x.dx;
    uint32_t total = below_16m + above_16m_64k_blocks * 64;
    if (total > 0)
      return total;
  }

  __dpmi_regs r2 = {};
  r2.h.ah = 0x88;
  __dpmi_int(0x15, &r2);
  if (!(r2.x.flags & 1) && r2.x.ax > 0)
    return r2.x.ax;

  return 0;
}

static void show_help(void)
{
  reset_page();
  print_line("Displays the amount of used and free memory in your system.");
  print_line("");
  print_line("MEM [/CLASSIFY | /FREE | /DEBUG] [/PAGE]");
  print_line("");
  print_line("  /CLASSIFY or /C  Classifies programs by memory usage.");
  print_line("  /FREE     or /F  Lists free memory blocks.");
  print_line("  /DEBUG    or /D  Displays detailed memory listing.");
  print_line("  /PAGE     or /P  Pauses after each screen of information.");
}

void perform_mem(const char *arg)
{
  /*
   * Built-in commands are handed only the first argument, while MEM
   * takes a combination of switches - "mem /d /p" used to arrive here
   * as just "/d", silently dropping /PAGE - so parse the full argument
   * string instead.
   */
  const char *p = get_cmd_args();

  opt_classify = 0;
  opt_free = 0;
  opt_debug = 0;
  opt_page = 0;

  while (*p)
  {
    while (isspace((unsigned char)*p))
      p++;
    if (!*p)
      break;
    if (*p == '/' || *p == '-')
    {
      p++;
      if (*p == '?')
      {
        show_help();
        return;
      }

      const char *tok = p;
      while (isalnum((unsigned char)*p))
        p++;
      size_t tok_len = p - tok;

      if ((tok_len == 1 && strnicmp(tok, "C", 1) == 0) ||
          (tok_len == 8 && strnicmp(tok, "CLASSIFY", 8) == 0))
        opt_classify = 1;
      else if ((tok_len == 1 && strnicmp(tok, "F", 1) == 0) ||
               (tok_len == 4 && strnicmp(tok, "FREE", 4) == 0))
        opt_free = 1;
      else if ((tok_len == 1 && strnicmp(tok, "D", 1) == 0) ||
               (tok_len == 5 && strnicmp(tok, "DEBUG", 5) == 0))
        opt_debug = 1;
      else if ((tok_len == 1 && strnicmp(tok, "P", 1) == 0) ||
               (tok_len == 4 && strnicmp(tok, "PAGE", 4) == 0))
        opt_page = 1;
      else if (tok_len == 4 && strnicmp(tok, "HELP", 4) == 0)
      {
        show_help();
        return;
      }
      else
      {
        cprintf("Invalid switch - %s\r\n", tok - 1);
        reset_batfile_call_stack();
        return;
      }
    }
    else
    {
      /* not a switch - skip the whole word, not just one character */
      while (*p && !isspace((unsigned char)*p))
        p++;
    }
  }

  reset_page();

  /*
   * Save UMB link state and link UMBs for the memory scan. This talks
   * to INT 21h/58h(03h) (Set UMB Link State) directly instead of going
   * through link_umb()/unlink_umb() in umb.c: those also poke the
   * allocation strategy via 58h(01h), and unlink_umb() hardcodes it
   * back to 0 rather than whatever it was before MEM ran. Doing the
   * link/unlink here means MEM never touches allocation strategy at
   * all, so there's nothing to restore.
   */
  __dpmi_regs r = {};
  r.x.ax = 0x5802;
  __dpmi_int(0x21, &r);
  uint8_t orig_umblink = r.h.al;

  r.x.ax = 0x5803;
  r.x.bx = 1;
  __dpmi_int(0x21, &r);

  /* Get List of Lists -> First MCB */
  r.h.ah = 0x52;
  __dpmi_int(0x21, &r);
  uint32_t sysvars_addr = (r.x.es << 4) + r.x.bx;
  uint16_t first_mcb_seg = _farpeekw(_dos_ds, sysvars_addr - 2);

  uint32_t conv_total_kb = 0;
  uint32_t conv_total_paras = 0;
  uint32_t conv_free_paras = 0;
  uint32_t conv_largest_paras = 0;

  uint32_t umb_free_paras = 0;
  uint32_t umb_largest_paras = 0;

  /* runs of adjacent free blocks, see below */
  uint32_t conv_run_paras = 0;
  uint32_t umb_run_paras = 0;

  struct module_entry modules[64];
  int num_modules = 0;
  struct psp_name psp_names[MAX_PSP_NAMES];
  int num_psp_names;

  memset(modules, 0, sizeof(modules));
  memset(psp_names, 0, sizeof(psp_names));
  num_psp_names = collect_psp_names(first_mcb_seg, psp_names, MAX_PSP_NAMES);

  /*
   * Memory below the first MCB - the interrupt vector table, the BIOS
   * data area and the resident kernel - belongs to no block, but it is
   * in use, and MEM.EXE counts it towards SYSTEM. Start the module list
   * with it, so the listing adds up to the conventional total and opens
   * with SYSTEM, as MEM.EXE's does.
   */
  if (opt_classify)
  {
    modules[0].owner_psp = 8;
    strcpy(modules[0].name, "SYSTEM");
    modules[0].conv_paras = first_mcb_seg;
    num_modules = 1;
  }

  char line_buf[160];

  if (opt_debug)
  {
    print_line("Memory Detail:");
    print_line("  Segment    Owner      Size   Type/Name");
    print_line("  -------  -------  --------   -----------");
  }
  else if (opt_free)
  {
    print_line("Free Memory Blocks:");
    print_line("  Segment      Size   Memory Type");
    print_line("  -------  --------   -----------");
  }

  uint16_t curr_seg = first_mcb_seg;
  while (curr_seg != 0)
  {
    struct MCB mcb;
    dosmemget(curr_seg << 4, sizeof(mcb), &mcb);

    if (mcb.id != 'M' && mcb.id != 'Z')
      break;

    uint32_t block_paras = mcb.size;
    int is_umb = (curr_seg >= UMB_START_SEG);
    int is_link = mcb_is_link(&mcb);

    /*
     * A block can cross the 640K line: with UMBs linked in, DOS merges
     * the top of conventional memory with an adjacent upper memory
     * block into a single MCB. Count each half against the region it
     * lies in, or conventional memory ends up bigger than 640K.
     */
    uint32_t conv_paras = 0;
    uint32_t umb_paras = 0;

    if (is_umb)
      umb_paras = block_paras;
    else if (curr_seg + block_paras <= UMB_START_SEG)
      conv_paras = block_paras;
    else
    {
      conv_paras = UMB_START_SEG - curr_seg;
      umb_paras = block_paras - conv_paras;
    }

    if (is_link)
    {
      /* a hole in the address space: listed, but not counted */
      conv_paras = 0;
      umb_paras = 0;
    }

    char name[16];
    get_mcb_name(&mcb, psp_names, num_psp_names, name, sizeof(name));

    /* the MCB header itself belongs to the region it sits in */
    if (!is_umb)
      conv_total_paras += conv_paras + 1;

    if (mcb.owner_psp == 0)
    {
      conv_free_paras += conv_paras;
      umb_free_paras += umb_paras;

      /*
       * DOS merges adjacent free blocks when it allocates, so a run of
       * them is a single allocatable block - and the MCB headers
       * between them become usable too. Tracking runs rather than
       * single blocks is what makes the largest sizes agree with what
       * MEM.EXE reports: right after a program exits, the memory it
       * used is several adjacent free blocks that DOS has not merged
       * yet.
       */
      if (conv_paras)
      {
        conv_run_paras = conv_run_paras ? conv_run_paras + 1 + conv_paras :
          conv_paras;
        if (conv_run_paras > conv_largest_paras)
          conv_largest_paras = conv_run_paras;
      }
      else
        conv_run_paras = 0;

      if (umb_paras)
      {
        umb_run_paras = umb_run_paras ? umb_run_paras + 1 + umb_paras :
          umb_paras;
        if (umb_run_paras > umb_largest_paras)
          umb_largest_paras = umb_run_paras;
      }
      else
        umb_run_paras = 0;
    }
    else
    {
      conv_run_paras = 0;
      umb_run_paras = 0;
    }

    /* Collect module info for /CLASSIFY */
    if (opt_classify && !is_link)
    {
      int found = -1;
      int i;
      for (i = 0; i < num_modules; i++)
      {
        if (modules[i].owner_psp == mcb.owner_psp && strcmp(modules[i].name, name) == 0)
        {
          found = i;
          break;
        }
      }
      if (found == -1 && num_modules < 64)
      {
        found = num_modules++;
        modules[found].owner_psp = mcb.owner_psp;
        strncpy(modules[found].name, name, sizeof(modules[found].name) - 1);
      }
      if (found >= 0)
      {
        if (is_umb)
          modules[found].umb_paras += umb_paras + 1;
        else
        {
          modules[found].conv_paras += conv_paras + 1;
          modules[found].umb_paras += umb_paras;
        }
      }
    }

    if (opt_debug)
    {
      char sz_str[24];
      format_kb(paras_to_kb(block_paras), sz_str, sizeof(sz_str));
      snprintf(line_buf, sizeof(line_buf), "    %04Xh    %04Xh  %8s   %s",
               curr_seg, mcb.owner_psp, sz_str, name);
      print_line(line_buf);
    }
    else if (opt_free && mcb.owner_psp == 0)
    {
      char sz_str[24];
      format_kb(paras_to_kb(block_paras), sz_str, sizeof(sz_str));
      snprintf(line_buf, sizeof(line_buf), "    %04Xh  %8s   %s",
               curr_seg, sz_str, is_umb ? "Upper" : "Conventional");
      print_line(line_buf);
    }

    if (mcb.id == 'Z')
      break;

    if (curr_seg + 1 + mcb.size > 0xffff)
      break; /* corrupt chain: don't wrap around into low memory */
    curr_seg += 1 + mcb.size;
  }

  if (!orig_umblink)
  {
    r.x.ax = 0x5803;
    r.x.bx = 0;
    __dpmi_int(0x21, &r);
  }

  if (page_stop)
    return;

  /*
   * The MCB arena walked above starts after the resident DOS kernel,
   * interrupt vector table and BIOS data area, so its sum is a few KB
   * short of the true installed conventional memory. INT 12h reports
   * the BIOS-detected total in KB directly, which is what real MEM.COM
   * shows as "Total conventional memory".
   */
  __dpmi_regs ir = {};
  __dpmi_int(0x12, &ir);
  if (ir.x.ax > 0)
    conv_total_kb = ir.x.ax;
  else
    conv_total_kb = conv_total_paras ? paras_to_kb(conv_total_paras) : CONV_MAX_KB;
  if (conv_total_kb > CONV_MAX_KB)
    conv_total_kb = CONV_MAX_KB;

  uint32_t conv_free_kb = paras_to_kb(conv_free_paras);
  uint32_t conv_used_kb = (conv_total_kb > conv_free_kb) ?
    (conv_total_kb - conv_free_kb) : 0;
  uint32_t umb_total_kb = UMB_TOTAL_KB;
  uint32_t umb_free_kb = paras_to_kb(umb_free_paras);
  uint32_t umb_used_kb = (umb_total_kb > umb_free_kb) ?
    (umb_total_kb - umb_free_kb) : 0;

  if (opt_classify)
  {
    print_line("");
    print_line("Modules using memory below 1 MB:");
    print_line("");
    print_line("  Name         Total   Conventional   Upper Memory");
    print_line("  --------  --------   ------------   ------------");

    /* loaded modules in the order they sit in memory, free memory last,
     * the way MEM.EXE lists them */
    int pass, i;
    for (pass = 0; pass < 2; pass++)
    {
      for (i = 0; i < num_modules; i++)
      {
        char t_str[24], c_str[24], u_str[24];
        int is_free = (modules[i].owner_psp == 0);

        if (is_free != pass)
          continue;

        format_kb(paras_to_kb(modules[i].conv_paras + modules[i].umb_paras),
                  t_str, sizeof(t_str));
        format_kb(paras_to_kb(modules[i].conv_paras), c_str, sizeof(c_str));
        format_kb(paras_to_kb(modules[i].umb_paras), u_str, sizeof(u_str));

        snprintf(line_buf, sizeof(line_buf), "  %-8s  %8s   %12s   %12s",
                 modules[i].name, t_str, c_str, u_str);
        print_line(line_buf);
      }
    }
    print_line("");
  }

  /* Query XMS */
  uint32_t xms_free_kb = 0;
  int xms_present = 0;

  __dpmi_regs dr = {};
  dr.x.ax = 0x4300;
  __dpmi_int(0x2f, &dr);
  if ((dr.h.al & 0xff) == 0x80)
  {
    xms_present = 1;
    dr.x.ax = 0x4310;
    __dpmi_int(0x2f, &dr);
    __dpmi_raddr xms_entry;
    xms_entry.segment = dr.x.es;
    xms_entry.offset16 = dr.x.bx;

    __dpmi_regs xr = {};
    xr.h.ah = 0x08; /* Query Free Extended Memory */
    xr.x.cs = xms_entry.segment;
    xr.x.ip = xms_entry.offset16;
    __dpmi_simulate_real_mode_procedure_retf(&xr);

    if (xr.h.bl == 0)
      xms_free_kb = xr.x.dx;
  }

  uint32_t xms_total_kb = query_ext_mem_total_kb();

  if (!xms_present && xms_total_kb > 0)
  {
    /* No XMS manager loaded, so nothing has claimed any of it yet. */
    xms_free_kb = xms_total_kb;
    xms_present = 1;
  }

  if (xms_total_kb < xms_free_kb)
  {
    /*
     * The BIOS-reported extended memory size is a legacy/compatibility
     * figure and can be smaller than the pool the XMS manager actually
     * hands out (observed under dosemu2, whose virtual XMS pool isn't
     * tied to what INT 15h reports). Never show a total smaller than
     * what XMS itself claims is free - that would be a nonsensical
     * "free > total" display.
     */
    xms_total_kb = xms_free_kb;
  }

  uint32_t xms_used_kb = xms_total_kb - xms_free_kb;

  /* Query EMS */
  uint32_t ems_total_kb = 0;
  uint32_t ems_free_kb = 0;
  int ems_present = 0;

  __dpmi_raddr int67;
  __dpmi_get_real_mode_interrupt_vector(0x67, &int67);
  if (int67.segment != 0)
  {
    __dpmi_regs er = {};
    er.h.ah = 0x40;
    __dpmi_int(0x67, &er);
    if (er.h.ah == 0)
    {
      er.h.ah = 0x42;
      __dpmi_int(0x67, &er);
      if (er.h.ah == 0)
      {
        ems_present = 1;
        ems_free_kb = (uint32_t)er.x.bx * EMS_PAGE_KB;
        ems_total_kb = (uint32_t)er.x.dx * EMS_PAGE_KB;
      }
    }
  }

  /* Print Memory Summary Table */
  char str_tot[24], str_used[24], str_free[24];

  print_line("");
  print_line("Memory Type         Total      Used       Free");
  print_line("----------------  --------   --------   --------");

  format_kb(conv_total_kb, str_tot, sizeof(str_tot));
  format_kb(conv_used_kb, str_used, sizeof(str_used));
  format_kb(conv_free_kb, str_free, sizeof(str_free));
  snprintf(line_buf, sizeof(line_buf), "%-16s  %8s   %8s   %8s",
           "Conventional", str_tot, str_used, str_free);
  print_line(line_buf);

  format_kb(umb_total_kb, str_tot, sizeof(str_tot));
  format_kb(umb_used_kb, str_used, sizeof(str_used));
  format_kb(umb_free_kb, str_free, sizeof(str_free));
  snprintf(line_buf, sizeof(line_buf), "%-16s  %8s   %8s   %8s",
           "Upper", str_tot, str_used, str_free);
  print_line(line_buf);

  format_kb(RESERVED_TOTAL_KB, str_tot, sizeof(str_tot));
  format_kb(0, str_free, sizeof(str_free));
  snprintf(line_buf, sizeof(line_buf), "%-16s  %8s   %8s   %8s",
           "Reserved", str_tot, str_tot, str_free);
  print_line(line_buf);

  if (xms_present)
  {
    format_kb(xms_total_kb, str_tot, sizeof(str_tot));
    format_kb(xms_used_kb, str_used, sizeof(str_used));
    format_kb(xms_free_kb, str_free, sizeof(str_free));
    snprintf(line_buf, sizeof(line_buf), "%-16s  %8s   %8s   %8s",
             "Extended (XMS)", str_tot, str_used, str_free);
    print_line(line_buf);
  }

  print_line("----------------  --------   --------   --------");

  format_kb(conv_total_kb + umb_total_kb + RESERVED_TOTAL_KB + xms_total_kb,
            str_tot, sizeof(str_tot));
  format_kb(conv_used_kb + umb_used_kb + RESERVED_TOTAL_KB + xms_used_kb,
            str_used, sizeof(str_used));
  format_kb(conv_free_kb + umb_free_kb + xms_free_kb, str_free, sizeof(str_free));
  snprintf(line_buf, sizeof(line_buf), "%-16s  %8s   %8s   %8s",
           "Total memory", str_tot, str_used, str_free);
  print_line(line_buf);

  print_line("");

  /* the reserved region is left out here, as MEM.EXE leaves it out */
  format_kb(conv_total_kb + umb_total_kb, str_tot, sizeof(str_tot));
  format_kb(conv_used_kb + umb_used_kb, str_used, sizeof(str_used));
  format_kb(conv_free_kb + umb_free_kb, str_free, sizeof(str_free));
  snprintf(line_buf, sizeof(line_buf), "%-16s  %8s   %8s   %8s",
           "Total under 1 MB", str_tot, str_used, str_free);
  print_line(line_buf);

  print_line("");

  if (ems_present)
  {
    print_kb_and_bytes("Total Expanded (EMS)", ems_total_kb, ems_total_kb * 1024);
    print_kb_and_bytes("Free Expanded (EMS)", ems_free_kb, ems_free_kb * 1024);
    print_line("");
  }

  /*
   * We are a DPMI client ourselves, so the host is always there. Its
   * memory is reported apart from the table above: under some hosts it
   * is carved out of XMS, under others (dosemu2) it is a separate pool,
   * so adding it to the totals would count the same memory twice.
   */
  __dpmi_free_mem_info dpmi_info;
  if (__dpmi_get_free_memory_information(&dpmi_info) == 0)
  {
    uint32_t dpmi_total = dpmi_info.total_number_of_physical_pages;
    uint32_t dpmi_free = dpmi_info.total_number_of_free_pages;
    uint32_t dpmi_largest = dpmi_info.largest_available_free_block_in_bytes;
    int shown = 0;

    if (dpmi_total != DPMI_UNKNOWN)
    {
      print_dpmi_pages("Total DPMI memory", dpmi_total);
      shown = 1;
    }
    if (dpmi_total != DPMI_UNKNOWN && dpmi_free != DPMI_UNKNOWN &&
        dpmi_free <= dpmi_total)
      print_dpmi_pages("Used DPMI memory", dpmi_total - dpmi_free);
    if (dpmi_free != DPMI_UNKNOWN)
    {
      print_dpmi_pages("Free DPMI memory", dpmi_free);
      shown = 1;
    }
    if (dpmi_largest != DPMI_UNKNOWN)
    {
      print_kb_and_bytes("Largest free DPMI block",
                         dpmi_largest / 1024, dpmi_largest);
      shown = 1;
    }
    if (shown)
      print_line("");
  }

  print_kb_and_bytes("Largest executable program size",
                     paras_to_kb(conv_largest_paras), conv_largest_paras * 16);
  print_kb_and_bytes("Largest available upper memory block",
                     paras_to_kb(umb_largest_paras), umb_largest_paras * 16);
}
