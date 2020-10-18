/* Command shell definitions and portability between
 * different environments, etc. by Hanzac Chen
 */

#ifndef __COMMAND_H__
#define __COMMAND_H__

#ifdef __GNUC__
#define __CMD_COMPILER__ "GCC "
#endif

/*
 * Command.com shell modes
 */
#define SHELL_NORMAL           0  // interactive mode, user can exit
#define SHELL_PERMANENT        1  // interactive mode, user cannot exit
#define SHELL_SINGLE_CMD       2  // non-interactive, run one command, then exit
#define SHELL_STARTUP_WITH_CMD 3  // run one command on startup, interactive thereafter, user can exit

/*
 * Pipe defines
 */
#define STDIN_INDEX  0
#define STDOUT_INDEX 1

/*
 * Max subdirectory level, used by /S switch within XCOPY, ATTRIB and DELTREE
 */
#define MAX_SUBDIR_LEVEL       15

/*
 * File transfer modes
 */
#define FILE_XFER_COPY         0
#define FILE_XFER_XCOPY        1
#define FILE_XFER_MOVE         2

/*
 * Count of the number of valid commands
 */
#define CMD_TABLE_COUNT        (sizeof(cmd_table) / sizeof(struct built_in_cmd))

/*
 * Temporarily and Slightly FIX the keyboard problem
 */
#define GET_ENHANCED_KEYSTROKE                0x10
#define GET_EXTENDED_SHIFT_STATES             0x12
#define KEYB_FLAG_INSERT    0x0080
#define KEY_ASCII(k)    (k & 0x00FF)
#define KEY_SCANCODE(k) (k >> 0x08 )
#define KEY_EXTM(k)     (k & 0xFF1F)
#define KEY_EXT          0x00E0
#define KEY_ESC          KEY_ASCII(0x011B)
#define KEY_ENTER        KEY_ASCII(0x1C0D)
#define KEY_BACKSPACE    KEY_ASCII(0x0E08)
#define KEY_HOME         KEY_EXTM(0x47E0)
#define KEY_UP           KEY_EXTM(0x48E0)
#define KEY_LEFT         KEY_EXTM(0x4BE0)
#define KEY_RIGHT        KEY_EXTM(0x4DE0)
#define KEY_END          KEY_EXTM(0x4FE0)
#define KEY_DOWN         KEY_EXTM(0x50E0)
#define KEY_INSERT       KEY_EXTM(0x52E0)
#define KEY_DELETE       KEY_EXTM(0x53E0)

/*
 * Common definitions
 */
#if defined(__MINGW32__) || defined(__WATCOMC__)
#include <direct.h>

#define _fixpath(a,b) _fullpath(b,a,_MAX_PATH)
#define fnsplit(p,drive,dir,n,e) _splitpath(p,drive,dir,n,e)
#define fnmerge(p,drive,dir,n,e) _makepath(p,drive,dir,n,e)

/* Cursor shape */
#define _NOCURSOR      0
#define _SOLIDCURSOR   1
#define _NORMALCURSOR  2

/* Additional access() checks */
#define D_OK	0x10

#define FA_RDONLY      1
#define FA_HIDDEN      2
#define FA_SYSTEM      4
#define FA_LABEL       8
#define FA_DIREC       16
#define FA_ARCH        32
#define MAXINT			(0x7fffffff)
#define MAXPATH			_MAX_PATH
#define MAXDRIVE		3
#define MAXDIR			256
#define MAXFILE			256
#define MAXEXT			255

