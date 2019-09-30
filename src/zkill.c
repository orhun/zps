// gcc -O3 -Wall zkill.c -o zkill.o
#include "zkill.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <ftw.h>

static int fd;          	 /* File descriptor to be used in file operations */
static char *strPath, 	 	 /* String part of a path in '/proc' */
	fileContent[BLOCK_SIZE], /* Text content of a file */
	buff;                    /* Char variable that used as buffer in read */

/*!
 * Read the given file and return its content.
 *
 * @param filename
 * @return fileContent
 */
static char* readFile(char *fileName) {
	/**
	 * Open file with following flags:
	 * O_RDONLY: Open for reading only.
	 * S_IRUSR: Read permission bit for the owner of the file.
	 * S_IRGRP: Read permission bit for the group owner of the file.
	 * S_IROTH: Read permission bit for other users.
	 */
	fd = open(fileName, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);
	/* Check for file open error. */
	if (fd == -1)
		return NULL;
	/**
	 * Read bytes from file descriptor into the buffer.
	 * Use 'read until the end' method since it's not always possible to
	 * read file knowing its size. ('/proc' has zero-length virtual files)
	 */
	for (int i = 0; read(fd, &buff, sizeof(buff)) != 0; i++) {
		fileContent[i] = buff;
	}
	/* Close the file descriptor and return file content. */
	close(fd);
	return fileContent;
}

/*!
 * Check the given process' status.
 *
 * @param procPath (process path in '/proc')
 * @return PROCESS_status
 */
static int checkProcStatus(const char *procPath) {
	/* Array for storing the status file name of the process. */
	char pidStatusFile[sizeof(procPath)+sizeof(STATUS_FILE)];
	/* Fill the array with the given parameter and append '/status'. */
	strcpy(pidStatusFile, procPath);
	strcat(pidStatusFile, STATUS_FILE);
	/* Read the PID status file. */
	char *content = readFile(pidStatusFile);
	if (content == NULL)
		return PROCESS_READ_ERROR;
	fprintf(stderr, "%s", content);
	return PROCESS_DRST;
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
        !strcmp(strPath, "")) {
		if (strcmp(fpath + ftwbuf->base, "1"))
			return EXIT_SUCCESS;

		switch (checkProcStatus(fpath)) {
			case PROCESS_DRST:
				break;
			case PROCESS_ZOMBIE:
				break;
			case PROCESS_READ_ERROR:
				fprintf(stderr, "Failed to open file: '%s' \n", fpath);
				break;
		}
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
                fprintf(stderr, "zkill v%s\n", VERSION);
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
    /* Call ftw to get '/proc' contents. */
    if (nftw(PROC_FS, procEntryRecv, USE_FDS, FTW_PHYS) == -1)
        return EXIT_FAILURE;
    return EXIT_SUCCESS;
}