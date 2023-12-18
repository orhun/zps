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

#define _GNU_SOURCE

#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "zps.h"

/*!
 * Write colored and formatted text to the specified stream.
 *
 * Resets the display attributes before returning
 *
 * @param[in]  color  ANSI SGR control sequence parameter n (color code)
 * @param[out] stream Pointer to the file to write to
 * @param[in]  format Format string specifying the text to print
 * @param[in]  ...    Variable format string arguments
 *
 * @return void
 */
static void cfprintf(enum ansi_fg_color_code color, FILE *stream,
                     const char *format, ...)
{
    va_list vargs;

    if (!stream || !format) {
        return;
    }

    fprintf(stream, "\x1b[%dm", color);
    va_start(vargs, format);
    vfprintf(stream, format, vargs);
    va_end(vargs);
    fprintf(stream, "\x1b[%dm", ANSI_FG_NORMAL);
}

/*!
 * Write bold, colored and formatted text to the specified stream.
 *
 * Resets the display attributes before returning
 *
 * @param[in]  color  ANSI SGR control sequence parameter n (color code)
 * @param[out] stream Pointer to the file to write to
 * @param[in]  format Format string specifying the text to print
 * @param[in]  ...    Variable format string arguments
 *
 * @return void
 */
static void cbfprintf(enum ansi_fg_color_code color, FILE *stream,
                      const char *format, ...)
{
    va_list vargs;

    if (!stream || !format) {
        return;
    }

    fprintf(stream, "\x1b[%dm", ANSI_DISPLAY_MODE_BOLD);
    if (color) {
        fprintf(stream, "\x1b[%dm", color);
    }
    va_start(vargs, format);
    vfprintf(stream, format, vargs);
    va_end(vargs);
    fprintf(stream, "\x1b[%dm", ANSI_DISPLAY_MODE_NORMAL);
}

/*!
 * Write bold, colored and formatted text to the specified stream.
 *
 * Encloses the bold and colored text.
 *
 * @param[in]  color  ANSI SGR control sequence parameter n (color code)
 * @param[in]  before String to put before the colored content
 * @param[in]  after  String to put after the colored content
 * @param[out] stream Pointer to the file to write to
 * @param[in]  format Format string specifying the text to print
 * @param[in]  ...    Variable format string arguments
 *
 * @return void
 */
static void cbfprintf_enclosed(enum ansi_fg_color_code color,
                               const char *before, const char *after,
                               FILE *stream, const char *format, ...)
{
    va_list vargs;

    if (!before || !after || !stream || !format) {
        return;
    }

    fprintf(stream, "%s\x1b[%dm", before, ANSI_DISPLAY_MODE_BOLD);
    if (color) {
        fprintf(stream, "\x1b[%dm", color);
    }
    va_start(vargs, format);
    vfprintf(stream, format, vargs);
    va_end(vargs);
    fprintf(stream, "\x1b[%dm%s", ANSI_DISPLAY_MODE_NORMAL, after);
}

/*!
 * Print version and exit
 *
 * @param[in] status Exit status to use
 */
static void __attribute__((noreturn)) version_exit(int status)
{
    cbfprintf(ANSI_FG_NORMAL, status ? stderr : stdout,
              "\n -hhhhdddddd/\n"
              " `++++++mMN+\n"
              "      :dMy.\n"
              "    -yMMh.\n"
              "  `oNNo:shy:`\n"
              " .dMm:```.+dNh`\n"
              " .github/orhun/zps v%s\n\n",
              VERSION);
    exit(status);
}

/*!
 * Print help and exit
 *
 * @param[in] status Exit status to use
 */
static void __attribute__((noreturn)) help_exit(int status)
{
    fprintf(status ? stderr : stdout,
            "\nUsage:\n"
            "  zps [options]\n\n"
            "Options:\n"
            "  -r, --reap      reap zombie processes\n"
            "  -x, --lreap     list and reap zombie processes\n"
            "  -l, --list      list zombie processes only\n"
            "  -p, --prompt    show prompt for selecting processes\n"
            "  -s, --silent    run in silent mode\n"
            "  -v, --version   show version\n"
            "  -h, --help      show help\n\n");
    exit(status);
}

/*!
 * Redirect `stream` to file `"/dev/null"`
 *
 * @param[out] stream I/O stream to silence
 *
 * @return void
 */
