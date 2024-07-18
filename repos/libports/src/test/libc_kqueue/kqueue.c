/*
 * \brief  kqueue test
 * \author Benjamin Lamowski
 * \date   2024-08-20
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/*
 * Note that our kqueue implementation does support EV_CLEAR
 * for EVFILT_READ and EVFILT_WRITE,
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/event.h>

/*
 * Information about the test.
 */
struct Test_info {
	char const *name; /* Name of the test */
	char const *path; /* Path of the file to open  */
	int open_flags;   /* Flags for opening the file */
	int filter;       /* kqueue filter */
	int flags;        /* kqueue flags */
};


/*
 * Structure to pass around open file descriptors.
 */
struct Fildes {
	int fd;
	int kq;
};


/*
 * Set up a kqueue, open the file and insert the initial kevent.
 */
int bringup(struct Test_info *test, struct Fildes *fildes)
{
	int ret = 0;
	struct kevent event;

	fildes->kq = kqueue();
	if (fildes->kq == -1) {
		printf("%s: Failed to create kqueue: %s\n", test->name,
		       strerror(errno));
		return ret;
	}


	fildes->fd = open(test->path, test->open_flags);

	if (fildes->fd == -1) {
		close(fildes->kq);
		printf("%s: Failed to open file %s: %s\n", test->name,
		       test->path, strerror(errno));

		return ret;
	}

	EV_SET(&event, fildes->fd, test->filter, test->flags, 0, 0, NULL);

	ret = kevent(fildes->kq, &event, 1, NULL, 0, NULL);
	if (ret) {
		close(fildes->fd);
		close(fildes->kq);
		printf("%s: Failed to register event: %s\n",
		       test->name, strerror(errno));
		return ret;
	}

	return 0;
}


/*
 * Get a result from the kqueue and close the file descriptors.
 */
int get_result(struct Test_info *test, struct Fildes *fildes)
{
	int ret = 0;
	struct kevent result;
	ret = kevent(fildes->kq, NULL, 0, &result, 1, NULL);
	if (ret == -1) {
		close(fildes->fd);
		close(fildes->kq);
		printf("%s: Failed to retrieve result: %s\n", test->name,
		       strerror(errno));
		return ret;
	}

	close(fildes->fd);
	close(fildes->kq);

	if (ret > 0) {
		if (result.flags & EV_ERROR) {
			printf("%s: ", test->name);
			printf("%s: Event indcated failure: %s\n", test->name,
			       strerror(result.data));
			return -1;
		} else {
			printf("%s: Test successful.\n", test->name);
			return 0;
		}
	}

	return 0;
}


/*
 * Test getting a kevent if a file can be written to.
 */
int test_write()
{
	int ret = 0;
	struct Test_info test = {
		"Write test",
		"/dev/log",
		O_WRONLY,
		EVFILT_WRITE,
		EV_ADD
	};
	struct Fildes fildes;
	ret = bringup(&test, &fildes);
	if (ret)
		return ret;

	return get_result(&test, &fildes);
}


/*
 * Test getting a kevent if a file can be read.
 */
int test_read()
{
	int ret = 0;
	struct Test_info test = {
		"Read test",
		"/dev/rtc",
		O_RDONLY,
		EVFILT_READ,
		EV_ADD
	};
	struct Fildes fildes;
	ret = bringup(&test, &fildes);
	if (ret)
		return ret;

	return get_result(&test, &fildes);
}


/*
 * Test deleting a queued kevent.
 */
