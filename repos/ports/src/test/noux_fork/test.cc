/*
 * \brief  Simple fork test
 * \author Norman Feske
 * \date   2012-02-14
 */

#include <stdio.h>
#include <unistd.h>
#include <errno.h>

enum { MAX_COUNT = 1000 };

int main(int, char **)
{
	int i = 0;

	pid_t fork_ret = fork();
	if (fork_ret < 0) {
		printf("Error: fork returned %d, errno=%d\n", fork_ret, errno);
		return -1;
	}

	printf("pid %d: fork returned %d\n", getpid(), fork_ret);

	if (fork_ret == 0) {
		printf("pid %d: child says hello\n", getpid());
		for (int j = 0; j < MAX_COUNT; j++) {
			printf("pid %d: child  i = %d\n", getpid(), i);
		}
		return 0;
	}

	printf("pid %d: parent received child pid %d, starts counting...\n",
	       getpid(), fork_ret);

	for (; i < MAX_COUNT; ) {
		printf("pid %d: parent i = %d\n", getpid(), i++);
	}

	pause();
	return 0;
}
