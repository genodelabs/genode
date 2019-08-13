/*
 * \brief  Simple fork test
 * \author Norman Feske
 * \date   2012-02-14
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>

enum { MAX_COUNT = 100 };

int main(int, char **argv)
{
	printf("--- test-fork started ---\n");

	enum { MSG_SIZE = 100 };
	static char message_in_rw_segment[MSG_SIZE];
	char const * const message_about_rw_segment = "message stored in rw segment";
	strncpy(message_in_rw_segment, message_about_rw_segment, MSG_SIZE);

	char       * const message_on_heap    = (char *)malloc(MSG_SIZE);
	char const * const message_about_heap = "message stored on the heap";
	strncpy(message_on_heap, message_about_heap, MSG_SIZE - 1);

	enum { ARGV0_SIZE = 100 };
	static char parent_argv0[ARGV0_SIZE];
	strncpy(parent_argv0, argv[0], ARGV0_SIZE - 1);

	pid_t fork_ret = fork();
	if (fork_ret < 0) {
		printf("Error: fork returned %d, errno=%d\n", fork_ret, errno);
		return -1;
	}

	printf("pid %d: fork returned %d\n", getpid(), fork_ret);

	/* child */
	if (fork_ret == 0) {
		printf("pid %d: child says hello\n", getpid());

		/*
		 * Validate that the child's heap and RW segment correspond to the
		 * the state of the parent.
		 */
		printf("RW segment: %s\n", message_in_rw_segment);
		if (strcmp(message_in_rw_segment, message_about_rw_segment)) {
			printf("Error: unexpected content of the child's RW segment\n");
			return -1;
		}

		printf("argv0: %s\n", argv[0]);
		if (!argv[0] || strcmp(argv[0], parent_argv0)) {
			printf("Error: unexpected content of the child's args buffer\n");
			return -1;
		}

		printf("heap: %s\n", message_on_heap);
		if (strcmp(message_on_heap, message_about_heap)) {
			printf("Error: unexpected content on the child's heap\n");
			return -1;
		}

		{
			char c = 0;
			if (read(3, &c, 1) == 1) {
				printf("read character '%c' from FD 3\n", c);
				if (c != '5') {
					printf("Error: read unexpected value from FD 3\n");
					return -1;
				}
			}
		}

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
