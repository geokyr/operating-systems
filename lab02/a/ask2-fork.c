#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "proc-common.h"

#define SLEEP_PROC_SEC  10
#define SLEEP_TREE_SEC  3

/* Create this process tree:
 * A---|---B----D
 *     |---C
 */
 
void fork_procs(void)
{
	/* Initial process is A */
	printf("A: Initiating...\n");
	change_pname("A");

	pid_t pidB, pidC, pidD;
	int status;

	/* Child B */
	pidB = fork();

	if(pidB < 0) {
		perror("fork_procs: forkB");
		exit(1);
	}
	if(pidB == 0) {
		printf("B: Initiating...\n");
		change_pname("B");

		/* Child D */
		pidD = fork();

		if(pidD < 0) {
			perror("fork_procs: forkD");
			exit(1);
		}
		if(pidD == 0) {
			printf("D: Initiating...\n");
			change_pname("D");
			printf("D: Sleeping...\n");
			sleep(SLEEP_PROC_SEC);
			exit(13);
		}

		printf("B: Waiting...\n");
		pidD = wait(&status);
		explain_wait_status(pidD, status);
		exit(19);
	}

	/* Child C */
	pidC = fork();

	if(pidC < 0) {
		perror("fork_procs: forkC");
		exit(1);
	}
	if(pidC == 0) {
		printf("C: Initiating...\n");
		change_pname("C");
		printf("C: Sleeping...\n");
		sleep(SLEEP_PROC_SEC);
		exit(17);
	}

	printf("A: Waiting...\n");
	pidC = wait(&status);
	explain_wait_status(pidC, status);

	pidB = wait(&status);
	explain_wait_status(pidB, status);

	exit(16);
}

int main(void)
{
	pid_t pid;
	int status;

	/* Fork root of process tree */
	pid = fork();
	
	if (pid < 0) {
		perror("main: fork");
		exit(1);
	}
	if (pid == 0) {
		/* Child A */
		fork_procs();
		exit(1);
	}

	/* Father */
	sleep(SLEEP_TREE_SEC);

	/* Print the process tree root at pid */
	show_pstree(pid);

	/* Wait for the root of the process tree to terminate */
	pid = wait(&status);
	explain_wait_status(pid, status);

	return 0;
}


/* 1. Exit message "terminated by a signal" then B, C, D exit as well but only
 * D produces exit message "terminated normally", since orphaned processes B, C 
 * are adopted by init (PID = 1) which performs the waiting system until the orphaned
 * processes terminate. We can see that using the ps -ef command after killing process
 * A and again a few seconds later, when D has exiting normally, and B, C have exited
 * after being adopted by init, without producing an exit message (since their parent
 * A was responsible for that).
 * 
 * 2. Processes ask2-fork (root), sh (child) and pstree (grandchild) also appear on the 
 * tree as a second branch with the first branch being the previous one (using pid instead 
 * of getpid()) with processes A, B, C, D. This is because the function system within
 * show_pstree uses fork to create a child process that executes the command created 
 * with sprintf (including pstree).
 *
 * 3. If there was no user limit, a user could create, whether intentionally or not, 
 * a big enough number of processes, therefore having the system run out of memory.
 * In addition, this user limit prevents fork bombs, which when executed can slow down
 * or crash the system and require a restart, due to resource starvation.
 */