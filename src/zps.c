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

#include "zps.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <regex.h>
#include <stdbool.h>
#include <time.h>
#include <ftw.h>

static int fd;                           /* File descriptor to be used in file operations */
static unsigned int defunctCount    = 0; /* Number of found defunct processes */
static unsigned int terminatedProcs = 0; /* Number of terminated processes */
static unsigned int procsChecked    = 0; /* Return value for the process check operation */
static unsigned int maxFD = MAX_FD;      /* Maximum number of file descriptors to use */
static bool terminate       = false;     /* Boolean value for terminating defunct processes */
static bool showProcList    = true;      /* Boolean value for listing the running processes */
static bool showDefunctList = false;     /* Boolean value for listing the defunct processes only */
static bool prompt          = false;     /* Boolean value for showing prompt for the reaping option */
static char buff;                        /* Char variable that used as buffer in read */
static char fileContent[BLOCK_SIZE];     /* Text content of a file */
static char match[BLOCK_SIZE/4];         /* Regex match */
static char fileName[BLOCK_SIZE/64];     /* Name of file to read */
static char indexPrompt[BLOCK_SIZE/64];  /* Input value for the prompt option */
static char *strPath;                    /* String part of a path in '/proc' */
static char *statContent;                /* Text content of the process's stat file */
static char *cmdContent;                 /* Text content of the process's command file */
static char *procEntryColor;             /* Color code of process entry to print */
typedef struct {                         /* Struct for storing process stats */
    unsigned int pid;
    unsigned int ppid;
    char name[BLOCK_SIZE/64];
    char state[BLOCK_SIZE/64];
    char cmd[BLOCK_SIZE];
    bool defunct;
} ProcStats;
static ProcStats
    defunctProcs[BLOCK_SIZE/4]; /* Array of defunct process's stats */
static va_list vargs;           /* List of information about variable arguments */
static regex_t regex;           /* Regex struct */
static regmatch_t               /* Regex match struct that contains start and end offsets */
    regMatch[REG_MAX_MATCH];
static struct option opts[] = { /* Long options for command line arguments  */
    {"version", no_argument,
        NULL, 'v'},
    {"help", no_argument,
        NULL, 'h'},
    {"reap", no_argument,
        NULL, 'r'},
    {"xreap", no_argument,
        NULL, 'x'},
    {"list", no_argument,
        NULL, 'l'},
    {"prompt", no_argument,
        NULL, 'p'},
    {"silent", no_argument,
        NULL, 's'},
    {"fd", required_argument,
        NULL, 'f'},
};

/*!
 * Write colored and formatted data to stream. (stderr)
 *
 * @param  color
 * @param  format
 * @return EXIT_SUCCESS
 */