static void silence(FILE *stream)
{
    if (!stream) {
        return;
    }
    int fd = open("/dev/null", O_WRONLY);
    if (fd != -1) {
        dup2(fd, fileno(stream));
        close(fd);
    }
}

/*!
 * Parse command line arguments.
 *
 * @param[in]     argc     Argument count
 * @param[in,out] argv     Argument vector (which may be permuted (GNU))
 * @param[out]    settings Pointer to struct for user-specified settings
 *
 * @return void
 */
static void parse_args(int argc, char *argv[], struct zps_settings *settings)
{
    /* Long options for command line arguments  */
    static const struct option longopts[] = {
        {"version", no_argument, NULL, 'v'},
        {   "help", no_argument, NULL, 'h'},
        {   "reap", no_argument, NULL, 'r'},
        {  "lreap", no_argument, NULL, 'x'},
        {   "list", no_argument, NULL, 'l'},
        { "prompt", no_argument, NULL, 'p'},
        { "silent", no_argument, NULL, 's'},
        {     NULL,           0, NULL,   0},
    };

    if (!argv || !settings) {
        return;
    }

    for (int opt;
         (opt = getopt_long(argc, argv, ":vhrxlps", longopts, NULL)) != -1;) {
        switch (opt) {
        case 'v': /* Show version information. */
            version_exit(EXIT_SUCCESS);
        case 'h': /* Show help message. */
            help_exit(EXIT_SUCCESS);
        case 'l': /* List defunct processes only. */
            settings->show_defunct_list = true;
            settings->terminate         = true;
            // fall through
        case 'r': /* Don't list running processes. */
            settings->show_proc_list = false;
            // fall through
        case 'x': /* Reap defunct processes. */
            settings->terminate = !settings->terminate;
            break;
        case 'p': /* Show prompt for the reaping option. */
            settings->prompt    = true;
            settings->terminate = true;
            break;
        case 's': /* Silent mode. */
            settings->silent = true;
            break;
        case ':': /* Missing argument. */
            cfprintf(ANSI_FG_RED, stderr, "Option requires an argument.\n");
            exit(EXIT_FAILURE);
        case '?': /* Unknown option. */
            cfprintf(ANSI_FG_RED, stderr, "Unknown option.\n");
            exit(EXIT_FAILURE);
        }
    }
}

/*!
 * Read the given file and return its content.
 *
 * @param[in,out] buf    Buffer to read bytes from the file into
 * @param[in]     bufsiz Size of allocated `buf`
 * @param[in]     format Format string specifying the file path
 * @param[in]     ...    Variable format string arguments
 *
 * @return number of bytes successfully read (max: `bufsiz - 1`),
           `-1` on error
 */
static ssize_t read_file(char *buf, size_t bufsiz, const char *format, ...)
{
    va_list vargs;
    char path[PATH_MAX] = {0};

    if (!buf || !format) {
        return -1;
    }

    va_start(vargs, format);
    vsnprintf(path, sizeof(path), format, vargs);
    va_end(vargs);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        return -1;
    }
    memset(buf, '\0', bufsiz);
    ssize_t read_rc = read(fd, buf, bufsiz - 1);
    close(fd);

    return read_rc;
}

/*!
 * Parse the content of `"/proc/<pid>/stat"` into `proc_stats`.
 *
 * @param[in,out] stat_buf   Null-terminated buffer containing the contents of
                             `"/proc/<pid>/stat"` from offset `0`; will be
                             modified during execution
 * @param[out]    proc_stats Pointer to write the process information to
 *
 * @return `-1` on error, otherwise `0` is returned
 */
static int parse_stat_content(char *stat_buf, struct proc_stats *proc_stats)
{
    if (!stat_buf || !proc_stats) {
        return -1;
    }

    /* Start with the PID field */
    if (sscanf(stat_buf, "%d", &proc_stats->pid) != 1) {
        return -1;
    }

    /* Pointer bounds for `comm` in the buffer */
    char *begin = strchr(stat_buf, '(');
    if (!begin) {
        return -1;
    }
    char *end = strrchr(begin, ')');
    if (!end || end[1] == '\0') {
        return -1;
    }
    *end = '\0';
    ++begin;
    char *last_fields = end + 2;
    /*
        begin:
            %d (...) %c %d
                ^
        end:
            %d (...) %c %d
                   ^
        last_fields:
            %d (...) %c %d
                     ^
    */
    /* Extract the last fields first */
    if (sscanf(last_fields, "%c %d", &proc_stats->state, &proc_stats->ppid) !=
        2) {
        return -1;
    }

    /* Limit `comm_strlen` */
    size_t comm_strlen = strnlen(begin, sizeof(proc_stats->name) - 1);
    /* Extract the process name (limited by `comm_strlen`) */
    memcpy(proc_stats->name, begin, comm_strlen);
    /* Make sure string ends here */
    proc_stats->name[comm_strlen] = '\0';

    return 0;
}

