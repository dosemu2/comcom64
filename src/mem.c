/*
 *  comcom64 - 64bit command.com
 *  mem.c: FreeDOS-like MEM command implementation
 *  Copyright (C) 2026  @stsp
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

#include "asm.h"
#include "command.h"
#include "umb.h"
#include "mem.h"

struct MCB {
    char id;                  /* 'M' (0x4D) or 'Z' (0x5A) */
    uint16_t owner_psp;       /* 0 = free, 8 = system, else PSP segment */
    uint16_t size;            /* Size in 16-byte paragraphs */
    char reserved[3];
    char name[8];             /* Owner/program name in DOS 4+ */
} __attribute__((packed));

struct module_entry {
    uint16_t owner_psp;
    char name[9];
    uint32_t conv_bytes;
    uint32_t umb_bytes;
};

static int opt_classify = 0;
static int opt_free = 0;
static int opt_debug = 0;
static int opt_page = 0;
static int page_line_count = 0;

static void reset_page(void)
{
    page_line_count = 0;
}

static void print_line(const char *str)
{
    printf("%s\n", str);
    if (opt_page)
    {
        page_line_count++;
        if (page_line_count >= 23)
        {
            printf("Press any key to continue . . .");
            fflush(stdout);
            getch();
            printf("\r                               \r");
            page_line_count = 0;
        }
    }
}

