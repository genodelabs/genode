/*
 * \brief  Simple fork test
 * \author Norman Feske
 * \date   2012-02-14
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>

enum { MAX_COUNT = 1000 };

int main(int, char **)
{
	printf("--- test-noux_fork started ---\n");

	pid_t fork_ret = fork();
	if (fork_ret < 0) {
		printf("Error: fork returned %d, errno=%d\n", fork_ret, errno);
		return -1;
	}

	printf("pid %d: fork returned %d\n", getpid(), fork_ret);

	/* child */
	if (fork_ret == 0) {
		printf("pid %d: child says hello\n", getpid());

		pid_t fork_ret = fork();
		if (fork_ret < 0) {
			printf("Error: fork returned %d, errno=%d\n", fork_ret, errno);
			return -1;
		}

		printf("pid %d: fork returned %d\n", getpid(), fork_ret);

		/* grand child */
		if (fork_ret == 0) {
			printf("pid %d: grand child says hello\n", getpid());

			for (int k = 0; k < MAX_COUNT; k++) {
				printf("pid %d: grand child k = %d\n", getpid(), k);
			}

			return 0;
		};

		for (int j = 0; j < MAX_COUNT; j++) {
			printf("pid %d: child       j = %d\n", getpid(), j);
		}

		if (fork_ret != 0) {
			printf("pid %d: child waits for grand-child exit\n", getpid());
			waitpid(fork_ret, nullptr, 0);
		}

		return 0;
	}

	printf("pid %d: parent received child pid %d, starts counting...\n",
	       getpid(), fork_ret);

	for (int i = 0; i < MAX_COUNT; ) {
		printf("pid %d: parent      i = %d\n", getpid(), i++);
	}

	printf("pid %d: parent waits for child exit\n", getpid());
	waitpid(fork_ret, nullptr, 0);

	printf("--- parent done ---\n");
	return 0;
}
