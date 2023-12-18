/**!
 * On Unix and Unix-like computer operating systems, a zombie process
 * or defunct process is a process that has completed execution but still
 * has an entry in the process table. (Wikipedia)
 * This program illustrates how zombie or defunct processes are created.
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

/*!
 * Entry point
 */
int main(int argc, char *argv[])
{
    unsigned int sleep_seconds = 60;
    if (argc == 2) { /* Parse command line argument. */
        sscanf(argv[1], "%u", &sleep_seconds);
    }
    pid_t pid = fork();
    if (pid > 0) { /* Parent process */
        /**
         * Sleep and eventually exit without the wait call.
         * This will cause child process to be a defunct process.
         */
        fprintf(stderr, "PPID: %d\n", getpid());
        sleep(sleep_seconds);
    } else if (!pid) { /* Child process */
        fprintf(stderr, "PID: %d\n", getpid());
    }
    return EXIT_SUCCESS;
}