int test_delete()
{
	int ret = 0;
	struct kevent event;
	struct Test_info test = {
		"Cancel test",
		"/dev/rtc",
		O_RDONLY,
		EVFILT_READ,
		EV_ADD
	};
	struct Fildes fildes;
	const struct timespec timeout = {.tv_sec = 0, .tv_nsec = 1000000};
	ret = bringup(&test, &fildes);
	if (ret)
		return ret;

	/* Delete the event */
	EV_SET(&event, fildes.fd, test.filter, EV_DELETE, 0, 0, NULL);
	ret = kevent(fildes.kq, &event, 1, NULL, 0, NULL);
	if (ret) {
		close(fildes.fd);
		close(fildes.kq);
		printf("%s: Failed to delete event: %s\n", test.name,
		       strerror(errno));

		return ret;
	}

	ret = kevent(fildes.kq, NULL, 0, &event, 1, &timeout);
	if (ret == -1) {
		close(fildes.fd);
		close(fildes.kq);
		printf("%s: Failed to retrieve result: %s\n", test.name,
		       strerror(errno));

		return ret;
	}

        /*
         * Since we can always read /dev/rtc, wetting the results will only
         * timeout if the event has been deleted sucessfuly.
         */
        if (ret == 0) {
			printf("%s: Test successful.\n", test.name);
			return 0;
	} else {
		printf("%s: Event was not deleted: %s\n", test.name,
		       strerror(errno));

		return -1;
	}
}


/*
 * Test repeating a non-oneshot event.
 */
int test_repeat()
{
	int ret = 0;
	struct Test_info test = {
		"Repeat test",
		"/dev/rtc",
		O_RDONLY,
		EVFILT_READ,
		EV_ADD
	};
	struct Fildes fildes;
	struct kevent event1, event2;

	ret = bringup(&test, &fildes);
	if (ret)
		return ret;

	ret = kevent(fildes.kq, NULL, 0, &event1, 1, NULL);
	if (ret == -1) {
		close(fildes.fd);
		close(fildes.kq);
		printf("%s: Failed to retrieve result 1: %s\n", test.name,
		       strerror(errno));

		return ret;
	}

	ret = kevent(fildes.kq, NULL, 0, &event2, 1, NULL);

	close(fildes.fd);
	close(fildes.kq);

	switch (ret) {
		case 1:
			printf("%s: Test successful.\n", test.name);
			return 0;
		case -1:
			printf("%s: Failed to retrieve result 2: %s\n",
			       test.name, strerror(errno));
			break;
		default:
			printf("%s: Non-oneshot event was not repeated: %s\n",
			       test.name, strerror(errno));
			return -1;
	}

	return ret;
}


/*
 * Test queuing a oneshot event.
 */
int test_oneshot()
{
	int ret = 0;
	struct Test_info test = {
		"Oneshot test",
		"/dev/rtc",
		O_RDONLY,
		EVFILT_READ,
		EV_ADD | EV_ONESHOT
	};
	struct Fildes fildes;
	const struct timespec timeout = {.tv_sec = 0, .tv_nsec = 1000000};
	struct kevent event1, event2;

	ret = bringup(&test, &fildes);
	if (ret)
		return ret;

	ret = kevent(fildes.kq, NULL, 0, &event1, 1, NULL);
	if (ret == -1) {
		close(fildes.fd);
		close(fildes.kq);
		printf("%s: Failed to retrieve result 1: %s\n", test.name,
		       strerror(errno));
		return ret;
	}

	ret = kevent(fildes.kq, NULL, 0, &event2, 1, &timeout);
	if (ret == -1) {
		close(fildes.fd);
		close(fildes.kq);
		printf("%s: Failed to retrieve result 2: %s\n", test.name,
		       strerror(errno));
		return ret;
	}

	if (ret == 0) {
			printf("%s: Test successful.\n", test.name);
			return 0;
	} else {
		printf("%s: Oneshot event was repeated: %s\n", test.name,
		       strerror(errno));

		return -1;
	}
}


/*
 * Test disabling a queued event.
 */