/* File find */
typedef struct _finddata_t finddata_t;
static inline int findfirst_f(const char *pathname, finddata_t *ff, int attrib, long *handle)
{
	if (attrib == FA_LABEL) {
		return -1;
	} else {
		long h = _findfirst(pathname, ff);
	    if (h != -1) {
	    	if (handle != NULL)
				*handle = h;
			return 0;
		} else {
			return -1;
		}
	}
}
static inline int findnext_f(finddata_t *ff, long handle)
{
    return _findnext(handle, ff);
}
static inline int findclose_f(long handle)
{
	return _findclose(handle);
}
#define FINDDATA_T_FILENAME(f) f.name
#define FINDDATA_T_ATTRIB(f) f.attrib
#define FINDDATA_T_SIZE(f) f.size
#define FINDDATA_T_WDATE_YEAR(f) localtime(&f.time_write)->tm_year+1900
#define FINDDATA_T_WDATE_MON(f) localtime(&f.time_write)->tm_mon+1
#define FINDDATA_T_WDATE_DAY(f) localtime(&f.time_write)->tm_mday
#define FINDDATA_T_WTIME_HOUR(f) localtime(&f.time_write)->tm_hour
#define FINDDATA_T_WTIME_MIN(f) localtime(&f.time_write)->tm_min

typedef struct _diskfree_t diskfree_t;
#define DISKFREE_T_AVAIL(d) d.avail_clusters
#define DISKFREE_T_TOTAL(d) d.total_clusters
#define DISKFREE_T_BSEC(d) d.bytes_per_sector
#define DISKFREE_T_SCLUS(d) d.sectors_per_cluster

#define getdfree(d,p) _getdiskfree(d,p)

#endif

/*
 * Different compilers
 */
#ifdef __MINGW32__
#include <errno.h>
#include <unistd.h>
#include <windows.h>

#define pipe(filedes) _pipe(filedes, 0x4000, O_TEXT)

/* Conio utilites */
#define cprintf(...) _cprintf(__VA_ARGS__)
#define cputs(s) _cputs(s)
static CONSOLE_SCREEN_BUFFER_INFO info;
static int __conio_x = 0;
static int __conio_y = 0;
static int __conio_top = 0;
static int __conio_left = 0;
static int __conio_width = 80;
static int __conio_height = 25;
static WORD __conio_attrib = 0x07;
static void __fill_conio_info (void)
{
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info);
    __conio_left = info.srWindow.Left;
    __conio_top = info.srWindow.Top;
    __conio_x = info.dwCursorPosition.X - __conio_left + 1;
    __conio_y = info.dwCursorPosition.Y - __conio_top  + 1;
    __conio_width = info.srWindow.Right - info.srWindow.Left + 1;
    __conio_height = info.srWindow.Bottom - info.srWindow.Top + 1;
    __conio_attrib = info.wAttributes;
}
static inline void gotoxy(int x, int y)
{
  COORD c;
  c.X = __conio_left + x - 1;
  c.Y = __conio_top  + y - 1;
  SetConsoleCursorPosition (GetStdHandle(STD_OUTPUT_HANDLE), c);
}
static inline void clrscr(void)
{
    DWORD written, i;
    __fill_conio_info();
    for (i = __conio_top; i < __conio_top + __conio_height; i++) {
        FillConsoleOutputAttribute (GetStdHandle(STD_OUTPUT_HANDLE), __conio_attrib, __conio_width, (COORD) {__conio_left, i}, &written);
        FillConsoleOutputCharacter (GetStdHandle(STD_OUTPUT_HANDLE), ' ', __conio_width, (COORD) {__conio_left, i}, &written);
    }
    gotoxy (1, 1);
}
static inline void clreol(void)
{
    COORD coord;
    DWORD written;
    __fill_conio_info();
    FillConsoleOutputCharacter (GetStdHandle(STD_OUTPUT_HANDLE), ' ', __conio_width - __conio_x + 1, coord, &written);
    gotoxy (__conio_x, __conio_y);
}
static inline void _setcursortype(int type)
{
    CONSOLE_CURSOR_INFO cursor_info;
    cursor_info.bVisible = TRUE;
    switch (type) {
		case _NOCURSOR:
            cursor_info.bVisible = FALSE;
			break;
		case _SOLIDCURSOR:
            cursor_info.dwSize = 100;
            break;
		default:
            cursor_info.dwSize = 1;
			break;
	}
    SetConsoleCursorInfo (GetStdHandle(STD_OUTPUT_HANDLE), &cursor_info);
}
#define delay(t) Sleep(t/1000)

