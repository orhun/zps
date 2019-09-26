// gcc -O3 -Wall zkill.c -o zkill.o
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "zkill.h"

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

    return EXIT_SUCCESS;
}