/*
 * \brief  Libc alarm test
 * \author Norman Feske
 * \date   2024-08-30
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <signal.h>
#include <unistd.h>
#include <stdio.h>

static unsigned triggered_alarms;

static void sigalarm_handler(int)
{
	triggered_alarms++;
}

int main(int, char **)
{
	static struct sigaction sa;
	sa.sa_handler = sigalarm_handler;

	int ret = sigaction(SIGALRM, &sa, NULL);
	if (ret < 0) {
		printf("sigaction unexpectedly returned %d\n", ret);
		return 1;
	}

	signal(SIGALRM, sigalarm_handler);

	unsigned observed_alarms = 0;

	alarm(2);

	while (observed_alarms != 3) {
		sleep(1);
		printf("triggered_alarms=%u\n", triggered_alarms);

		if (triggered_alarms != observed_alarms) {
			observed_alarms = triggered_alarms;
			alarm(2);
		}
	}
	return 0;
}