/*!
 * Parse and return the stats for a given PID.
 *
 * @param[in]  pid        String containing the PID
 * @param[out] proc_stats Pointer to the struct to write to
 *
 * @return `-1` on error, `0` otherwise
 */
static int get_proc_stats(const char *pid, struct proc_stats *proc_stats)
{
    char stat_buf[MAX_BUF_SIZE] = {0};

    if (!pid || !proc_stats) {
        return -1;
    }

    /* Read the `"/proc/<pid>/stat"` file. */
    if (read_file(stat_buf, sizeof(stat_buf), "%s/%s/%s", PROC_FILESYSTEM, pid,
                  STAT_FILE) == -1) {
        return -1;
    }
    if (parse_stat_content(stat_buf, proc_stats)) {
        return -1;
    }
    /* We do not want kernel processes/threads */
    if (proc_stats->ppid == KTHREADD_PID || proc_stats->pid == KTHREADD_PID) {
        return -1;
    }

    /* Read the `"/proc/<pid>/cmdline"` file */
    ssize_t cmd_len = read_file(proc_stats->cmd, sizeof(proc_stats->cmd),
                                "%s/%s/%s", PROC_FILESYSTEM, pid, CMD_FILE);
    if (cmd_len == -1) {
        return -1;
    }

    /* Replace any null bytes with spaces to also print further arguments */
    for (size_t i = 0; i < (size_t)cmd_len; ++i) {
        if (!proc_stats->cmd[i]) {
            proc_stats->cmd[i] = ' ';
        }
    }

    return 0;
}

/*!
 * Helper to get the string abbreviation of signal constants
 *
 * @param[in] sig Signal number to get the string representation of
 *
 * @return String representing the signal constant (abbreviated)
 */
const char *sig_abbrev(int sig)
{
    static const char *const abbrevs[NSIG] = {
        [SIGHUP] = "HUP",       [SIGINT] = "INT",     [SIGQUIT] = "QUIT",
        [SIGILL] = "ILL",       [SIGTRAP] = "TRAP",   [SIGABRT] = "ABRT",
        [SIGFPE] = "FPE",       [SIGKILL] = "KILL",   [SIGBUS] = "BUS",
        [SIGSYS] = "SYS",       [SIGSEGV] = "SEGV",   [SIGPIPE] = "PIPE",
        [SIGALRM] = "ALRM",     [SIGTERM] = "TERM",   [SIGURG] = "URG",
        [SIGSTOP] = "STOP",     [SIGTSTP] = "TSTP",   [SIGCONT] = "CONT",
        [SIGCHLD] = "CHLD",     [SIGTTIN] = "TTIN",   [SIGTTOU] = "TTOU",
        [SIGPOLL] = "POLL",     [SIGXCPU] = "XCPU",   [SIGXFSZ] = "XFSZ",
        [SIGVTALRM] = "VTALRM", [SIGPROF] = "PROF",   [SIGUSR1] = "USR1",
        [SIGUSR2] = "USR2",     [SIGWINCH] = "WINCH",
    };
    if (sig < 0 || !((size_t)sig < sizeof(abbrevs))) {
        return NULL;
    }
    return abbrevs[sig];
}

/*!
 * Send signal to the given PPID of the `proc_stats` entry.
 *
 * @param[in]  proc_stats Pointer to the entry to send the signal for
 * @param[in]  settings   Pointer to user-specified settings (terminate?)
 * @param[out] stats      The `signaled_procs` field will be updated
 * @param[in]  verbose    Boolean specifying the behavior (print result)
 *
 * @return `-1` on error, otherwise `0` is returned
 */
