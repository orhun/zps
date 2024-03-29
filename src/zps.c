/**!
 * zps, a small utility for listing and reaping zombie processes.
 * Copyright © 2019-2024 by Orhun Parmaksız <orhunparmaksiz@gmail.com>
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

#include <assert.h>
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

#ifndef PATH_MAX
#define PATH_MAX MAX_BUF_SIZE
#endif

/* Array used for lookup of common signals' abbreviations */
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

/*!
 * Helper to get the string abbreviation of signal constants
 *
 * @param[in] sig Signal number to get the string representation of
 *
 * @return String representing the signal constant (abbreviated),
 *         or NULL if no corresponding signal string was found
 */
static const char *sig_abbrev(int sig)
{
    if (sig < 0 || !((size_t)sig < sizeof(abbrevs))) {
        return NULL;
    }
    return abbrevs[sig];
}

/*!
 * Attempts to find the corresponding signal number to the
 * given signal constant's name
 *
 * @param[in] sig_str Signal name to convert
 *
 * @return -1 on error, the corresponding signal number otherwise
 */
static int sig_str_to_num(const char *sig_str)
{
    assert(sig_str);

    const char *const prefix = "SIG";
    const size_t prefix_len  = strlen(prefix);
    if (!strncasecmp(sig_str, prefix, prefix_len)) {
        sig_str += prefix_len;
    }

    for (size_t sig = 0; sig < sizeof(abbrevs) / sizeof(abbrevs[0]); ++sig) {
        if (!abbrevs[sig]) {
            continue;
        }
        if (!strcasecmp(sig_str, abbrevs[sig])) {
            return sig;
        }
    }
    return -1;
}

/*!
 * Tries to associate the user's signal input with a known
 * signal number
 *
 * @param[in] sig_str Signal characters to convert
 *
 * @return -1 on error, the corresponding signal number otherwise
 */
static int user_signal(const char *sig_str)
{
    if (!sig_str) {
        return -1;
    }
    if (!isdigit(*sig_str)) {
        return sig_str_to_num(sig_str);
    }

    int sig = -1;
    if (sscanf(sig_str, "%d", &sig) != 1 || sig < 0 || NSIG <= sig) {
        return -1;
    }
    return abbrevs[sig] ? sig : -1;
}

/*!
 * Checks if the standard I/O streams refer to a terminal and deduces
 * whether to use colored output.
 *
 * @param[out] settings Settings struct to update
 *
 * @return void
 */
static void check_interactive(struct zps_settings *settings)
{
    assert(settings);

    settings->color_allowed = settings->interactive =
        isatty(STDIN_FILENO) && isatty(STDOUT_FILENO) && isatty(STDERR_FILENO);
}

/*!
 * Write colored and formatted text to the specified stream.
 *
 * Resets the display attributes before returning
 *
 * @param[in]  color         ANSI SGR control sequence parameter n (color code)
 * @param[in]  color_allowed Boolean specifying whether we can print color
 * @param[out] stream        Pointer to the file to write to
 * @param[in]  format        Format string specifying the text to print
 * @param[in]  ...           Variable format string arguments
 *
 * @return void
 */
static void cfprintf(enum ansi_fg_color_code color, bool color_allowed,
                     FILE *stream, const char *format, ...)
{
    va_list vargs;

    assert(stream);
    assert(format);

    if (color_allowed) {
        fprintf(stream, "\x1b[%dm", color);
    }
    va_start(vargs, format);
    vfprintf(stream, format, vargs);
    va_end(vargs);
    if (color_allowed) {
        fprintf(stream, "\x1b[%dm", ANSI_FG_NORMAL);
    }
}

/*!
 * Write bold, colored and formatted text to the specified stream.
 *
 * Resets the display attributes before returning
 *
 * @param[in]  color         ANSI SGR control sequence parameter n (color code)
 * @param[in]  color_allowed Boolean specifying whether we can print color
 * @param[out] stream        Pointer to the file to write to
 * @param[in]  format        Format string specifying the text to print
 * @param[in]  ...           Variable format string arguments
 *
 * @return void
 */
static void cbfprintf(enum ansi_fg_color_code color, bool color_allowed,
                      FILE *stream, const char *format, ...)
{
    va_list vargs;

    assert(stream);
    assert(format);

    if (color_allowed) {
        fprintf(stream, "\x1b[%dm", ANSI_DISPLAY_MODE_BOLD);
        if (color) {
            fprintf(stream, "\x1b[%dm", color);
        }
    }
    va_start(vargs, format);
    vfprintf(stream, format, vargs);
    va_end(vargs);
    if (color_allowed) {
        fprintf(stream, "\x1b[%dm", ANSI_DISPLAY_MODE_NORMAL);
    }
}