static void format_number(uint32_t val, char *buf, size_t buf_size)
{
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%lu", (unsigned long)val);
    int len = strlen(tmp);
    int dest_idx = 0;

    for (int i = 0; i < len; i++)
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

static void get_mcb_name(uint16_t seg, const struct MCB *mcb, char *name_out, size_t name_out_size)
{
    memset(name_out, 0, name_out_size);

    if (mcb->owner_psp == 0)
    {
        strncpy(name_out, "FREE", name_out_size - 1);
        return;
    }
    if (mcb->owner_psp == 8)
    {
        strncpy(name_out, "SYSTEM", name_out_size - 1);
        return;
    }

    /* Check if mcb->name contains printable characters */
    int valid_mcb_name = 1;
    for (int i = 0; i < 8; i++)
    {
        if (mcb->name[i] == 0)
            break;
        if (!isprint((unsigned char)mcb->name[i]))
        {
            valid_mcb_name = 0;
            break;
        }
    }

    if (valid_mcb_name && mcb->name[0] != 0 && mcb->name[0] != ' ')
    {
        int i = 0;
        for (; i < 8 && mcb->name[i] != 0 && !isspace((unsigned char)mcb->name[i]); i++)
        {
            name_out[i] = mcb->name[i];
        }
        name_out[i] = '\0';
        if (strlen(name_out) > 0)
            return;
    }

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
    opt_classify = 0;
    opt_free = 0;
    opt_debug = 0;
    opt_page = 0;

    const char *p = arg;
    while (*p)
    {
        while (isspace((unsigned char)*p))
            p++;
        if (*p == '/' || *p == '-')
        {
            p++;
            if (strnicmp(p, "CLASSIFY", 8) == 0 || strnicmp(p, "C", 1) == 0)
            {
                opt_classify = 1;
                while (isalnum((unsigned char)*p))
                    p++;
            }
            else if (strnicmp(p, "FREE", 4) == 0 || strnicmp(p, "F", 1) == 0)
            {
                opt_free = 1;
                while (isalnum((unsigned char)*p))
                    p++;
            }
            else if (strnicmp(p, "DEBUG", 5) == 0 || strnicmp(p, "D", 1) == 0)
            {
                opt_debug = 1;
                while (isalnum((unsigned char)*p))
                    p++;
            }
            else if (strnicmp(p, "PAGE", 4) == 0 || strnicmp(p, "P", 1) == 0)
            {
                opt_page = 1;
                while (isalnum((unsigned char)*p))
                    p++;
            }
            else if (*p == '?' || strnicmp(p, "HELP", 4) == 0)
            {
                show_help();
                return;
            }
            else
            {
                p++;
            }
        }
        else
        {
            p++;
        }
    }

    reset_page();

    /* Save UMB link state and link UMBs for memory scan */
    __dpmi_regs r = {};
    r.x.ax = 0x5800;
    __dpmi_int(0x21, &r);
    uint16_t orig_strat = r.x.ax;

    r.x.ax = 0x5802;
    __dpmi_int(0x21, &r);
    uint8_t orig_umblink = r.h.al;

    link_umb(orig_strat);

    /* Get List of Lists -> First MCB */
    r.h.ah = 0x52;
    __dpmi_int(0x21, &r);
    uint32_t sysvars_addr = (r.x.es << 4) + r.x.bx;
    uint16_t first_mcb_seg = _farpeekw(_dos_ds, sysvars_addr - 2);

    uint32_t conv_total = 0;
    uint32_t conv_free = 0;
    uint32_t conv_largest = 0;

    uint32_t umb_total = 0;
    uint32_t umb_free = 0;
    uint32_t umb_largest = 0;

    struct module_entry modules[64];
    int num_modules = 0;
    memset(modules, 0, sizeof(modules));

    char line_buf[160];

    if (opt_debug)
    {
        print_line("Memory Detail:");
        print_line("  Segment   Owner    Size (Bytes)   Type/Name");
        print_line("  -------  -------  --------------  -----------");
    }
    else if (opt_free)
    {
        print_line("Free Memory Blocks:");
        print_line("  Segment   Size (Bytes)    Memory Type");
        print_line("  -------  --------------   -----------");
    }

    uint16_t curr_seg = first_mcb_seg;
    while (curr_seg != 0)
    {
        struct MCB mcb;
        dosmemget(curr_seg << 4, sizeof(mcb), &mcb);

        if (mcb.id != 'M' && mcb.id != 'Z')
            break;

        uint32_t block_bytes = (uint32_t)mcb.size * 16;
        int is_umb = (curr_seg >= 0xA000);

        char name[16];
        get_mcb_name(curr_seg, &mcb, name, sizeof(name));

        if (is_umb)
        {
            umb_total += block_bytes + 16; /* include MCB header */
            if (mcb.owner_psp == 0)
            {
                umb_free += block_bytes;
                if (block_bytes > umb_largest)
                    umb_largest = block_bytes;
            }
        }
        else
        {
            conv_total += block_bytes + 16; /* include MCB header */
            if (mcb.owner_psp == 0)
            {
                conv_free += block_bytes;
                if (block_bytes > conv_largest)
                    conv_largest = block_bytes;
            }
        }

        /* Collect module info for /CLASSIFY */
        if (opt_classify)
        {
            int found = -1;
            for (int i = 0; i < num_modules; i++)
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
                    modules[found].umb_bytes += block_bytes + 16;
                else
                    modules[found].conv_bytes += block_bytes + 16;
            }
        }

        if (opt_debug)
        {
            char sz_str[32];
            format_number(block_bytes, sz_str, sizeof(sz_str));
            snprintf(line_buf, sizeof(line_buf), "   %04Xh    %04Xh   %14s  %s",
                     curr_seg, mcb.owner_psp, sz_str, name);
            print_line(line_buf);
        }
        else if (opt_free && mcb.owner_psp == 0)
        {
            char sz_str[32];
            format_number(block_bytes, sz_str, sizeof(sz_str));
            snprintf(line_buf, sizeof(line_buf), "   %04Xh   %14s   %s",
                     curr_seg, sz_str, is_umb ? "Upper" : "Conventional");
            print_line(line_buf);
        }

        if (mcb.id == 'Z')
            break;

        curr_seg += 1 + mcb.size;
    }

    if (!orig_umblink)
        unlink_umb();

    /* Fill default conventional memory total if 0 */
    if (conv_total == 0 || conv_total < 640 * 1024)
    {
        __dpmi_regs ir = {};
        __dpmi_int(0x12, &ir);
        if (ir.x.ax > 0)
            conv_total = (uint32_t)ir.x.ax * 1024;
        else
            conv_total = 640 * 1024;
    }

    if (opt_classify)
    {
        print_line("");
        print_line("Modules using memory below 1 MB:");
        print_line("");
        print_line("  Name           Total           Conventional       Upper Memory");
        print_line("  --------   -----------------   ----------------   ----------------");

        for (int i = 0; i < num_modules; i++)
        {
            uint32_t tot = modules[i].conv_bytes + modules[i].umb_bytes;
            char t_str[24], c_str[24], u_str[24];
            format_number(tot, t_str, sizeof(t_str));
            format_number(modules[i].conv_bytes, c_str, sizeof(c_str));
            format_number(modules[i].umb_bytes, u_str, sizeof(u_str));

            snprintf(line_buf, sizeof(line_buf), "  %-8s   %12s (%3luK)   %12s (%3luK)   %12s (%3luK)",
                     modules[i].name,
                     t_str, (unsigned long)(tot / 1024),
                     c_str, (unsigned long)(modules[i].conv_bytes / 1024),
                     u_str, (unsigned long)(modules[i].umb_bytes / 1024));
            print_line(line_buf);
        }
        print_line("");
    }

    /* Query XMS */
    uint32_t xms_free_kb = 0;
    uint32_t xms_largest_free_kb = 0;
    int xms_present = 0;

    r.x.ax = 0x4300;
    __dpmi_int(0x2f, &r);
    if ((r.h.al & 0xff) == 0x80)
    {
        xms_present = 1;
        r.x.ax = 0x4310;
        __dpmi_int(0x2f, &r);
        __dpmi_raddr xms_entry;
        xms_entry.segment = r.x.es;
        xms_entry.offset16 = r.x.bx;

        __dpmi_regs xr = {};
        xr.h.ah = 0x08; /* Query Free Extended Memory */
        xr.x.cs = xms_entry.segment;
        xr.x.ip = xms_entry.offset16;
        __dpmi_simulate_real_mode_procedure_retf(&xr);

        if (xr.h.bl == 0)
        {
            xms_largest_free_kb = xr.x.ax;
            xms_free_kb = xr.x.dx;
        }
    }

    if (!xms_present || xms_free_kb == 0)
    {
        __dpmi_regs xr = {};
        xr.h.ah = 0x88;
        __dpmi_int(0x15, &xr);
        if (!(xr.x.flags & 1) && xr.x.ax > 0)
        {
            xms_free_kb = xr.x.ax;
            xms_largest_free_kb = xr.x.ax;
            xms_present = 1;
        }
    }

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
                ems_free_kb = (uint32_t)er.x.bx * 16;
                ems_total_kb = (uint32_t)er.x.dx * 16;
            }
        }
    }

    /* Print Memory Summary Table */
    print_line("");
    print_line("Memory Type         Total        Used        Free");
    print_line("----------------  -----------  -----------  -----------");

    uint32_t conv_used = (conv_total > conv_free) ? (conv_total - conv_free) : 0;
    uint32_t umb_used = (umb_total > umb_free) ? (umb_total - umb_free) : 0;
    uint32_t xms_total_bytes = xms_free_kb * 1024;
    uint32_t xms_free_bytes = xms_free_kb * 1024;
    uint32_t xms_used_bytes = 0;

    char str_c_tot[20], str_c_used[20], str_c_free[20];
    format_number(conv_total, str_c_tot, sizeof(str_c_tot));
    format_number(conv_used, str_c_used, sizeof(str_c_used));
    format_number(conv_free, str_c_free, sizeof(str_c_free));

    snprintf(line_buf, sizeof(line_buf), "Conventional      %11s  %11s  %11s",
             str_c_tot, str_c_used, str_c_free);
    print_line(line_buf);

    if (umb_total > 0)
    {
        char str_u_tot[20], str_u_used[20], str_u_free[20];
        format_number(umb_total, str_u_tot, sizeof(str_u_tot));
        format_number(umb_used, str_u_used, sizeof(str_u_used));
        format_number(umb_free, str_u_free, sizeof(str_u_free));

        snprintf(line_buf, sizeof(line_buf), "Upper             %11s  %11s  %11s",
                 str_u_tot, str_u_used, str_u_free);
        print_line(line_buf);
    }

    if (xms_present)
    {
        char str_x_tot[20], str_x_used[20], str_x_free[20];
        format_number(xms_total_bytes, str_x_tot, sizeof(str_x_tot));
        format_number(xms_used_bytes, str_x_used, sizeof(str_x_used));
        format_number(xms_free_bytes, str_x_free, sizeof(str_x_free));

        snprintf(line_buf, sizeof(line_buf), "Extended (XMS)    %11s  %11s  %11s",
                 str_x_tot, str_x_used, str_x_free);
        print_line(line_buf);
    }

    print_line("----------------  -----------  -----------  -----------");

    uint32_t grand_tot = conv_total + umb_total + xms_total_bytes;
    uint32_t grand_used = conv_used + umb_used + xms_used_bytes;
    uint32_t grand_free = conv_free + umb_free + xms_free_bytes;

    char str_g_tot[20], str_g_used[20], str_g_free[20];
    format_number(grand_tot, str_g_tot, sizeof(str_g_tot));
    format_number(grand_used, str_g_used, sizeof(str_g_used));
    format_number(grand_free, str_g_free, sizeof(str_g_free));

    snprintf(line_buf, sizeof(line_buf), "Total memory      %11s  %11s  %11s",
             str_g_tot, str_g_used, str_g_free);
    print_line(line_buf);

    print_line("");

    uint32_t u1mb_tot = conv_total + umb_total;
    uint32_t u1mb_used = conv_used + umb_used;
    uint32_t u1mb_free = conv_free + umb_free;

    char str_1_tot[20], str_1_used[20], str_1_free[20];
    format_number(u1mb_tot, str_1_tot, sizeof(str_1_tot));
    format_number(u1mb_used, str_1_used, sizeof(str_1_used));
    format_number(u1mb_free, str_1_free, sizeof(str_1_free));

    snprintf(line_buf, sizeof(line_buf), "Total under 1 MB  %11s  %11s  %11s",
             str_1_tot, str_1_used, str_1_free);
    print_line(line_buf);

    print_line("");

    if (ems_present)
    {
        char str_ems_tot[20], str_ems_free[20];
        format_number(ems_total_kb * 1024, str_ems_tot, sizeof(str_ems_tot));
        format_number(ems_free_kb * 1024, str_ems_free, sizeof(str_ems_free));

        snprintf(line_buf, sizeof(line_buf), "Total Expanded (EMS)          %10lu KB (%s bytes)",
                 (unsigned long)ems_total_kb, str_ems_tot);
        print_line(line_buf);

        snprintf(line_buf, sizeof(line_buf), "Free Expanded (EMS)           %10lu KB (%s bytes)",
                 (unsigned long)ems_free_kb, str_ems_free);
        print_line(line_buf);
        print_line("");
    }

    char str_c_lrg[20];
    format_number(conv_largest, str_c_lrg, sizeof(str_c_lrg));
    snprintf(line_buf, sizeof(line_buf), "Largest executable program size       %7lu KB (%s bytes)",
             (unsigned long)(conv_largest / 1024), str_c_lrg);
    print_line(line_buf);

    if (umb_total > 0)
    {
        char str_u_lrg[20];
        format_number(umb_largest, str_u_lrg, sizeof(str_u_lrg));
        snprintf(line_buf, sizeof(line_buf), "Largest available upper memory block  %7lu KB (%s bytes)",
                 (unsigned long)(umb_largest / 1024), str_u_lrg);
        print_line(line_buf);
    }

    print_line("MS-DOS is resident in the high memory area.");
}
