#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pid_t childPID; /* PID of the child process */

int main () {
	/* Call fork to create a child process. */
	childPID = fork();
	/* Parent process ID. */
	if (childPID > 0) {
		/* Sleep for 60s and eventually exit without the wait call. */
    	fprintf(stderr, "PPID: %d\n", getpid());
    	sleep(60);
	/* Child process ID. */
  	} else if (childPID == 0) {
    	fprintf(stderr,"PID: %d\n", getpid());
  	}
	return EXIT_SUCCESS;
}