
#define VERSION "1.0"           /* Version */
#define _XOPEN_SOURCE 700       /* POSIX.1-2008 + XSI (SuSv4) */
#define _LARGEFILE64_SOURCE     /* Enable LFS */
#define _FILE_OFFSET_BITS 64    /* Support 64-bit file sizes */
#define _GNU_SOURCE             /* Include GNU extensions. */
#ifndef USE_FDS
#define USE_FDS 15              /* Maximum number of file descriptors to use */
#endif
#define PROC_FS "/proc"         /* '/proc' filesystem */