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

#include "zps.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <regex.h>
#include <ftw.h>

static int fd,		            /* File descriptor to be used in file operations */
	defunctCount = 0;           /* Number of found defunct processes */
static char *strPath,		    /* String part of a path in '/proc' */
	fileContent[BLOCK_SIZE],    /* Text content of a file */
    match[BLOCK_SIZE/4],        /* Regex match */
	buff,                       /* Char variable that used as buffer in read */
	*statContent, *cmdContent;  /* Text content of the process' information file */
typedef struct {	            /* Struct for storing process stats */
	int pid;
	int ppid;
	char name[BLOCK_SIZE/64];
	char state[BLOCK_SIZE/64];
	char cmd[BLOCK_SIZE];
} ProcStats;
static ProcStats
	defunctProcs[BLOCK_SIZE/4]; /* Array of defunct process' stats */
static va_list vargs;		    /* List of information about variable arguments */
static regex_t regex;           /* Regex struct */
static regmatch_t               /* Regex match struct that contains start and end offsets */
	regMatch[REG_MAX_MATCH];

/*!
 * Write colored and formatted data to stream. (stderr)
 *
 * @param  color
 * @param  format
 * @return EXIT_status
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
 * @param  filename
 * @param  format
 * @return fileContent
 */
static char* readFile(char *fileName, char *format, ...) {
	/* Format the given file name with the arguments. */
	va_start(vargs, format);
	vsnprintf(fileName, sizeof(vargs)+1, format, vargs);
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
	if (fd == -1)
		return NULL;
	/* Empty the content string. */
	fileContent[0] = '\0';
	/**
	 * Read bytes from file descriptor into the buffer.
	 * Use 'read until the end' method since it's not always possible to
	 * read file knowing its size. ('/proc' has zero-length virtual files)
     * Also, check the boundaries while reading the file.
	 */
	for (int i = 0; i < sizeof(fileContent) &&
        read(fd, &buff, sizeof(buff)) != 0; i++) {
		fileContent[i] = buff;
	}
	/* Close the file descriptor and return file content. */
	close(fd);
	return fileContent;
}

/*!
 * Parse and return the stats from process path.
 *
 * @param procPath   (process path in '/proc')
 * @return procStats (process stats)
 */
static ProcStats getProcStats(const char *procPath) {
	/* Create a structure for storing parsed process' stats. */
	ProcStats procStats = {.state=DEFAULT_STATE};
	/* Array for storing information about the process. */
	char pidStatFile[strlen(procPath)+strlen(STAT_FILE)+1],
		pidCmdFile[strlen(procPath)+strlen(CMD_FILE)+1];
	/* Read the 'status' file and check error. */
	statContent = readFile(pidStatFile, "%s%s", procPath, STAT_FILE);
	if (statContent == NULL)
		goto RETURN;
    /**
     * Some of the processes contain spaces in their name so
     * parsing stats with sscanf fails on those cases.
     * Regex used for replacing the spaces in process names.
     * Compile the regex and check error.
     */
	if (regcomp(&regex, STAT_REGEX, REG_EXTENDED) != 0)
		goto RETURN;
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
        char *offsetBegin = statContent + regMatch[1].rm_so, /* Beginning offset of first match. */
            *offsetEnd    = statContent + regMatch[1].rm_eo, /* Ending offset of first match. */
            *contentDup   = strdup(statContent);             /* Duplicate of content for changing */
        int offsetSpace   = regMatch[1].rm_so - 1,           /* Offset of first space in content */
            matchLength   = (int) strcspn(offsetBegin, " "); /* Length of the match */
        /* Check the match length for parsing or replacing spaces. */
        if (matchLength > (regMatch[1].rm_eo-regMatch[1].rm_so))
            goto PARSE;
        else
            matchLength = 0;
        /* Loop through the matches using offsets. */
        while(offsetBegin < offsetEnd) {
            /* Change the space character if the space is inside the parentheses. */
            if (offsetBegin != offsetEnd && matchLength != 0)
                contentDup[offsetSpace] = SPACE_REPLACEMENT;
            /* Set the match length using the space span. */
            matchLength = (int) strcspn(offsetBegin, " ");
            /* Set the current match. */
            sprintf(match, "%.*s", matchLength, offsetBegin);
            /* Next space is always the character next to the current match. */
            offsetSpace += strlen(match) + 1;
            /* Set the offsets for the next match. */
            offsetBegin += matchLength;
            offsetBegin += strspn(offsetBegin, " ");
        }
        /* Update the original file content and deallocate the memory. */
        strcpy(statContent, contentDup);
        free(contentDup);
    }
    PARSE:
	/* Parse the '/stat' file into process status struct. */
	sscanf(statContent, "%d %64s %64s %d", &procStats.pid,
		procStats.name, procStats.state, &procStats.ppid);
	/* Remove the parentheses around the process name. */
	procStats.name[strlen(procStats.name)-1] = '\0';
	memmove(procStats.name, procStats.name+1, strlen(procStats.name));
	/* Read the 'cmdline' file and check error. */
	cmdContent = readFile(pidCmdFile, "%s%s", procPath, CMD_FILE);
	if (cmdContent == NULL)
		goto RETURN;
	/* Update the command variable in the process status struct. */
	strcpy(procStats.cmd, cmdContent);
	/* Return the process stats. */
	RETURN: return procStats;
}

