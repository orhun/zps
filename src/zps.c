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

/* POSIX.1-2008 + XSI (SUSv4) */
#define _XOPEN_SOURCE 700

#include <ctype.h>
#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
#include <limits.h>
#include <regex.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "zps.h"

static struct zps_settings settings = {
    .max_fd = MAX_FD,
    .terminate = false,
    .show_proc_list = true,
    .show_defunct_list = false,
    .prompt = false
};

static struct zps_stats stats = {
    .defunct_count = 0,
    .signaled_procs = 0
};

/* Array of defunct process's stats */
static struct proc_stats defunct_procs[BLOCK_SIZE / 4];

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
static void cfprintf(enum ansi_fg_color_code color,
                     FILE *stream, const char *format, ...)
{
    /* List of information about variable arguments */
    va_list vargs;
    /* Set the color. */
    fprintf(stream, "\x1b[%dm", color);
    /* Format and print the data. */
    va_start(vargs, format);
    vfprintf(stream, format, vargs);
    va_end(vargs);
    /* Reset display. */
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
static void cbfprintf(enum ansi_fg_color_code color,
                      FILE *stream, const char *format, ...)
{
    /* List of information about variable arguments */
    va_list vargs;
    /* Set bold display mode. */
    fprintf(stream, "\x1b[%dm", ANSI_DISPLAY_MODE_BOLD);
    if (color) {
        /* Set the color. */
        fprintf(stream, "\x1b[%dm", color);
    }
    /* Format and print the data. */
    va_start(vargs, format);
    vfprintf(stream, format, vargs);
    va_end(vargs);
    /* Reset display. */
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
    /* List of information about variable arguments */
    va_list vargs;
    /* Set the color. */
    fprintf(stream, "%s\x1b[%dm", before, ANSI_DISPLAY_MODE_BOLD);
    if (color) {
        /* Set the color. */
        fprintf(stream, "\x1b[%dm", color);
    }
    /* Format and print the data. */
    va_start(vargs, format);
    vfprintf(stream, format, vargs);
    va_end(vargs);
    /* Reset display. */
    fprintf(stream, "\x1b[%dm%s", ANSI_DISPLAY_MODE_NORMAL, after);
}

/*!
 * Print version and exit
 *
 * @param[in] status (exit status to use)
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
             " .github/orhun/zps v%s\n\n", VERSION);
    exit(status);
}

/*!
 * Print help and exit
 *
 * @param[in] status (exit status to use)
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
            "  -f, --fd <num>  set maximum file descriptors (default: 15)\n"
            "  -s, --silent    run in silent mode\n"
            "  -v, --version   show version\n"
            "  -h, --help      show help\n\n");
    exit(status);
}

/*!
 * Redirect `stderr` to file `"/dev/null"`
 *
 * @return void
 */
static void silence_stderr(void)
{
    int fd = open("/dev/null", O_WRONLY);
    if (fd != -1) {
        dup2(fd, STDERR_FILENO);
        close(fd);
    }
}

/*!
 * Parse command line arguments.
 *
 * @param argc     (argument count)
 * @param argv     (argument vector)
 * @param settings (pointer to settings struct for user-specified settings)
 *
 * @return void
 */
static void parse_args(int argc, char *argv[], struct zps_settings *settings)
{
    /* Long options for command line arguments  */
    static const struct option longopts[] = {
        {"version", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {"reap", no_argument, NULL, 'r'},
        {"lreap", no_argument, NULL, 'x'},
        {"list", no_argument, NULL, 'l'},
        {"prompt", no_argument, NULL, 'p'},
        {"silent", no_argument, NULL, 's'},
        {"fd", required_argument, NULL, 'f'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, ":vhrxlpsf:",
            longopts, NULL)) != -1) {
        switch (opt) {
        case 'v': /* Show version information. */
            version_exit(EXIT_SUCCESS);
        case 'h': /* Show help message. */
            help_exit(EXIT_SUCCESS);
        case 'l': /* List defunct processes only. */
            settings->show_defunct_list = true;
            settings->terminate = true;
            // fall through
        case 'r': /* Don't list running processes. */
            settings->show_proc_list = false;
            // fall through
        case 'x': /* Reap defunct processes. */
            settings->terminate = !settings->terminate;
            break;
        case 'p': /* Show prompt for the reaping option. */
            settings->prompt = true;
            settings->terminate = true;
            break;
        case 's': /* Silent mode. */
            silence_stderr();
            break;
        case 'f': /* Set file descriptor count. */
            settings->max_fd = atoi(optarg);
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
 * @return `buf` if bytes were successfully read, `NULL` otherwise
 */
static char *read_file(char *buf, size_t bufsiz, char *format, ...)
{
    /* List of information about variable arguments */
    va_list vargs;
    /* Name of file to read */
    char path[PATH_MAX] = {0};
    /* Format the file name with the arguments. */
    va_start(vargs, format);
    vsnprintf(path, sizeof(path), format, vargs);
    va_end(vargs);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        return NULL;
    }

    memset(buf, '\0', bufsiz);
    ssize_t read_rc = read(fd, buf, bufsiz - 1);

    close(fd);
    return read_rc > 0 ? buf : NULL;
}

/*!
 * Format the content of `"/proc/<pid>/stat"` using regex.
 *
 * @param stat_content
 *
 * @return -1 on error, otherwise 0 is returned
 */
static int format_stat_content(char *stat_content)
{
    /* Regex match */
    char match[BLOCK_SIZE / 4] = {0};
    /* Regex struct */
    regex_t regex;
    /* Regex match struct that contains start and end offsets */
    regmatch_t reg_match[REG_MAX_MATCH];
    /**
     * Some of the processes contain spaces in their name so
     * parsing stats with sscanf fails on those cases.
     * Regex used for replacing the spaces in process names.
     * Compile the regex and check error.
     */
    if (regcomp(&regex, STAT_REGEX, REG_EXTENDED) != 0) {
        return -1;
    }
    /**
     * Match the content with regex pattern using the following arguments:
     * regex:         Regex struct to execute.
     * stat_content:  Content of `"/proc/<pid>/stat"`.
     * REG_MAX_MATCH: Maximum number of regex matches.
     * reg_match:     Regex matches.
     * REG_NOTEOL:    Flag for not matching "match-end-of-line" operator.
     */
    if (regexec(&regex, stat_content, REG_MAX_MATCH, reg_match, REG_NOTEOL)
            != REG_NOMATCH) {
        /* Beginning offset of first match */
        char *offset_begin = stat_content + reg_match[1].rm_so;
        /* Ending offset of first match */
        char *offset_end = stat_content + reg_match[1].rm_eo;
        /* Duplicate of content for changing */
        char *content_dup = strdup(stat_content);
        /* Offset of first space in content */
        regoff_t offset_space = reg_match[1].rm_so - 1;
        /* Length of the match */
        int match_length = strcspn(offset_begin, " ");
        /* Check the match length for parsing or replacing spaces. */
        if (match_length <= (reg_match[1].rm_eo - reg_match[1].rm_so)) {
            match_length = 0;
            /* Loop through the matches using offsets. */
            while (offset_begin < offset_end) {
                /* Change the space character if the space is inside the parentheses. */
                if (offset_begin != offset_end && match_length != 0) {
                    content_dup[offset_space] = SPACE_REPLACEMENT;
                }
                /* Set the match length using the space span. */
                match_length = strcspn(offset_begin, " ");
                /* Set the current match. */
                sprintf(match, "%.*s", match_length, offset_begin);
                /* Next space is always the character next to the current match. */
                offset_space += match_length + 1;
                /* Set the offsets for the next match. */
                offset_begin += match_length;
                offset_begin += strspn(offset_begin, " ");
            }
            /* Update the original file content and deallocate the memory. */
            strcpy(stat_content, content_dup);
        }
        free(content_dup);
    }
    regfree(&regex);
    return 0;
}

/*!
 * Parse and return the stats from process path.
 *
 * @param proc_path (process path in `"/proc"`)
 *
 * @return proc_stats (process stats)
 */
static struct proc_stats get_proc_stats(const char *proc_path)
{
    char buf[BLOCK_SIZE] = {0};
    char *stat_content, *cmd_content;
    /* Create a structure for storing parsed process's stats. */
    struct proc_stats proc_stats = {.state = DEFAULT_STATE};
    /* Read the process status file. */

    stat_content = read_file(buf, sizeof(buf), "%s/%s", proc_path, STAT_FILE);
    /* Check file read error and fix file content. */
    if (!stat_content || format_stat_content(stat_content)) {
        return proc_stats;
    }
    /* Parse the `"/stat"` file into process status struct. */
    sscanf(stat_content, "%d %64s %c %d", &proc_stats.pid,
           proc_stats.name, &proc_stats.state, &proc_stats.ppid);
    /* Remove the parentheses around the process name. */
    proc_stats.name[strnlen(proc_stats.name, sizeof(proc_stats.name)) - 1] = '\0';
    memmove(proc_stats.name, proc_stats.name + 1,
            strnlen(proc_stats.name, sizeof(proc_stats.name)));
    /* Read the `"cmdline"` file and check error. */
    memset(buf, '\0', sizeof(buf));
    cmd_content = read_file(buf, sizeof(buf), "%s/%s", proc_path, CMD_FILE);
    if (!cmd_content) {
        return proc_stats;
    }
    /* Update the command variable in the process status struct. */
    strcpy(proc_stats.cmd, cmd_content);
    /* Return the process stats. */
    return proc_stats;
}

/*!
 * Event for receiving tree entry from `"/proc"`.
 *
 * @param fpath  (path name of the entry)
 * @param sb     (file status structure for fpath)
 * @param tflag  (type flag of the entry)
 * @param ftwbuf (structure that contains entry base and level)
 *
 * @return EXIT_status
 */
static int proc_entry_recv(const char *fpath, const struct stat *sb,
                           int tflag, struct FTW *ftwbuf)
{
    (void)sb;

    /**
     * Check for depth of the fpath (1), type of the entry (directory),
     * base of the fpath (numeric value) to filter entries except PID.
     */
    if (ftwbuf->level != 1 || tflag != FTW_D ||
            !isdigit(*(fpath + ftwbuf->base))) {
        return EXIT_SUCCESS;
    }
    /* Get the process stats from the path. */
    struct proc_stats proc_stats = get_proc_stats(fpath);
    /* Check for process's file parse error. */
    if (proc_stats.state == DEFAULT_STATE) {
        cfprintf(ANSI_FG_RED, stderr, "Failed to parse \"%s\".\n", fpath);
        return EXIT_FAILURE;
    } else if (proc_stats.state == STATE_ZOMBIE) { /* Check for zombie. */
        /* Add process stats to the array of defunct process stats. */
        defunct_procs[stats.defunct_count++] = proc_stats;
    }
    /* Print the process's stats. */
    if (settings.show_proc_list ||
            (proc_stats.state == STATE_ZOMBIE && settings.show_defunct_list)) {
        cfprintf(proc_stats.state == STATE_ZOMBIE ? ANSI_FG_RED
                                                  : ANSI_FG_NORMAL,
                 stderr, "%-7d\t%-7d\t%-c\t%16.16s %.64s\n",
                 proc_stats.pid, proc_stats.ppid, proc_stats.state,
                 proc_stats.name, proc_stats.cmd);
    }
    return EXIT_SUCCESS;
}

/*!
 * Send termination signal to the parent of the process.
 *
 * @param      ppid
 * @param[out] stats (function updates the `terminated` field accordingly)
 *
 * @return -1 on error, otherwise 0 is returned
 */
static int kill_proc_by_ppid(pid_t ppid, struct zps_stats *stats)
{
    int kill_rc = kill(ppid, SIGTERM);
    if (!kill_rc) {
        stats->signaled_procs++;
    }
    return kill_rc;
}

/*!
 * Request user input if prompt argument is provided.
 *
 * @param stats (pointer to zombie stats)
 *
 * @return void
 */
static void prompt_to_kill(struct zps_stats *stats)
{
    /* Input value for the prompt option */
    char index_prompt[BLOCK_SIZE] = {0};
    /* Print user input message and ask for input. */
    fprintf(stderr, "\nEnter process index(es) to proceed: ");
    if (!fgets(index_prompt, sizeof(index_prompt), stdin)) {
        return;
    }
    /* Remove trailing newline character from input. */
    index_prompt[strcspn(index_prompt, "\n")] = '\0';
    /* Duplicate the input string */
    char *index_str = index_prompt;
    /* Current token of the string */
    char *token;
    /* Split the given input by comma character. */
    while ((token = strtok_r(index_str, ",", &index_str))) {
        /* Process index */
        size_t index = 0;
        /* Check token for the numeric index value. */
        if (sscanf(token, "%zu", &index) == 1 &&
                (index > 0) && (index <= stats->defunct_count)) {
            /* Send termination signal to the given process. */
            kill_proc_by_ppid(defunct_procs[index - 1].ppid, stats);
            /* Print the process's stats. */
            fprintf(stderr, " -> %s [%d](%d)\n",
                    defunct_procs[index - 1].name,
                    defunct_procs[index - 1].pid,
                    defunct_procs[index - 1].ppid);
        }
    }
}

/*!
 * Check running process's states using the `"/proc"` filesystem.
 *
 * @param settings (pointer to user-specified settings)
 * @param stats    (pointer to zombie stats)
 *
 * @return EXIT_status
 */
static int check_procs(struct zps_settings *settings, struct zps_stats *stats)
{
    /* Set begin time. */
    clock_t begin = clock();
    /* Print column titles. */
    if (settings->show_proc_list || settings->show_defunct_list) {
        cbfprintf(ANSI_FG_NORMAL, stderr, "%-7s\t%-7s\t%-5s\t%16.16s %s\n",
                 "PID", "PPID", "STATE", "NAME", "COMMAND");
    }
    /**
     * Call ftw with the following parameters to get `"/proc"` contents:
     * PROC_FILESYSTEM: `"/proc"` filesystem.
     * proc_entry_recv: Function to call for each entry found in the tree.
     * max_fd:          Maximum number of file descriptors to use.
     * FTW_PHYS:        Flag for not to follow symbolic links.
     */
    if (nftw(PROC_FILESYSTEM, proc_entry_recv, settings->max_fd, FTW_PHYS)) {
        cfprintf(ANSI_FG_RED, stderr, "ftw failed.\n");
        return EXIT_FAILURE;
    }
    /* Check for termination command line argument. */
    if (!settings->terminate) {
        return EXIT_SUCCESS;
    }
    /**
     * Handle the defunct processes after the ftw completes.
     * Terminating a process while ftw might cause interruption.
     */
    for (size_t i = 0; i < stats->defunct_count; i++) {
        /* Terminate the process or print process index. */
        if (!settings->prompt) {
            int kill_rc = kill_proc_by_ppid(defunct_procs[i].ppid, stats);
            cbfprintf_enclosed(ANSI_FG_RED, "\n[", "]", stderr,
                               kill_rc ? "Failed to terminate" : "Terminated");
        }
        else {
            cbfprintf_enclosed(ANSI_FG_RED, "\n[", "]", stderr,
                               "%zu", i + 1);
        }
        /* Print defunct process's stats. */
        fprintf(stderr,
                "\n Name:    %s\n PID:     %d\n PPID:    %d\n State:   %c\n",
                defunct_procs[i].name, defunct_procs[i].pid,
                defunct_procs[i].ppid, defunct_procs[i].state);
        if (strcmp(defunct_procs[i].cmd, "")) {
            fprintf(stderr, " Command: %s\n", defunct_procs[i].cmd);
        }
    }
    /* Check for prompt argument and defunct process count. */
    if (settings->prompt && stats->defunct_count) {
        prompt_to_kill(stats);
    }
    /* Show terminated process count and taken time. */
    fprintf(stderr, "\n%zu defunct process(es) cleaned up in %.2fs\n",
            stats->signaled_procs,
            (double)(clock() - begin) / CLOCKS_PER_SEC);

    return EXIT_SUCCESS;
}

/*!
 * Entry-point
 */
int main(int argc, char *argv[])
{
    /* Parse command line arguments. */
    parse_args(argc, argv, &settings);
    /* Check running processes. */
    return check_procs(&settings, &stats);
}