/*!
 * Write bold, colored and formatted text to the specified stream.
 *
 * Encloses the bold and colored text.
 *
 * @param[in]  color         ANSI SGR control sequence parameter n (color code)
 * @param[in]  color_allowed Boolean specifying whether we can print color
 * @param[in]  before        String to put before the colored content
 * @param[in]  after         String to put after the colored content
 * @param[out] stream        Pointer to the file to write to
 * @param[in]  format        Format string specifying the text to print
 * @param[in]  ...           Variable format string arguments
 *
 * @return void
 */
static void cbfprintf_enclosed(enum ansi_fg_color_code color,
                               bool color_allowed, const char *before,
                               const char *after, FILE *stream,
                               const char *format, ...)
{
    va_list vargs;

    assert(before);
    assert(after);
    assert(stream);
    assert(format);

    fprintf(stream, "%s", before);
    if (color_allowed) {
        fprintf(stream, "\x1b[%dm", ANSI_DISPLAY_MODE_BOLD);
        if (color) {
            fprintf(stream, "\x1b[%dm", color);
        }
    }
    va_start(vargs, format);
    vfprintf(stream, format, vargs);
    va_end(vargs);
    if (color_allowed) {
        fprintf(stream, "\x1b[%dm", ANSI_DISPLAY_MODE_NORMAL);
    }
    fprintf(stream, "%s", after);
}

/*!
 * Print version and exit
 *
 * @param[in] status   Exit status to use
 * @param[in] settings Pointer to settings determining output coloring (bold)
 */
static void __attribute__((noreturn))
version_exit(int status, struct zps_settings *settings)
{
    assert(settings);

    cbfprintf(ANSI_FG_NORMAL, settings->interactive, status ? stderr : stdout,
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
            "  -v, --version        show version\n"
            "  -h, --help           show help\n"
            "  -a, --all            list all user-space processes\n"
            "  -r, --reap           reap zombie processes\n"
            "  -s, --signal   <sig> signal to be used on zombie parents\n"
            "  -p, --prompt         show prompt for selecting processes\n"
            "  -q, --quiet          reap in quiet mode\n"
            "  -n, --no-color       disable color output\n\n");
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
    assert(stream);

    const int fd = open("/dev/null", O_WRONLY);
    if (fd != -1) {
        dup2(fd, fileno(stream));
        close(fd);
    }
}

/*!
 * Function performing sanity checks on the configured settings
 *
 * This function will call `exit()` if the settings are erroneous.
 *
 * @param[in] settings Settings to check
 *
 * @return void
 */
