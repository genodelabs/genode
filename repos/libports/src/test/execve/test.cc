/*
 * \brief  Simple execve test
 * \author Norman Feske
 * \date   2019-08-20
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	if (argc <= 1)
		return -1;

	int const count = atoi(argv[1]);

	printf("count %d\n", count);

	if (count <= 0)
		return 0;

	{
		char argv0[20];
		char argv1[20];

		snprintf(argv0, sizeof(argv0), "test-execve");
		snprintf(argv1, sizeof(argv1), "%d", count - 1);

		char *argv[] { argv0, argv1, NULL };

		execve("test-execve", argv, NULL);
	}

	printf("This code should never be reached.\n");
	return -1;
}