int test_disable()
{
	int ret = 0;
	struct Test_info test = {
		"Disable test",
		"/dev/rtc",
		O_RDONLY,
		EVFILT_READ,
		EV_ADD
	};
	struct Fildes fildes;
	const struct timespec timeout = {.tv_sec = 0, .tv_nsec = 1000000};
	struct kevent event;

	ret = bringup(&test, &fildes);
	if (ret)
		return ret;

	/* Disable the event */
	EV_SET(&event, fildes.fd, test.filter, EV_DISABLE, 0, 0, NULL);
	ret = kevent(fildes.kq, &event, 1, NULL, 0, NULL);
	if (ret) {
		close(fildes.fd);
		close(fildes.kq);
		printf("%s: Failed to disable event: %s\n", test.name,
		       strerror(errno));
		return ret;
	}

	ret = kevent(fildes.kq, NULL, 0, &event, 1, &timeout);

	close(fildes.fd);
	close(fildes.kq);

	switch (ret) {
		case 0:
			printf("%s: Test successful.\n", test.name);
			return 0;
		case -1:
			printf("%s: Failed to retrieve result: %s\n", test.name,
			       strerror(errno));
			return -1;
		default:
			printf("%s: Event was not disabled: %s\n", test.name,
			       strerror(errno));
			return -1;
	}
}


/*
 * Test (re-)enabling a disabled event.
 */
int test_enable()
{
	int ret = 0;
	struct Test_info test = {
		"Enable test",
		"/dev/rtc",
		O_RDONLY,
		EVFILT_READ,
		EV_ADD
	};
	struct Fildes fildes;
	const struct timespec timeout = {.tv_sec = 0, .tv_nsec = 1000000};
	struct kevent event;

	ret = bringup(&test, &fildes);
	if (ret)
		return ret;

	/* Disable the event */
	EV_SET(&event, fildes.fd, test.filter, EV_DISABLE, 0, 0, NULL);
	ret = kevent(fildes.kq, &event, 1, NULL, 0, NULL);
	if (ret) {
		close(fildes.fd);
		close(fildes.kq);
		printf("%s: Failed to disable event: %s\n", test.name,
		       strerror(errno));

		return ret;
	}

	ret = kevent(fildes.kq, NULL, 0, &event, 1, &timeout);
	if (ret == -1) {
		close(fildes.fd);
		close(fildes.kq);
		printf("%s: Failed to retrieve result 2: %s\n", test.name,
		       strerror(errno));
		return ret;
	}

	if (ret != 0) {
		printf("%s: Event was not disabled: %s\n", test.name,
		       strerror(errno));
		return -1;
	}

	/* (Re-)Enable the event */
	EV_SET(&event, fildes.fd, test.filter, EV_ENABLE, 0, 0, NULL);
	ret = kevent(fildes.kq, &event, 1, NULL, 0, NULL);
	if (ret) {
		close(fildes.fd);
		close(fildes.kq);
		printf("%s: Failed to enable event: %s\n", test.name,
		       strerror(errno));
		return ret;
	}

	return get_result(&test, &fildes);
}


/*
 * Test queuing a disabled event.
 */
int test_queue_disabled()
{
	int ret = 0;
	struct Test_info test = {
		"Queue disabled test",
		"/dev/rtc",
		O_RDONLY,
		EVFILT_READ,
		EV_ADD | EV_DISABLE
	};
	struct Fildes fildes;
	const struct timespec timeout = {.tv_sec = 0, .tv_nsec = 1000000};
	struct kevent event;

	ret = bringup(&test, &fildes);
	if (ret)
		return ret;

	ret = kevent(fildes.kq, NULL, 0, &event, 1, &timeout);

	close(fildes.fd);
	close(fildes.kq);

	switch (ret) {
		case 0:
			printf("%s: Test successful.\n", test.name);
			return 0;
		case -1:
			printf("%s: Failed to retrieve result: %s\n", test.name,
			       strerror(errno));
			return -1;
		default:
			printf("%s: Event was not disabled: %s\n", test.name,
			       strerror(errno));
			return -1;
	}
}


int main(int argc, char **argv)
{
	int retval = 0;

	retval += test_write();
	retval += test_read();
	retval += test_delete();
	retval += test_repeat();
	retval += test_oneshot();
	retval += test_disable();
	retval += test_enable();
	retval += test_queue_disabled();

	if (!retval)
		printf("--- test succeeded ---\n");

	return retval;
}