static int handle_zombie(const struct proc_stats *proc_stats,
                         const struct zps_settings *settings,
                         struct zps_stats *stats, bool verbose)
{
    if (!proc_stats || !settings || !stats) {
        return -1;
    }
    const pid_t ppid = proc_stats->ppid;
    if (ppid <= 0 || ppid == INIT_PID || ppid == KTHREADD_PID) {
        return -1;
    }
    const int sig     = settings->terminate ? SIGTERM : SIGCHLD;
    const int kill_rc = kill(ppid, sig);
    if (!kill_rc) {
        ++stats->signaled_procs;
        const char *sigabbrev = sig_abbrev(sig);
        if (verbose) {
            cbfprintf_enclosed(ANSI_FG_RED, "\n[", "]", stdout, "SIG%s",
                               sigabbrev ? sigabbrev : "Unknown signal");
        }
    } else if (verbose) {
        cbfprintf_enclosed(ANSI_FG_RED, "\n[", "]", stdout, "Failed to signal");
    }
    return kill_rc;
}

/*!
 * Iterate over found zombies and list information while doing so.
 *
 * If the user is not to be prompted, this function immediately sends a signal.
 *
 * @param[in]  defunct_procs Pointer to the zombie process vector
 * @param[in]  settings      Pointer to user-specified settings (terminate?)
 * @param[out] stats         The `signaled_procs` field will be updated
 *
 * @return void
 */
static void handle_found_zombies(const struct proc_vec *defunct_procs,
                                 const struct zps_settings *settings,
                                 struct zps_stats *stats)
{
    if (!defunct_procs || !settings || !stats) {
        return;
    }
    for (size_t i = 0, sz = proc_vec_size(defunct_procs); i < sz; ++i) {
        const struct proc_stats *entry = proc_vec_at(defunct_procs, i);
        if (!settings->prompt) {
            handle_zombie(entry, settings, stats, true);
        } else {
            cbfprintf_enclosed(ANSI_FG_RED, "\n[", "]", stdout, "%zu", i + 1);
        }

        fprintf(stdout,
                "\n Name:    %s\n PID:     %d\n PPID:    %d\n State:   %c\n",
                entry->name, entry->pid, entry->ppid, entry->state);
        if (entry->cmd[0]) {
            fprintf(stdout, " Command: %s\n", entry->cmd);
        }
    }
}

/*!
 * Iterate through `"/proc"` and save found zombie entries.
 *
 * @param[out] defunct_procs Pointer to the zombie process vector to fill
 * @param[in]  settings      Pointer to user-specified settings (list?)
 * @param[out] stats         The `defunct_count` field will be updated
 *
 * @return void
 */
static void proc_iter(struct proc_vec *defunct_procs,
                      const struct zps_settings *settings,
                      struct zps_stats *stats)
{
    if (!defunct_procs || !settings || !stats) {
        return;
    }
    DIR *dir = opendir(PROC_FILESYSTEM);
    if (dir == NULL) {
        return;
    }

    for (;;) {
        struct dirent *d;
        d = readdir(dir);
        if (d == NULL) {
            break;
        }
        if (!(d->d_type == DT_DIR && isdigit(d->d_name[0]))) {
            continue;
        }

        struct proc_stats proc_stats = {0};
        /*  Get the process stats from the path. */
        if (get_proc_stats(d->d_name, &proc_stats)) {
            continue;
        } else if (proc_stats.state == STATE_ZOMBIE) {
            ++stats->defunct_count;
            /* Add process to the array of defunct processes (could fail) */
            proc_vec_add(defunct_procs, proc_stats);
        }
        /* Print the process's stats. */
        if (settings->show_proc_list ||
            (proc_stats.state == STATE_ZOMBIE && settings->show_defunct_list)) {
            cfprintf(proc_stats.state == STATE_ZOMBIE ? ANSI_FG_RED :
                                                        ANSI_FG_NORMAL,
                     stdout, "%-*d %-*d %-*c %*.*s %s\n", PID_COL_WIDTH,
                     proc_stats.pid, PPID_COL_WIDTH, proc_stats.ppid,
                     STATE_COL_WIDTH, proc_stats.state, NAME_COL_WIDTH,
                     NAME_COL_WIDTH, proc_stats.name, proc_stats.cmd);
        }
    }

    closedir(dir);
}

/*!
 * Request user input to explicitly signal processes.
 *
 * @param[in,out] defunct_procs Pointer to the zombie process vector
 * @param[in]     settings      Pointer to user-specified settings (terminate?)
 * @param[out]    stats         The `signaled_procs` field will be updated
 *
 * @return void
 */
