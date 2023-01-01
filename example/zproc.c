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
#include <unistd.h>

static pid_t childPID;        /* PID of the child process */
static int sleepSeconds = 60; /* Sleep time of the parent process */

/*!
 * Entry-point
 */
int main (int argc, char *argv[]) {
	/* Parse command line arguments. */
	if (argc > 1)
		 sleepSeconds = atoi(argv[1]);
	/* Call fork to create a child process. */
	childPID = fork();
	/* Parent process ID. */
	if (childPID > 0) {
		/**
		* Sleep and eventually exit without the wait call.
		* This will cause child process to be a defunct process.
		*/
		fprintf(stderr, "PPID: %d\n", getpid());
		sleep(sleepSeconds);
	/* Child process ID. */
  	} else if (childPID == 0) {
		fprintf(stderr,"PID: %d\n", getpid());
  	}
	return EXIT_SUCCESS;
}