/*!
 * Event for receiving tree entry from '/proc'.
 *
 * @param fpath  (path name of the entry)
 * @param sb     (file status structure for fpath)
 * @param tflag  (type flag of the entry)
 * @param ftwbuf (structure that contains entry base and level)
 * @return EXIT_status
 */
static int procEntryRecv(const char *fpath, const struct stat *sb,
		int tflag, struct FTW *ftwbuf) {
	/**
	 * Check for depth of the fpath (1), type of the entry (directory),
	 * base of the fpath (numeric value) to filter entries except PID.
	 */
    if (ftwbuf->level == 1 && tflag == FTW_D &&
        strtol(fpath + ftwbuf->base, &strPath, 10) &&
        !strncmp(strPath, "", strlen(strPath)+1)) {
		/* Get the process stats from the path. */
		ProcStats procStats = getProcStats(fpath);
		/* Check for process' file parse error. */
		if (!strncmp(procStats.state, DEFAULT_STATE,
			strlen(procStats.state)+1)) {
			cprintf(CLR_RED, "Failed to parse \"%s\".\n", fpath);
			exit(0);
		/* Check for the process state for being zombie. */
		} else if (strstr(procStats.state, STATE_ZOMBIE) != NULL) {
			/* Add process stats to the array of defunct process stats. */
			defunctProcs[defunctCount++] = procStats;
		} else {
			fprintf(stderr, "%-6d\t%-6d\t%-2s\t%16.16s %.64s\n", procStats.pid,
				procStats.ppid ,procStats.state, procStats.name, procStats.cmd);
		}
    }
    return EXIT_SUCCESS;
}

/*!
 * Check running process' states using the '/proc' filesystem.
 *
 * @return EXIT_status
 */
static int checkProcesses() {
	cprintf(CLR_BOLD, "%-6s\t%-6s\t%-2s\t%16.16s %s\n",
		"PID", "PPID", "STATE", "NAME", "COMMAND");
	/**
	 * Call ftw with the following parameters to get '/proc' contents:
	 * PROC_FS:       '/proc' filesystem.
	 * procEntryRecv: Function to call for each entry found in the tree.
	 * USE_FDS:       Maximum number of file descriptors to use.
	 * FTW_PHYS:      Flag for not to follow symbolic links.
	 */
	if (nftw(PROC_FS, procEntryRecv, USE_FDS, FTW_PHYS)) {
		return EXIT_FAILURE;
	}
	/**
	 * Handle the defunct processes after the ftw completes.
	 * Terminating a process while ftw might cause interruption.
	 */
	fprintf(stderr, "%c[2KDefunct (zombie) "
		"processes found: %d\n", 27, defunctCount);
	for(int i = 0; i < defunctCount; i++) {
		fprintf(stderr, "Process (%s): %d, PPID: %d ", defunctProcs[i].state,
					defunctProcs[i].pid, defunctProcs[i].ppid);
		/* Send termination signal to the parent of defunct process. */
		if(!kill(defunctProcs[i].ppid, SIGTERM))
			fprintf(stderr, "(terminated)\n");
		else
			fprintf(stderr, "(failed to terminate)\n");
	}
	return EXIT_SUCCESS;
}

/*!
 * Parse command line arguments.
 *
 * @param argc (argument count)
 * @param argv (argument vector)
 * @return EXIT_status
 */
static int parseArgs(int argc, char **argv){
    int opt;
    while ((opt = getopt(argc, argv, "v")) != -1) {
        switch (opt) {
            case 'v': /* Show version information. */
                fprintf(stderr, "zps v%s\n", VERSION);
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
	if(parseArgs(argc, argv))
		return EXIT_SUCCESS;
	/* Check running processes. */
	return checkProcesses();
}