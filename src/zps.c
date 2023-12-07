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

#include <fcntl.h>
#include <ftw.h>
#include <getopt.h>
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

/* File descriptor to be used in file operations */
static int fd;
/* Number of found defunct processes */
static unsigned int defunct_count = 0;
/* Number of terminated processes */
static unsigned int terminated_procs = 0;
/* Maximum number of file descriptors to use */
static unsigned int max_fd = MAX_FD;
/* Boolean value for terminating defunct processes */
static bool terminate = false;
/* Boolean value for listing the running processes */
static bool show_proc_list = true;
/* Boolean value for listing the defunct processes only */
static bool show_defunct_list = false;
/* Boolean value for showing prompt for the reaping option */
static bool prompt = false;
/* Char variable that used as buffer in read */
static char buff;
/* Text content of a file */
static char file_content[BLOCK_SIZE];
/* Regex match */
static char match[BLOCK_SIZE / 4];
/* Name of file to read */
static char file_name[BLOCK_SIZE / 64];
/* Input value for the prompt option */
static char index_prompt[BLOCK_SIZE / 64];
/* String part of a path in '/proc' */
static char *str_path;
/* Text content of the process's stat file */
static char *stat_content;
/* Text content of the process's command file */
static char *cmd_content;
/* Color code of process entry to print */
static char *proc_entry_color;
/* Array of defunct process's stats */
static struct proc_stats defunct_procs[BLOCK_SIZE / 4];
/* List of information about variable arguments */
static va_list vargs;
/* Regex struct */
static regex_t regex;
/* Regex match struct that contains start and end offsets */
static regmatch_t reg_match[REG_MAX_MATCH];
/* Long options for command line arguments  */
static struct option opts[] = {
    {"version", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {"reap", no_argument, NULL, 'r'},
    {"lreap", no_argument, NULL, 'x'},
    {"list", no_argument, NULL, 'l'},
    {"prompt", no_argument, NULL, 'p'},
    {"silent", no_argument, NULL, 's'},
    {"fd", required_argument, NULL, 'f'},
};

/*!
 * Write colored and formatted data to stderr.
 *
 * @param color
 * @param format
 *
 * @return EXIT_SUCCESS
 */
static int cprintf(char *color, char *format, ...)
{
    /* Set the color. */
    fprintf(stderr, "%s", color);
    /* Format and print the data. */
    va_start(vargs, format);
    vfprintf(stderr, format, vargs);
    va_end(vargs);
    /* Set color to the default. */
    fprintf(stderr, "%s", CLR_DEFAULT);
    return EXIT_SUCCESS;
}

/*!
 * Read the given file and return its content.
 *
 * @param format
 *
 * @return file_content
 */
static char *read_file(char *format, ...)
{
    /* Format the file name with the arguments. */
    va_start(vargs, format);
    vsnprintf(file_name, sizeof(file_name), format, vargs);
    va_end(vargs);
    /**
     * Open file with following flags:
     * O_RDONLY: Open for reading only.
     * S_IRUSR:  Read permission bit for the owner of the file.
     * S_IRGRP:  Read permission bit for the group owner of the file.
     * S_IROTH:  Read permission bit for other users.
     */
    fd = open(file_name, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);
    /* Check for file open error. */
    if (fd == -1) {
        return NULL;
    }
    /**
     * Read bytes from file descriptor into the buffer.
     * Use 'read until the end' method since it's not always possible to
     * read file knowing its size. ('/proc' has zero-length virtual files)
     * Also, check the boundaries while reading the file.
     */
    int i;
    for (i = 0; i < BLOCK_SIZE - 1 && read(fd, &buff, sizeof(buff)) > 0; i++) {
        file_content[i] = buff;
    }
    /* End the content string. */
    file_content[i] = '\0';
    /* Close the file descriptor and return file content. */
    close(fd);
    return file_content;
}

/*!
 * Format the content of '/proc/<pid>/stat' using regex.
 *
 * @param stat_content
 *
 * @return EXIT_status
 */
static int format_stat_content(char *stat_content)
{
    /**
     * Some of the processes contain spaces in their name so
     * parsing stats with sscanf fails on those cases.
     * Regex used for replacing the spaces in process names.
     * Compile the regex and check error.
     */
    if (regcomp(&regex, STAT_REGEX, REG_EXTENDED) != 0) {
        return EXIT_FAILURE;
    }
    /**
     * Match the content with regex pattern using the following arguments:
     * regex:         Regex struct to execute.
     * stat_content:  Content of '/proc/<pid>/stat'.
     * REG_MAX_MATCH: Maximum number of regex matches.
     * reg_match:     Regex matches.
     * REG_NOTEOL:    Flag for not matching 'match-end-of-line' operator.
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
            while(offset_begin < offset_end) {
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
    return EXIT_SUCCESS;
}

/*!
 * Parse and return the stats from process path.
 *
 * @param proc_path (process path in '/proc')
 *
 * @return proc_stats (process stats)
 */
static struct proc_stats get_proc_stats(const char *proc_path)
{
    /* Create a structure for storing parsed process's stats. */
    struct proc_stats proc_stats = {.state = DEFAULT_STATE};
    /* Read the 'status' file. */
    stat_content = read_file("%s/%s", proc_path, STAT_FILE);
    /* Check file read error and fix file content. */
    if (!stat_content || format_stat_content(stat_content)) {
        return proc_stats;
    }
    /* Parse the '/stat' file into process status struct. */
    sscanf(stat_content, "%u %64s %64s %u", &proc_stats.pid,
           proc_stats.name, proc_stats.state, &proc_stats.ppid);
    /* Remove the parentheses around the process name. */
    proc_stats.name[strnlen(proc_stats.name, sizeof(proc_stats.name)) - 1] = '\0';
    memmove(proc_stats.name, proc_stats.name + 1,
            strnlen(proc_stats.name, sizeof(proc_stats.name)));
    /* Set the defunct process state. */
    proc_stats.defunct = (strstr(proc_stats.state, STATE_ZOMBIE) != NULL);
    /* Read the 'cmdline' file and check error. */
    cmd_content = read_file("%s/%s", proc_path, CMD_FILE);
    if (!cmd_content) {
        return proc_stats;
    }
    /* Update the command variable in the process status struct. */
    strcpy(proc_stats.cmd, cmd_content);
    /* Return the process stats. */
    return proc_stats;
}

/*!
 * Event for receiving tree entry from '/proc'.
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
            !strtol(fpath + ftwbuf->base, &str_path, 10) ||
            strcmp(str_path, "")) {
        return EXIT_SUCCESS;
    }
    /* Get the process stats from the path. */
    struct proc_stats proc_stats = get_proc_stats(fpath);
    /* Set process entry color to default. */
    proc_entry_color = CLR_DEFAULT;
    /* Check for process's file parse error. */
    if (!strcmp(proc_stats.state, DEFAULT_STATE)) {
        cprintf(CLR_RED, "Failed to parse \"%s\".\n", fpath);
        return EXIT_FAILURE;
    } else if (proc_stats.defunct) { /* Check for zombie. */
        /* Add process stats to the array of defunct process stats. */
        defunct_procs[defunct_count++] = proc_stats;
        proc_entry_color = CLR_RED;
    }
    /* Print the process's stats. */
    if (show_proc_list ||
            (!strcmp(proc_entry_color, CLR_RED) && show_defunct_list)) {
        cprintf(proc_entry_color, "%-6d\t%-6d\t%-2s\t%16.16s %.64s\n",
                proc_stats.pid, proc_stats.ppid, proc_stats.state,
                proc_stats.name, proc_stats.cmd);
    }
    return EXIT_SUCCESS;
}

/*!
 * Send termination signal to the parent of the process.
 *
 * @param ppid
 * @param terminated
 *
 * @return terminated
 */
static int kill_proc_by_ppid(int ppid, int terminated)
{
    if (!kill(ppid, SIGTERM)) {
        terminated++;
        cprintf(CLR_BOLD, "\n[%sTerminated%s]", CLR_RED, CLR_DEFAULT);
    } else {
        cprintf(CLR_BOLD, "\n[%sFailed to terminate%s]", CLR_RED, CLR_DEFAULT);
    }
    return terminated;
}

/*!
 * Request user input if prompt argument is provided.
 *
 * @return EXIT_status
 */
static int show_prompt()
{
    /* Print user input message and ask for input. */
    fprintf(stderr, "\nEnter process index(es) to proceed: ");
    if (!fgets(index_prompt, sizeof(index_prompt), stdin)) {
        return EXIT_FAILURE;
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
        unsigned int index = 0;
        /* Check token for the numeric index value. */
        if ((index = atoi(token)) && (index > 0)
                && (index <= defunct_count)) {
            /* Send termination signal to the given process. */
            terminated_procs = kill_proc_by_ppid(defunct_procs[index - 1].ppid,
                                                 terminated_procs);
            /* Print the process's stats. */
            fprintf(stderr, " -> %s [%u](%u)\n",
                    defunct_procs[index - 1].name,
                    defunct_procs[index - 1].pid,
                    defunct_procs[index - 1].ppid);
        }
    }
    return EXIT_SUCCESS;
}

/*!
 * Check running process's states using the '/proc' filesystem.
 *
 * @return EXIT_status
 */
static int check_procs()
{
    /* Set begin time. */
    clock_t begin = clock();
    /* Print column titles. */
    if (show_proc_list || show_defunct_list) {
        cprintf(CLR_BOLD, "%-6s\t%-6s\t%-2s\t%16.16s %s\n",
                "PID", "PPID", "STATE", "NAME", "COMMAND");
    }
    /**
     * Call ftw with the following parameters to get '/proc' contents:
     * PROC_FILESYSTEM: '/proc' filesystem.
     * proc_entry_recv: Function to call for each entry found in the tree.
     * max_fd:          Maximum number of file descriptors to use.
     * FTW_PHYS:        Flag for not to follow symbolic links.
     */
    if (nftw(PROC_FILESYSTEM, proc_entry_recv, max_fd, FTW_PHYS)) {
        cprintf(CLR_RED, "ftw failed.\n");
        return EXIT_FAILURE;
    }
    /* Check for termination command line argument. */
    if (!terminate) {
        return EXIT_SUCCESS;
    }
    /**
     * Handle the defunct processes after the ftw completes.
     * Terminating a process while ftw might cause interruption.
     */
    for (unsigned int i = 0; i < defunct_count; i++) {
        /* Terminate the process or print process index. */
        if (!prompt) {
            terminated_procs = kill_proc_by_ppid(defunct_procs[i].ppid,
                                                 terminated_procs);
        }
        else {
            cprintf(CLR_BOLD, "\n[%s%d%s]", CLR_RED, i + 1, CLR_DEFAULT);
        }
        /* Print defunct process's stats. */
        fprintf(stderr,
                "\n Name:    %s\n PID:     %u\n PPID:    %u\n State:   %s\n",
                defunct_procs[i].name, defunct_procs[i].pid,
                defunct_procs[i].ppid, defunct_procs[i].state);
        if (strcmp(defunct_procs[i].cmd, "")) {
            fprintf(stderr, " Command: %s\n", defunct_procs[i].cmd);
        }
    }
    /* Check for prompt argument and defunct process count. */
    if (prompt && defunct_count > 0) {
        show_prompt();
    }
    /* Show terminated process count and taken time. */
    fprintf(stderr, "\n%u defunct process(es) cleaned up in %.2fs\n",
            terminated_procs, (double)(clock() - begin) / CLOCKS_PER_SEC);

    return EXIT_SUCCESS;
}

/*!
 * Parse command line arguments.
 *
 * @param argc (argument count)
 * @param argv (argument vector)
 *
 * @return EXIT_SUCCESS
 */
static int parse_args(int argc, char **argv)
{
    int opt;
    while ((opt = getopt_long(argc, argv, ":vhrxlpsf:",
            opts, NULL)) != -1) {
        switch (opt) {
        case 'v': /* Show version information. */
            cprintf(CLR_BOLD,
                "\n -hhhhdddddd/\n"
                " `++++++mMN+\n"
                "      :dMy.\n"
                "    -yMMh.\n"
                "  `oNNo:shy:`\n"
                " .dMm:```.+dNh`\n"
                " .github/orhun/zps v%s\n\n", VERSION);
            return EXIT_FAILURE;
        case 'h': /* Show help message. */
            fprintf(stderr,
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
            return EXIT_FAILURE;
        case 'l': /* List defunct processes only. */
            show_defunct_list = true;
            terminate = true;
            // fall through
        case 'r': /* Don't list running processes. */
            show_proc_list = false;
            // fall through
        case 'x': /* Reap defunct processes. */
            terminate = !terminate;
            break;
        case 'p': /* Show prompt for the reaping option. */
            prompt = true;
            terminate = true;
            break;
        case 's': /* Silent mode. */
            /* Redirect stderr to /dev/null */
            fd = open("/dev/null", O_WRONLY);
            if (fd != -1) {
                dup2(fd, 2);
                close(fd);
            }
            break;
        case 'f': /* Set file descriptor count.*/
            max_fd = atoi(optarg);
            break;
        case ':': /* Missing argument. */
            cprintf(CLR_RED, "Option requires an argument.\n");
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

/*!
 * Entry-point
 */
int main(int argc, char *argv[])
{
    /* Parse command line arguments. */
    if (parse_args(argc, argv)) {
        return EXIT_SUCCESS;
    }
    /* Check running processes. */
    return check_procs();
}