static void settings_check(const struct zps_settings *settings)
{
    assert(settings);

    bool failed = false;
    if (settings->sig == -1) {
        cfprintf(ANSI_FG_RED, settings->color_allowed, stderr,
                 "Unknown signal\n");
        failed = true;
    }
    if (settings->sig && !settings->signal) {
        cfprintf(ANSI_FG_RED, settings->color_allowed, stderr,
                 "The -s option has to be used with either -r or -p\n");
        failed = true;
    }
    if (settings->quiet) {
        if (settings->show_all) {
            cfprintf(ANSI_FG_RED, settings->color_allowed, stderr,
                     "Incompatible options: -q, -a\n");
            failed = true;
        }
        if (settings->prompt) {
            cfprintf(ANSI_FG_RED, settings->color_allowed, stderr,
                     "Incompatible options: -q, -p\n");
            failed = true;
        }
    }
    if (failed) {
        help_exit(EXIT_FAILURE);
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
        { "version",       no_argument, NULL, 'v'},
        {    "help",       no_argument, NULL, 'h'},
        {     "all",       no_argument, NULL, 'a'},
        {    "reap",       no_argument, NULL, 'r'},
        {  "signal", required_argument, NULL, 's'},
        {  "prompt",       no_argument, NULL, 'p'},
        {   "quiet",       no_argument, NULL, 'q'},
        {"no-color",       no_argument, NULL, 'n'},
        {      NULL,                 0, NULL,   0},
    };

    assert(argv);
    assert(settings);

    for (int opt;
         (opt = getopt_long(argc, argv, "vhars:pqn", longopts, NULL)) != -1;) {
        switch (opt) {
        case 'v': /* Show version information. */
            version_exit(EXIT_SUCCESS, settings);
        case 'h': /* Show help message. */
            help_exit(EXIT_SUCCESS);
        case 'a': /* List all processes. */
            settings->show_all = true;
            break;
        case 'r': /* Actually reap the zombies. */
            settings->signal = true;
            break;
        case 's': /* User-specified zombie parent signal. */
            settings->sig = user_signal(optarg);
            break;
        case 'p': /* Show a prompt for interactive reaping. */
            settings->prompt = true;
            settings->signal = true;
            break;
        case 'q': /* Quiet reap mode. */
            settings->quiet  = true;
            settings->signal = true;
            break;
        case 'n': /* Disable color output. */
            settings->color_allowed = false;
            break;
        default:
            help_exit(EXIT_FAILURE);
        }
    }

    settings_check(settings);
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

    assert(buf);
    assert(bufsiz > 0);
    assert(format);

    va_start(vargs, format);
    int num_required = vsnprintf(path, sizeof(path), format, vargs);
    va_end(vargs);
    /* Check for errors or truncation */
    if (num_required < 0 || (size_t)num_required >= sizeof(path)) {
        return -1;
    }

    const int fd = open(path, O_RDONLY);
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
    assert(stat_buf);
    assert(proc_stats);

    /* Start with the PID field */
    if (sscanf(stat_buf, "%d", &proc_stats->pid) != 1) {
        return -1;
    }

    /* Pointer bounds for `comm` in the buffer */
    const char *begin = strchr(stat_buf, '(');
    if (!begin) {
        return -1;
    }
    char *const end = strrchr(begin, ')');
    if (!end || end[1] == '\0') {
        return -1;
    }
    ++begin;
    const char *const last_fields = end + 2;
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
    *end                     = '\0';
    const size_t comm_strlen = strnlen(begin, sizeof(proc_stats->name) - 1);
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

    assert(pid);
    assert(proc_stats);

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
    const ssize_t cmd_len = read_file(proc_stats->cmd, sizeof(proc_stats->cmd),
                                      "%s/%s/%s", PROC_FILESYSTEM, pid,
                                      CMD_FILE);
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
 * Send signal to the given PPID of the `proc_stats` entry.
 *
 * @param[in]  proc_stats Pointer to the entry to send the signal for
 * @param[in]  settings   Pointer to user-specified settings (signal?)
 * @param[out] stats      The `signaled_procs` field will be updated
 * @param[in]  verbose    Boolean specifying the behavior (print result)
 *
 * @return `-1` on error, otherwise `0` is returned
 */
static int handle_zombie(const struct proc_stats *proc_stats,
                         const struct zps_settings *settings,
                         struct zps_stats *stats, bool verbose)
{
    assert(proc_stats);
    assert(settings);
    assert(stats);

    const pid_t ppid = proc_stats->ppid;
    if (ppid <= 0 || ppid == INIT_PID || ppid == KTHREADD_PID) {
        return -1;
    }
    const int sig     = settings->sig ? settings->sig : SIGTERM;
    const int kill_rc = kill(ppid, sig);
    if (!kill_rc) {
        ++stats->signaled_procs;
        const char *const sigabbrev = sig_abbrev(sig);
        if (verbose) {
            cbfprintf_enclosed(ANSI_FG_RED, settings->color_allowed, "\n[", "]",
                               stdout, "SIG%s",
                               sigabbrev ? sigabbrev : "Unknown signal");
        }
    } else if (verbose) {
        cbfprintf_enclosed(ANSI_FG_RED, settings->color_allowed, "\n[", "]",
                           stdout, "Failed to signal");
    }
    return kill_rc;
}

/*!
 * Iterate over found zombies and list information while doing so.
 *
 * If the user is not to be prompted, this function immediately sends a signal.
 *
 * @param[in]  defunct_procs Pointer to the zombie process vector
 * @param[in]  settings      Pointer to user-specified settings (signal?)
 * @param[out] stats         The `signaled_procs` field will be updated
 *
 * @return void
 */
static void handle_found_zombies(const struct proc_vec *defunct_procs,
                                 const struct zps_settings *settings,
                                 struct zps_stats *stats)
{
    assert(defunct_procs);
    assert(settings);
    assert(stats);

    for (size_t i = 0, sz = proc_vec_size(defunct_procs); i < sz; ++i) {
        const struct proc_stats *const entry = proc_vec_at(defunct_procs, i);
        if (!settings->prompt) {
            handle_zombie(entry, settings, stats, true);
        } else {
            cbfprintf_enclosed(ANSI_FG_RED, settings->color_allowed, "\n[", "]",
                               stdout, "%zu", i + 1);
        }

        fprintf(stdout,
                "\n Name:    %s\n PID:     %d\n PPID:    %d\n State:   %c\n",
                entry->name, entry->pid, entry->ppid, entry->state);
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
    assert(defunct_procs);
    assert(settings);
    assert(stats);

    DIR *dir = opendir(PROC_FILESYSTEM);
    if (dir == NULL) {
        return;
    }

    for (;;) {
        struct dirent *d = readdir(dir);
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
        if (settings->show_all || proc_stats.state == STATE_ZOMBIE) {
            cfprintf(
                proc_stats.state == STATE_ZOMBIE ? ANSI_FG_RED : ANSI_FG_NORMAL,
                settings->color_allowed, stdout, "%-*d %-*d %-*c %*.*s %s\n",
                PID_COL_WIDTH, proc_stats.pid, PPID_COL_WIDTH, proc_stats.ppid,
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
 * @param[in]     settings      Pointer to user-specified settings (signal?)
 * @param[out]    stats         The `signaled_procs` field will be updated
 *
 * @return void
 */
static void prompt_user(const struct proc_vec *defunct_procs,
                        const struct zps_settings *settings,
                        struct zps_stats *stats)
{
    char index_prompt[MAX_BUF_SIZE] = {0};

    assert(defunct_procs);
    assert(settings);
    assert(stats);

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
            cfprintf(ANSI_FG_RED, settings->color_allowed, stderr,
                     "\nInvalid input: %s\n", token);
            continue;
        }
        --index;
        if (!(index < proc_vec_size(defunct_procs))) {
            cfprintf(ANSI_FG_RED, settings->color_allowed, stderr,
                     "\nIndex not in range: %zu\n", index + 1);
            continue;
        }

        const struct proc_stats *entry = proc_vec_at(defunct_procs, index);
        handle_zombie(entry, settings, stats, true);
        cbfprintf_enclosed(ANSI_FG_MAGENTA, settings->color_allowed, " -> ",
                           " ", stdout, "%s", entry->name);
        cbfprintf_enclosed(ANSI_FG_MAGENTA, settings->color_allowed,
                           "[PID (Z): ", ", ", stdout, "%d", entry->pid);
        cbfprintf_enclosed(ANSI_FG_RED, settings->color_allowed,
                           "PPID: ", "]\n", stdout, "%d", entry->ppid);
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
    assert(settings);
    assert(stats);

    struct proc_vec *const defunct_procs = proc_vec();
    if (!defunct_procs) {
        return -1;
    }

    /* Print column titles (header line). */
    cbfprintf(ANSI_FG_NORMAL, settings->color_allowed, stdout,
              "%-*s %-*s %-*s %*.*s %s\n", PID_COL_WIDTH, "PID", PPID_COL_WIDTH,
              "PPID", STATE_COL_WIDTH, "STATE", NAME_COL_WIDTH, NAME_COL_WIDTH,
              "NAME", "COMMAND");

    /* Main function logic */
    proc_iter(defunct_procs, settings, stats);
    if (settings->signal) {
        handle_found_zombies(defunct_procs, settings, stats);
    }
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
        .sig           = 0,
        .signal        = false,
        .show_all      = false,
        .prompt        = false,
        .quiet         = false,
        .interactive   = true,
        .color_allowed = true,
    };
    struct zps_stats stats = {
        .defunct_count  = 0,
        .signaled_procs = 0,
    };
    struct timespec start = {0}, end = {0};

    clock_gettime(CLOCK_REALTIME, &start);
    check_interactive(&settings);
    parse_args(argc, argv, &settings);
    if (settings.quiet) {
        silence(stdout);
        silence(stderr);
    }
    const int rc = check_procs(&settings, &stats);
    clock_gettime(CLOCK_REALTIME, &end);

    const double duration_ms = (end.tv_sec - start.tv_sec) * 1e3 +
                               (end.tv_nsec - start.tv_nsec) * 1e-6;
    if (stats.signaled_procs) {
        /* Show signal count and taken time. */
        fprintf(stdout,
                "\nParent(s) signaled: %zu/%zu\nElapsed time: %.2f ms\n",
                stats.signaled_procs, stats.defunct_count, duration_ms);
    }

    return rc ? EXIT_FAILURE : EXIT_SUCCESS;
}
