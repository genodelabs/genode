/*
 * \brief  Noux SIGINT handler test
 * \author Christian Prochaska
 * \date   2013-10-17
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

void signal_handler(int signal)
{
	printf("%d: signal handler for signal %d called\n",
	       getpid(), signal);
}

int main(int argc, char *argv[])
{
	char c;

	struct sigaction sa;

	memset (&sa, '\0', sizeof(sa));

	sa.sa_handler = signal_handler;

	sigaction(SIGINT, &sa, 0);

	int pid = fork();

	if (pid == 0)
		printf("test ready\n");

	if ((read(0, &c, 1) == -1) && (errno = EINTR))
		printf("%d: 'read()' returned with error EINTR\n", getpid());
	else
		printf("%d: 'read()' returned character 0x = %x\n", getpid(), c);

	if (pid > 0) {
		waitpid(pid, 0, 0);
		printf("test finished\n");
	}

	return 0;
}

