/**!
 * zps, a small utility for listing and reaping zombie processes.
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

#define VERSION "1.2.1"            /* Version */
#define _XOPEN_SOURCE 700          /* POSIX.1-2008 + XSI (SuSv4) */
#define _LARGEFILE64_SOURCE        /* Enable LFS */
#define _FILE_OFFSET_BITS 64       /* Support 64-bit file sizes */
#define MAX_FD 15                  /* Maximum number of file descriptors to use */
#define PROC_FILESYSTEM "/proc"    /* '/proc' filesystem */
#define STAT_FILE "stat"           /* PID status file */
#define CMD_FILE "cmdline"         /* PID command file */
#define BLOCK_SIZE 4096            /* Fixed block size*/
#define STATE_ZOMBIE "Z"           /* Status file entry of zombie state */
#define DEFAULT_STATE "~"          /* Default state of the process before parsing */
#define STAT_REGEX "\\(([^)]*)\\)" /* Regex for matching the values in 'stat' file */
#define REG_MAX_MATCH 8            /* Maximum number of regex matches */
#define SPACE_REPLACEMENT '~'      /* Character for replacing the spaces in regex match */
#define CLR_DEFAULT "\x1b[0m"      /* Default color and style attributes */
#define CLR_BOLD "\x1b[1m"         /* Bold attribute */
#define CLR_RED "\x1b[31m"         /* Color red */