/* File attributes */
static inline unsigned int setfileattr(const char *filename, unsigned int attr)
{
    unsigned int ret = 0;
    DWORD win32_attr = 0;

    win32_attr |= attr&_A_RDONLY ? FILE_ATTRIBUTE_READONLY : 0;
    win32_attr |= attr&_A_HIDDEN ? FILE_ATTRIBUTE_HIDDEN : 0;
    win32_attr |= attr&_A_SYSTEM ? FILE_ATTRIBUTE_SYSTEM : 0;
    win32_attr |= attr&_A_SUBDIR ? FILE_ATTRIBUTE_DIRECTORY : 0;
    win32_attr |= attr&_A_ARCH ? FILE_ATTRIBUTE_ARCHIVE : 0;
    if (!SetFileAttributes(filename, win32_attr)) {
        errno = ENOENT;
        ret = 2; /* File not found */
    }
    return ret;
}
static inline unsigned int getfileattr(const char *filename, unsigned int *p_attr)
{
    unsigned int ret = 0;
    DWORD win32_attr = GetFileAttributes(filename);

    if (win32_attr != INVALID_FILE_ATTRIBUTES) {
        if (p_attr != NULL) {
            *p_attr = 0;
            /* *p_attr |= attr&0 ? _A_VOLID : 0
               *p_attr |= attr&FILE_ATTRIBUTE_NORMAL ? _A_NORMAL : 0; */
            *p_attr |= win32_attr&FILE_ATTRIBUTE_READONLY ? _A_RDONLY : 0;
            *p_attr |= win32_attr&FILE_ATTRIBUTE_HIDDEN ? _A_HIDDEN : 0;
            *p_attr |= win32_attr&FILE_ATTRIBUTE_SYSTEM ? _A_SYSTEM : 0;
            *p_attr |= win32_attr&FILE_ATTRIBUTE_DIRECTORY ? _A_SUBDIR : 0;
            *p_attr |= win32_attr&FILE_ATTRIBUTE_ARCHIVE ? _A_ARCH : 0;
        }
    } else {
        errno = ENOENT;
        ret = 2; /* File not found */
    }

    return ret;
}
static inline int file_access(const char *filename, int flags)
{
    if (flags & D_OK) {
        unsigned int attr = 0;
        getfileattr(filename, &attr);
        if (attr & _A_SUBDIR) {
            return 0;
        } else {
            errno = EACCES;
            return -1; /* not a directory */
        }
    }
    return access(filename, flags);
}
static inline int file_copytime(int desc_handle, int src_handle)
{
  int ret;
  struct stat source_st;
  struct _utimbuf dest_ut;
  if ((ret = fstat(src_handle, &source_st)) == 0) {
    dest_ut.actime = source_st.st_atime;
    dest_ut.modtime = source_st.st_mtime;
    ret = _futime(desc_handle, &dest_ut);
  }
  return ret;
}
/* Disk free */
static inline void setdrive(unsigned int drive, unsigned int *p_drives)
{
    _chdrive(drive);
}
static inline void getdrive(unsigned int *p_drive)
{
    if (p_drive != NULL)
        *p_drive = _getdrive();
}

#elif __WATCOMC__
#ifndef __VERSION__
#define __VERSION__ "1.6"
#endif

#ifndef __CMD_COMPILER__
#define __CMD_COMPILER__ "WATCOMC "
#endif

int pipe( int *__phandles)
{
  return 0;
}

static inline void clrscr(void)
{
}
static inline void clreol(void)
{
}
static inline void _setcursortype(int type)
{
}

struct ftime {
  unsigned ft_tsec:5;	/* 0-29, double to get real seconds */
  unsigned ft_min:6;	/* 0-59 */
  unsigned ft_hour:5;	/* 0-23 */
  unsigned ft_day:5;	/* 1-31 */
  unsigned ft_month:4;	/* 1-12 */
  unsigned ft_year:7;	/* since 1980 */
};

