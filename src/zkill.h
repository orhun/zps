
#define VERSION "1.0"           /* Version */
#define _XOPEN_SOURCE 700       /* POSIX.1-2008 + XSI (SuSv4) */
#define _LARGEFILE64_SOURCE     /* Enable LFS */
#define _FILE_OFFSET_BITS 64    /* Support 64-bit file sizes */
#ifndef USE_FDS
#define USE_FDS 15              /* Maximum number of file descriptors to use */
#endif
#define PROC_FS "/proc"         /* '/proc' filesystem */
#define STATUS_FILE "/status"   /* Filename for status of PID */
#define BLOCK_SIZE 4096         /* Fixed block size for reading operations */
#define PROCESS_DRST 0          /* Return value of D/R/S/T process code */
#define PROCESS_ZOMBIE 1        /* Return value of Z (zombie/defunct) process code */
#define PROCESS_READ_ERROR -1   /* Return value of read error on '/proc' */
#define STATUS_ZOMBIE "State:\tZ (zombie)"