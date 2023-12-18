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
#include <stdlib.h>
#include <sys/types.h>

/* Version number string */
#define VERSION "1.2.9"

#define DELIMS ", \n"

/* PID of `init` */
#define INIT_PID 1
/* PID of `kthreadd` */
#define KTHREADD_PID 2

/* Maximum length of userland process names (incl. '\0') */
#define TASK_COMM_LEN 16
/* We will truncate the cmdline string */
#define CMD_MAX_LEN 32

/* Formatting widths for our columns */
#define PID_COL_WIDTH     10
#define PPID_COL_WIDTH    PID_COL_WIDTH
#define STATE_COL_WIDTH   5
#define NAME_COL_WIDTH    (TASK_COMM_LEN - 1)
#define COMMAND_COL_WIDTH CMD_MAX_LEN - 1

/* `/proc` filesystem */
#define PROC_FILESYSTEM "/proc"
/* PID status file */
#define STAT_FILE "stat"
/* PID command file */
#define CMD_FILE "cmdline"

/* Fixed block size */
#define BLOCK_SIZE 4096

/* Status file entry of zombie state */
#define STATE_ZOMBIE 'Z'

/* Enum for relevant ANSI SGR display modes */
enum ansi_display_mode_code {
    ANSI_DISPLAY_MODE_NORMAL = 0,
    ANSI_DISPLAY_MODE_BOLD   = 1,
};

/* Enum for the different standard ANSI SGR color options */
enum ansi_fg_color_code {
    ANSI_FG_NORMAL  = 0,
    ANSI_FG_BLACK   = 30,
    ANSI_FG_RED     = 31,
    ANSI_FG_GREEN   = 32,
    ANSI_FG_YELLOW  = 33,
    ANSI_FG_BLUE    = 34,
    ANSI_FG_MAGENTA = 35,
    ANSI_FG_CYAN    = 36,
    ANSI_FG_WHITE   = 37,
};

/* Struct for keeping track of the `zps` CLI options */
struct zps_settings {
    /* Boolean value for listing the running processes */
    bool show_proc_list;
    /* Boolean value for listing the defunct processes only */
    bool show_defunct_list;
    /* Boolean value for terminating defunct processes */
    bool terminate;
    /* Boolean value for showing prompt for the reaping option */
    bool prompt;
    /* Boolean value for silent mode */
    bool silent;
};

/* Struct for keeping track of the zombies */
struct zps_stats {
    /* Number of found defunct processes */
    size_t defunct_count;
    /* Number of signaled processes */
    size_t signaled_procs;
};

/* Struct for storing process stats */
struct proc_stats {
    pid_t pid;
    pid_t ppid;
    char state;
    char padding[7];
    char name[TASK_COMM_LEN];
    char cmd[CMD_MAX_LEN];
};

/* Struct to be used as a dynamically growing vector with immutable elements */
struct proc_vec {
    struct proc_stats *ptr;
    size_t sz;
    size_t max_sz;
};

/*!
 * Constructs an initial process vector with `max_sz` of `64`.
 *
 * The `proc_vec_free()` function should be called on this return value
 * in order to free the resources.
 *
 * @return Pointer to the allocated structure, `NULL` on error
 */
static inline struct proc_vec *proc_vec(void)
{
    struct proc_vec *proc_v = (struct proc_vec *)malloc(sizeof(*proc_v));
    if (!proc_v) {
        return NULL;
    }

    proc_v->max_sz = 64;
    proc_v->sz     = 0;
    proc_v->ptr =
        (struct proc_stats *)malloc(proc_v->max_sz * sizeof(*proc_v->ptr));
    if (!proc_v->ptr) {
        return NULL;
    }

    return proc_v;
}

/*!
 * Frees and invalidates the process vector pointed to by the `proc_v`.
 *
 * @param[out] proc_v Process vector to deallocate
 *
 * @return void
 */
static inline void proc_vec_free(struct proc_vec *proc_v)
{
    if (!proc_v) {
        return;
    }
    free(proc_v->ptr);
    free(proc_v);
}

/*!
 * Adds `entry` to the end of the `proc_v` vector.
 *
 * @param[out] proc_v Process vector to use
 * @param[in]  entry  Entry to add to the vector
 *
 * @return `false` on error, `true` otherwise
 */
static inline bool proc_vec_add(struct proc_vec *proc_v,
                                struct proc_stats entry)
{
    if (!proc_v) {
        return false;
    }

    if (proc_v->sz == proc_v->max_sz) {
        proc_v->max_sz *= 2;
        struct proc_stats *tmp = (struct proc_stats *)realloc(
            proc_v->ptr, proc_v->max_sz * sizeof(*proc_v->ptr));
        if (!tmp) {
            return false;
        } else {
            proc_v->ptr = tmp;
        }
    }

    proc_v->ptr[proc_v->sz++] = entry;
    return true;
}

/*!
 * Returns a pointer to the element at index `i` in `proc_v`.
 *
 * @param[out] proc_v Process vector to use
 * @param[in]  i      Vector index to access
 *
 * @return `NULL` on error, a pointer to the respective entry otherwise
 */
static inline const struct proc_stats *
proc_vec_at(const struct proc_vec *proc_v, size_t i)
{
    if (!proc_v || i >= proc_v->sz) {
        return NULL;
    }

    return &proc_v->ptr[i];
}

/*!
 * Returns the current number of elements stored in `proc_v`
 *
 * @param[in] proc_v Process vector to use
 *
 * @return Size of the vector pointed to by `proc_v`
 */
static inline size_t proc_vec_size(const struct proc_vec *proc_v)
{
    if (!proc_v) {
        return 0;
    }

    return proc_v->sz;
}

#endif // ZPS_H
