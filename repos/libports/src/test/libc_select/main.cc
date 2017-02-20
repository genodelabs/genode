/*
 * \brief  Test select() in libc
 * \author Christian Helmuth
 * \date   2017-01-13
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* libc includes */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>

/* stdcxx includes */
#include <list>
#include <vector>
#include <string>
#include <sstream>
#include <mutex>
#include <algorithm>  /* std::max() */
#include <iostream>


struct Mutex_guard
{
	pthread_mutex_t &mutex;

	Mutex_guard(pthread_mutex_t &mutex) : mutex(mutex)
	{
		pthread_mutex_lock(&mutex);
	}

	~Mutex_guard()
	{
		pthread_mutex_unlock(&mutex);
	}
};


template <typename TAIL>
static void output(std::ostream &os, TAIL && tail)
{
	os << tail;
}


template <typename HEAD, typename... TAIL>
static void output(std::ostream &os, HEAD && head, TAIL &&... tail)
{
	output(os, head);
	output(os, tail...);
}


static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

template <typename... ARGS>
static void log(ARGS &&... args)
{
	Mutex_guard guard(log_mutex);

	output(std::cout, args...);
	std::cout << std::endl;
}


static void die(char const *token) __attribute__((noreturn));
static void die(char const *token)
{
	log("Error: ", token, ": ", strerror(errno));
	exit(1);
}


struct File
{
	char const *name;
	int         fd;

	File(char const *name, int fd) : name(name), fd(fd) { }
};


struct File_set : std::list<File>
{
	private:

		unsigned const _id;
		fd_set         _fds;
		int            _max_fd = 0;
		int            _pipe_fd[2];

	public:

		File_set(unsigned id, std::vector<char const *> const &args) : _id(id)
		{
			FD_ZERO(&_fds);

			/* add a pipe first */
			int const ret = pipe(_pipe_fd);
			if (ret == -1)
				die("pipe");

			std::list<File>::push_back(File("pipe_out", _pipe_fd[0]));
			std::list<File>::push_back(File("pipe_in",  _pipe_fd[1]));
			FD_SET(_pipe_fd[0], &_fds);
			FD_SET(_pipe_fd[1], &_fds);
			_max_fd = std::max(_max_fd, std::max(_pipe_fd[0], _pipe_fd[1]));

			log("[", id, "] _pipe_fd={", _pipe_fd[0], ",", _pipe_fd[1], "}");

			for (char const * const name : args) {

				int const fd = open(name, O_RDWR | O_NONBLOCK);
				if (fd == -1)
					die("open");

				std::list<File>::push_back(File(name, fd));
				FD_SET(fd, &_fds);
				_max_fd = std::max(_max_fd, fd);

				log("[", id, "] name=", name, " fd=", fd);
			}

			log("[", id, "] _max_fd=", _max_fd);
		}

		unsigned id()       const { return _id; }
		fd_set const &fds() const { return _fds; }
		int max_fd()        const { return _max_fd; }
		int pipe_in()       const { return _pipe_fd[1]; }
};


enum { TEST_ROUNDS = 32, PIPE_ROUNDS = TEST_ROUNDS - 2 };


static void * test(void *arg)
{
	File_set const &file_set = *(File_set const *)arg;

	std::string const label = "[" + std::to_string(file_set.id()) + "] ";

	int max_ready = 0;
	for (unsigned r = 0; r < TEST_ROUNDS; ++r) {
		log(label, "ROUND ", r);

		fd_set read_fds = file_set.fds();

		timeval tv { 2, 0 };

		int num_ready = select(file_set.max_fd() + 1, &read_fds, nullptr, nullptr, &tv);
		if (num_ready == -1)
			die("select");

		if (!num_ready) {
			log(label, "timeout");
			continue;
		}

		max_ready = std::max(max_ready, num_ready);

		log(label, "num_ready=", num_ready);
		for (int i = 0; num_ready && i <= file_set.max_fd(); ++i) {
			if (!FD_ISSET(i, &read_fds))
				continue;

			--num_ready;

			char b[4];
			int ret = read(i, b, sizeof(b));
			if (ret == -1)
				die("read");

			log(label, "read ", ret, " bytes from ", i);
		}
	}

	log(label, "max_ready=", max_ready);
	return nullptr;
}


static void * pipe_test(void *arg)
{
	std::vector<int> *fds = (std::vector<int> *)arg;

	log("starting pipe_test");

	for (int i = 0; i < PIPE_ROUNDS; ++i) {
		int      const t  { 2500*i/PIPE_ROUNDS + 100 }; /* ms */
		timespec const tv { t / 1000 , (t % 1000) * 1000000L };

		nanosleep(&tv, nullptr);

		int skip_nth = i % (fds->size() + 1);
		for (int fd : *fds)
			if (--skip_nth != 0)
				write(fd, "X", 1);
	}

	log("pipe_test done");
	return nullptr;
}


int main(int argc, char **argv)
{
	/* skip the program name */
	--argc; ++argv;

	/* arguments for the threads (thread 0 is main) */
	std::vector<std::vector<char const *>> thread_args;
	thread_args.push_back(std::vector<char const *>());

	unsigned thread_num = 0;
	for (char const *arg : std::vector<char const *>(argv, argv + argc)) {
		/* switch to next thread's arguments */
		if (std::string(":") == arg) {
			++thread_num;
			thread_args.push_back(std::vector<char const *>());
		} else {
			thread_args[thread_num].push_back(arg);
		}
	}

	std::vector<pthread_t> threads;
	std::vector<int>       pipe_in_fds;

	log("test with ", thread_args.size(), " threads");
	unsigned id = 0;
	for (std::vector<char const *> &args : thread_args) {
		if (id > 0) {
			pthread_t t;
			File_set *fs = new File_set(id, args);
			int const ret = pthread_create(&t, nullptr, test, fs);
			if (ret == -1)
				die("pthread_create");
			threads.push_back(t);
			pipe_in_fds.push_back(fs->pipe_in());
		}

		++id;
	}

	File_set file_set(0, thread_args[0]);

	pipe_in_fds.push_back(file_set.pipe_in());

	if (true) {
		pthread_t t;
		int ret = pthread_create(&t, nullptr, pipe_test, (void *)&pipe_in_fds);
		if (ret == -1)
			die("pthread_create");
	}

	test(&file_set);

	/* XXX pthread_join is currently not implemented */
	if (false)
		for (pthread_t t : threads) pthread_join(t, nullptr);
	else
		sleep(1);

	exit(0);
}
