/**!
 * zps, a small utility for listing and reaping zombie processes.
 * Copyright © 2019-2023 by Orhun Parmaksız <orhunparmaksiz@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ZPS_H
#define ZPS_H

#include <stdbool.h>

/* Version number string */
#define VERSION "1.2.9"

/* Maximum number of file descriptors to use */
#define MAX_FD 15

/* '/proc' filesystem */
#define PROC_FILESYSTEM "/proc"
/* PID status file */
#define STAT_FILE "stat"
/* PID command file */
#define CMD_FILE "cmdline"

/* Fixed block size */
#define BLOCK_SIZE 4096

/* Status file entry of zombie state */
#define STATE_ZOMBIE "Z"
/* Default state of the process before parsing */
#define DEFAULT_STATE "~"

/* Regex for matching the values in 'stat' file */
#define STAT_REGEX "\\(([^)]*)\\)"
/* Maximum number of regex matches */
#define REG_MAX_MATCH 8
/* Character for replacing the spaces in regex match */
#define SPACE_REPLACEMENT '~'

/* Default color and style attributes */
#define CLR_DEFAULT "\x1b[0m"
/* Bold attribute */
#define CLR_BOLD "\x1b[1m"
/* Color red */
#define CLR_RED "\x1b[31m"

/* Struct for storing process stats */
struct proc_stats {
    unsigned int pid;
    unsigned int ppid;
    char name[BLOCK_SIZE / 64];
    char state[BLOCK_SIZE / 64];
    char cmd[BLOCK_SIZE];
    bool defunct;
};

#endif // ZPS_H