static inline void setdrive(unsigned int drive, unsigned int *p_drives)
{
    _dos_setdrive(drive, p_drives);
}
static inline void getdrive(unsigned int *p_drive)
{
    _dos_getdrive(p_drive);
}

/* File attributes */
static inline unsigned int setfileattr(const char *filename, unsigned int attr)
{
    return  _dos_setfileattr(filename, attr);
}
static inline unsigned int getfileattr(const char *filename, unsigned int *p_attr)
{
    return  _dos_getfileattr(filename, p_attr);
}
static inline int file_access(const char *filename, int flags)
{
    return access(filename, flags);
}
static inline int file_copytime(int desc_handle, int src_handle)
{
  int ret;
  unsigned short _date, _time;
  if ((ret = _dos_getftime(src_handle, &_date, &_time)) == 0)
    ret = _dos_setftime(desc_handle, _date, _time);
  return ret;
}

#elif __DJGPP__
#include <dir.h>
#include <bios.h>
#include <values.h>
#include <unistd.h>
#include <sys/exceptn.h>

#ifndef USE_CONIO_OUT
#define cprintf printf
#define cputs(s) fputs(s, stdout)
#define putch(c) putchar(c)
#endif

#define mkdir(dir_path) mkdir(dir_path, S_IWUSR)

/* File find */
typedef struct ffblk finddata_t;
#define FINDDATA_T_FILENAME(f) (f).ff_name
#define FINDDATA_T_ATTRIB(f) (f).ff_attrib
#define FINDDATA_T_SIZE(f) (f).ff_fsize
#define FINDDATA_T_WDATE_YEAR(f) (((f).ff_fdate>>9)&0x7F)+1980
#define FINDDATA_T_WDATE_MON(f) ((f).ff_fdate>>5)&0xF
#define FINDDATA_T_WDATE_DAY(f) ((f).ff_fdate)&0x1F
#define FINDDATA_T_WTIME_HOUR(f) ((f).ff_ftime>>11)&0x1F
#define FINDDATA_T_WTIME_MIN(f) ((f).ff_ftime>>5)&0x3F
static inline int findfirst_f(const char *pathname, finddata_t *ff, int attrib, long *handle)
{
    int err = findfirst(pathname, ff, attrib);
    if (err)
        return err;
    if (attrib == FA_DIREC && FINDDATA_T_ATTRIB(*ff) != attrib)
        return -1;
    return 0;
}
static inline int findnext_f(finddata_t *ff, long handle)
{
    return findnext(ff);
}
static inline int findclose_f(long handle)
{
    return 0;
}
/* File attributes */
static inline unsigned int setfileattr(const char *filename, unsigned int attr)
{
    return  _dos_setfileattr(filename, attr);
}
static inline unsigned int getfileattr(const char *filename, unsigned int *p_attr)
{
    return  _dos_getfileattr(filename, p_attr);
}
static inline int file_access(const char *filename, int flags)
{
    return access(filename, flags);
}
static inline int file_copytime(int desc_handle, int src_handle)
{
  int ret;
  struct ftime file_time;
  if ((ret = getftime(src_handle, &file_time)) == 0)
    ret = setftime(desc_handle, &file_time);
  return ret;
}
/* Disk free */
static inline void setdrive(unsigned int drive, unsigned int *p_drives)
{
    _dos_setdrive(drive, p_drives);
}
static inline void getdrive(unsigned int *p_drive)
{
    _dos_getdrive(p_drive);
}
typedef struct dfree diskfree_t;
#define DISKFREE_T_AVAIL(d) d.df_avail
#define DISKFREE_T_TOTAL(d) d.df_total
#define DISKFREE_T_BSEC(d) d.df_bsec
#define DISKFREE_T_SCLUS(d) d.df_sclus
#endif


#endif
