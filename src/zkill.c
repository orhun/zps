// gcc -O3 -Wall zkill.c -o zkill.o
#include "zkill.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <ftw.h>

static char *strPath, 	 	 /* String part of a path in '/proc' */
	fileContent[BLOCK_SIZE];

static char* readFile(char *filename) {
	int fd = open(filename, O_RDONLY, S_IRUSR | S_IRGRP | S_IROTH);
	if (fd == -1) {
		fprintf(stderr, "Failed to open file: '%s' \n", filename);
	}
	int i = 0;
	char c;
	while (read(fd, &c, sizeof(c)) != 0) {
		fileContent[i] = c;
		i++;
	}
	close(fd);
	return fileContent;
}

/*!
 * Event for receiving tree entry from '/proc'.
 *
 * \param fpath  (pathname of the entry)
 * \param sb     (file status structure for fpath)
 * \param tflag  (type flag of the entry)
 * \param ftwbuf (structure that contains entry base and level)
 * \return EXIT_status
 */
static int procEntryRecv(const char *fpath, const struct stat *sb,
		int tflag, struct FTW *ftwbuf) {
	/* Check for depth of the fpath (1), type of the entry (directory),
	 * base of the fpath (numeric value) to filter entries except PID.
	 */
    if (ftwbuf->level == 1 && tflag == FTW_D &&
        strtol(fpath + ftwbuf->base, &strPath, 10) &&
        !strcmp(strPath, "")) {

        //fprintf(stderr, "[%s]\n", fpath + ftwbuf->base);
		if (!strcmp(fpath + ftwbuf->base, "1")) {
			char pidStatusFile[sizeof(fpath)+sizeof(STATUS_FILE)];
			strcpy(pidStatusFile, fpath);
			strcat(pidStatusFile, STATUS_FILE);
			fprintf(stderr, "%s", readFile(pidStatusFile));
		}
    }
    return EXIT_SUCCESS;
}

/*!
 * Parse command line arguments.
 *
 * \param argc (argument count)
 * \param argv (argument vector)
 * \return EXIT_status
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