static void prompt_user(const struct proc_vec *defunct_procs,
                        const struct zps_settings *settings,
                        struct zps_stats *stats)
{
    char index_prompt[MAX_BUF_SIZE] = {0};

    if (!defunct_procs || !settings || !stats) {
        return;
    }

    /* Print user input message and ask for input. */
    fprintf(stdout, "\nEnter process index(es) to proceed: ");
    fflush(stdout);
    if (!fgets(index_prompt, sizeof(index_prompt), stdin)) {
        return;
    }

    /* Split the given input. */
    char *saveptr = NULL;
    for (char *token = strtok_r(index_prompt, DELIMS, &saveptr); token;
         token       = strtok_r(NULL, DELIMS, &saveptr)) {
        size_t index = 0;
        if (token[0] == '-' || sscanf(token, "%zu", &index) != 1) {
            cfprintf(ANSI_FG_RED, stderr, "\nInvalid input: %s\n", token);
            continue;
        }
        --index;
        if (!(index < proc_vec_size(defunct_procs))) {
            cfprintf(ANSI_FG_RED, stderr, "\nIndex not in range: %zu\n",
                     index + 1);
            continue;
        }

        const struct proc_stats *entry = proc_vec_at(defunct_procs, index);
        handle_zombie(entry, settings, stats, true);
        cbfprintf_enclosed(ANSI_FG_MAGENTA, " -> ", " ", stdout, "%s",
                           entry->name);
        cbfprintf_enclosed(ANSI_FG_MAGENTA, "[pid (Z): ", ", ", stdout, "%d",
                           entry->pid);
        cbfprintf_enclosed(ANSI_FG_RED, "ppid: ", "]\n", stdout, "%d",
                           entry->ppid);
    }
}

/*!
 * Check running process's states using the `"/proc"` filesystem.
 *
 * @param[in]  settings Pointer to user-specified settings
 * @param[out] stats    Pointer to statistics to update for the zombies found
 *
 * @return -1 on error, 0 otherwise
 */
static int check_procs(struct zps_settings *settings, struct zps_stats *stats)
{
    if (!settings || !stats) {
        return -1;
    }
    struct proc_vec *defunct_procs = proc_vec();
    if (!defunct_procs) {
        return -1;
    }

    /* Print column titles (header line). */
    if (settings->show_proc_list || settings->show_defunct_list) {
        cbfprintf(ANSI_FG_NORMAL, stdout, "%-*s %-*s %-*s %*.*s %s\n",
                  PID_COL_WIDTH, "PID", PPID_COL_WIDTH, "PPID", STATE_COL_WIDTH,
                  "STATE", NAME_COL_WIDTH, NAME_COL_WIDTH, "NAME", "COMMAND");
    }

    /* Main function logic */
    proc_iter(defunct_procs, settings, stats);
    handle_found_zombies(defunct_procs, settings, stats);
    if (settings->prompt && proc_vec_size(defunct_procs)) {
        prompt_user(defunct_procs, settings, stats);
    }

    proc_vec_free(defunct_procs);

    return 0;
}

/*!
 * Entry point
 */
int main(int argc, char *argv[])
{
    struct zps_settings settings = {
        .terminate         = false,
        .show_proc_list    = true,
        .show_defunct_list = false,
        .prompt            = false,
        .silent            = false,
    };
    struct zps_stats stats = {
        .defunct_count  = 0,
        .signaled_procs = 0,
    };
    struct timespec start = {0}, end = {0};

    clock_gettime(CLOCK_REALTIME, &start);
    parse_args(argc, argv, &settings);
    if (settings.silent) {
        silence(stdout);
        silence(stderr);
    }
    int rc = check_procs(&settings, &stats);
    clock_gettime(CLOCK_REALTIME, &end);

    const double duration_ms = (end.tv_sec - start.tv_sec) * 1e3 +
                               (end.tv_nsec - start.tv_nsec) * 1e-6;
    if (stats.signaled_procs) {
        /* Show signal count and taken time. */
        fprintf(stdout,
                "\nparent(s) signaled: %zu/%zu\nelapsed time: %.2f ms\n",
                stats.signaled_procs, stats.defunct_count, duration_ms);
    }

    return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}
