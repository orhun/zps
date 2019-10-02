/**!
 * zps, <TODO: Add description>
 * Copyright (C) 2019 by orhun <https://www.github.com/orhun>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define VERSION "1.0"         /* Version */
#define _XOPEN_SOURCE 700     /* POSIX.1-2008 + XSI (SuSv4) */
#define _LARGEFILE64_SOURCE   /* Enable LFS */
#define _FILE_OFFSET_BITS 64  /* Support 64-bit file sizes */
#ifndef USE_FDS
#define USE_FDS 15            /* Maximum number of file descriptors to use */
#endif
#define PROC_FS "/proc"       /* '/proc' filesystem */
#define STAT_FILE "/stat"     /* Filename for status of PID */
#define BLOCK_SIZE 4096       /* Fixed block size*/
#define PROCESS_DRST 0        /* Return value of D/R/S/T process code */
#define PROCESS_ZOMBIE 1      /* Return value of Z (zombie/defunct) process code */
#define PROCESS_READ_ERROR -1 /* Return value of read error on '/proc' */
#define STATE_ZOMBIE "Z"      /* Status file entry of zombie state */