static int cprintf(char *color, char *format, ...) {
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
 * @param  format
 * @return fileContent
 */
static char* readFile(char *format, ...) {
    /* Format the file name with the arguments. */
    va_start(vargs, format);
    vsnprintf(fileName, sizeof(fileName), format, vargs);
    va_end(vargs);
    /**
     * Open file with following flags:
     * O_RDONLY: Open for reading only.
     * S_IRUSR:  Read permission bit for the owner of the file.
     * S_IRGRP:  Read permission bit for the group owner of the file.
     * S_IROTH:  Read permission bit for other users.
     */
    fd = open(fileName, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);
    /* Check for file open error. */
    if (fd == -1) return NULL;
    /* Empty the content string. */
    fileContent[0] = '\0';
    /**
     * Read bytes from file descriptor into the buffer.
     * Use 'read until the end' method since it's not always possible to
     * read file knowing its size. ('/proc' has zero-length virtual files)
     * Also, check the boundaries while reading the file.
     */
    for (int i = 0; read(fd, &buff, sizeof(buff)) != 0; i++) {
        fileContent[i] = buff;
    }
    /* Close the file descriptor and return file content. */
    close(fd);
    return fileContent;
}

/*!
 * Format the content of '/proc/<pid>/stat' using regex.
 *
 * @param  statContent
 * @return EXIT_status
 */
static int formatStatContent(char *statContent) {
    /**
     * Some of the processes contain spaces in their name so
     * parsing stats with sscanf fails on those cases.
     * Regex used for replacing the spaces in process names.
     * Compile the regex and check error.
     */
    if (regcomp(&regex, STAT_REGEX, REG_EXTENDED) != 0) return EXIT_FAILURE;
    /**
     * Match the content with regex pattern using the following arguments:
     * regex:         Regex struct to execute.
     * statContent:   Content of '/proc/<pid>/stat'.
     * REG_MAX_MATCH: Maximum number of regex matches.
     * regMatch:      Regex matches.
     * REG_NOTEOL:    Flag for not matching 'match-end-of-line' operator.
     */
    if ((regexec(&regex, statContent, REG_MAX_MATCH, regMatch, REG_NOTEOL))
        != REG_NOMATCH) {
        char *offsetBegin = statContent + regMatch[1].rm_so; /* Beginning offset of first match   */
        char *offsetEnd   = statContent + regMatch[1].rm_eo; /* Ending offset of first match      */
        char *contentDup  = strdup(statContent);             /* Duplicate of content for changing */
        regoff_t offsetSpace = regMatch[1].rm_so - 1;        /* Offset of first space in content  */
        int matchLength = strcspn(offsetBegin, " ");         /* Length of the match  */
        /* Check the match length for parsing or replacing spaces. */
        if (matchLength <= (regMatch[1].rm_eo-regMatch[1].rm_so)) {
            matchLength = 0;
            /* Loop through the matches using offsets. */
            while(offsetBegin < offsetEnd) {
                /* Change the space character if the space is inside the parentheses. */
                if (offsetBegin != offsetEnd && matchLength != 0)
                    contentDup[offsetSpace] = SPACE_REPLACEMENT;
                /* Set the match length using the space span. */
                matchLength = strcspn(offsetBegin, " ");
                /* Set the current match. */
                sprintf(match, "%.*s", matchLength, offsetBegin);
                /* Next space is always the character next to the current match. */
                offsetSpace += matchLength + 1;
                /* Set the offsets for the next match. */
                offsetBegin += matchLength;
                offsetBegin += strspn(offsetBegin, " ");
            }
            /* Update the original file content and deallocate the memory. */
            strcpy(statContent, contentDup);
            free(contentDup);
        }
    }
    return EXIT_SUCCESS;
}

/*!
 * Parse and return the stats from process path.
 *
 * @param  procPath   (process path in '/proc')
 * @return procStats (process stats)
 */
static ProcStats getProcStats(const char *procPath) {
    /* Create a structure for storing parsed process's stats. */
    ProcStats procStats = {.state=DEFAULT_STATE};
    /* Read the 'status' file. */
    statContent = readFile("%s/%s", procPath, STAT_FILE);
    /* Check file read error and fix file content. */
    if (statContent == NULL || formatStatContent(statContent)) return procStats;
    /* Parse the '/stat' file into process status struct. */
    sscanf(statContent, "%u %64s %64s %u", &procStats.pid,
        procStats.name, procStats.state, &procStats.ppid);
    /* Remove the parentheses around the process name. */
    procStats.name[strnlen(procStats.name, sizeof(procStats.name))-1] = '\0';
    memmove(procStats.name, procStats.name+1,
        strnlen(procStats.name, sizeof(procStats.name)));
    /* Set the defunct process state. */
    procStats.defunct = (strstr(procStats.state, STATE_ZOMBIE) != NULL);
    /* Read the 'cmdline' file and check error. */
    cmdContent = readFile("%s/%s", procPath, CMD_FILE);
    if (cmdContent == NULL) return procStats;
    /* Update the command variable in the process status struct. */
    strcpy(procStats.cmd, cmdContent);
    /* Return the process stats. */
    return procStats;
}

/*!
 * Event for receiving tree entry from '/proc'.
 *
 * @param  fpath  (path name of the entry)
 * @param  sb     (file status structure for fpath)
 * @param  tflag  (type flag of the entry)
 * @param  ftwbuf (structure that contains entry base and level)
 * @return EXIT_status
 */
static int procEntryRecv(const char *fpath, const struct stat *sb,
    int tflag, struct FTW *ftwbuf) {
    (void)sb;
    /**
     * Check for depth of the fpath (1), type of the entry (directory),
     * base of the fpath (numeric value) to filter entries except PID.
     */
    if (ftwbuf->level != 1 || tflag != FTW_D ||
        !strtol(fpath + ftwbuf->base, &strPath, 10) ||
        strcmp(strPath, "")) {
        return EXIT_SUCCESS;
    }
    /* Get the process stats from the path. */
    ProcStats procStats = getProcStats(fpath);
    /* Set process entry color to default. */
    procEntryColor = CLR_DEFAULT;
    /* Check for process's file parse error. */
    if (!strcmp(procStats.state, DEFAULT_STATE)) {
        cprintf(CLR_RED, "Failed to parse \"%s\".\n", fpath);
        return EXIT_FAILURE;
    /* Check for the process state for being zombie. */
    } else if (procStats.defunct) {
        /* Add process stats to the array of defunct process stats. */
        defunctProcs[defunctCount++] = procStats;
        procEntryColor = CLR_RED;
    }
    /* Print the process's stats. */
    if (showProcList || (!strcmp(procEntryColor, CLR_RED) && showDefunctList))
        cprintf(procEntryColor, "%-6d\t%-6d\t%-2s\t%16.16s %.64s\n",
        procStats.pid, procStats.ppid, procStats.state,
        procStats.name, procStats.cmd);
    return EXIT_SUCCESS;
}

/*!
 * Send termination signal to the parent of the process.
 *
 * @param  PPID
 * @param  terminated
 * @return terminated
 */
static int killProcByPPID(int PPID, int terminated) {
    if(!kill(PPID, SIGTERM)) {
        terminated++;
        cprintf(CLR_BOLD, "\n[%sTerminated%s]", CLR_RED, CLR_DEFAULT);
    } else {
        cprintf(CLR_BOLD, "\n[%sFailed to terminate%s]", CLR_RED,
            CLR_DEFAULT);
    }
    return terminated;
}

/*!
 * Request user input if prompt argument is provided.
 *
 * @return EXIT_SUCCESS
 */
static int showPrompt() {
    /* Print user input message and ask for input. */
    fprintf(stderr, "\nEnter process index(es) to proceed: ");
    fgets(indexPrompt, sizeof(indexPrompt), stdin);
    /* Remove trailing newline character from input. */
    indexPrompt[strcspn(indexPrompt, "\n")] = 0;
    char* indexStr = indexPrompt; /* Duplicate the input string */
    char* token;                  /* Current token of the string */
    /* Split the given input by comma character. */
    while ((token = strtok_r(indexStr, ",", &indexStr))) {
        unsigned int index = 0;   /* Process index */
        /* Check token for the numeric index value. */
        if((index = atoi(token)) && (index > 0)
            && (index <= defunctCount)) {
            /* Send termination signal to the given process. */
            terminatedProcs = killProcByPPID(
                defunctProcs[index-1].ppid, terminatedProcs);
            /* Print the process's stats. */
            fprintf(stderr, " -> %s [%u](%u)\n",
                defunctProcs[index-1].name,
                defunctProcs[index-1].pid,
                defunctProcs[index-1].ppid);
        }
    }
    return EXIT_SUCCESS;
}

/*!
 * Check running process's states using the '/proc' filesystem.
 *
 * @return EXIT_status
 */
static int checkProcs() {
    /* Set begin time. */
    clock_t begin = clock();
    /* Print column titles. */
    if (showProcList || showDefunctList) cprintf(CLR_BOLD,
        "%-6s\t%-6s\t%-2s\t%16.16s %s\n", "PID", "PPID",
        "STATE", "NAME", "COMMAND");
    /**
     * Call ftw with the following parameters to get '/proc' contents:
     * PROC_FILESYSTEM: '/proc' filesystem.
     * procEntryRecv:   Function to call for each entry found in the tree.
     * maxFD:           Maximum number of file descriptors to use.
     * FTW_PHYS:        Flag for not to follow symbolic links.
     */
    if (nftw(PROC_FILESYSTEM, procEntryRecv,
        maxFD, FTW_PHYS)) {
        cprintf(CLR_RED, "ftw failed.\n");
        return EXIT_FAILURE;
    }
    /* Check for termination command line argument. */
    if (!terminate) return EXIT_SUCCESS;
    /**
     * Handle the defunct processes after the ftw completes.
     * Terminating a process while ftw might cause interruption.
     */
    for(unsigned int i = 0; i < defunctCount; i++) {
        /* Terminate the process or print process index. */
        if (!prompt) terminatedProcs = killProcByPPID(
            defunctProcs[i].ppid, terminatedProcs);
        else cprintf(CLR_BOLD, "\n[%s%d%s]", CLR_RED,
            i+1, CLR_DEFAULT);
        /* Print defunct process's stats. */
        fprintf(stderr, "\n Name:    %s\n PID:"
            "     %u\n PPID:    %u\n State:   %s\n",
            defunctProcs[i].name, defunctProcs[i].pid,
            defunctProcs[i].ppid, defunctProcs[i].state);
        if (strcmp(defunctProcs[i].cmd, "")) fprintf(stderr,
            " Command: %s\n", defunctProcs[i].cmd);
    }
    /* Check for prompt argument and defunct process count. */
    if (prompt && defunctCount > 0) showPrompt();
    /* Show terminated process count and taken time. */
    fprintf(stderr, "\n%u defunct process(es) cleaned up in %.2fs\n",
        terminatedProcs, (double)(clock() - begin) / CLOCKS_PER_SEC);
    return EXIT_SUCCESS;
}

/*!
 * Parse command line arguments.
 *
 * @param  argc (argument count)
 * @param  argv (argument vector)
 * @return EXIT_SUCCESS
 */
static int parseArgs(int argc, char **argv){
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
                "  -x, --xreap     list and reap zombie processes\n"
                "  -l, --list      list zombie processes only\n"
                "  -p, --prompt    show prompt for selecting processes\n"
                "  -f, --fd <num>  set maximum file descriptors (default: 15)\n"
                "  -s, --silent    run in silent mode\n"
                "  -v, --version   show version\n"
                "  -h, --help      show help\n\n");
                return EXIT_FAILURE;
            case 'l': /* List defunct processes only. */
                showDefunctList = true;
                terminate = true;
                // fall through
            case 'r': /* Don't list running processes. */
                showProcList = false;
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
                maxFD = atoi(optarg);
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
int main(int argc, char *argv[]) {
    /* Parse command line arguments. */
    if(parseArgs(argc, argv)) return EXIT_SUCCESS;
    /* Check running processes. */
    procsChecked = checkProcs();
    return procsChecked;
}
