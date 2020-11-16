/*
 * \brief  Integration test for interplay of the following
 *         features:
 *           - libc
 *           - pthread
 *           - vfs_pipe
 * \author Pirmin Duss
 * \date   2020-11-11
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 * Copyright (C) 2020 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */


/* libc includes */
#include <string.h>
#include <errno.h>
#include <sys/select.h>

/* stdcxx includes */
#include <vector>

/* local includes */
#include "definitions.h"
#include "fd_set.h"
#include "input_sender.h"
#include "stdcxx_log.h"
#include "thread.h"


namespace Integration_test
{
	using namespace std;

	class Main;

	struct Thread_data;
}


struct Integration_test::Thread_data
{
	size_t max_workers;
	size_t parallel_workers;
	size_t buffer_size;
	size_t write_size;
};


/*
 * Thread-function that runs the test.
 */
void *test_runner(void *arg)
{
	using namespace Integration_test;
	using namespace std;

	enum { OUTPUT_REDUCTION_FACTOR = 100 };

	Thread_data  data             { *reinterpret_cast<Thread_data*>(arg) };
	Input_sender sender           { };
	Thread_list  threads          { data.buffer_size };
	size_t       threads_started  { 0 };

	for (size_t thread=0; thread<data.parallel_workers; ++thread) {
		auto handle { threads.add_worker() };
		sender.add_worker(handle.first, handle.second);
		++threads_started;
	}

	while (threads_started < data.max_workers) {

		static size_t cnt { 0 };
		if ((cnt++ % OUTPUT_REDUCTION_FACTOR) == 0) {
			log((threads_started - threads.count()), "  workers finished, ",
			    threads.count(), " currently running");
		}

		auto fds { threads.fds() };
		auto num_ready = select(threads.max_fd()+1, &fds, nullptr, nullptr, nullptr);

		if (num_ready == -1) {
			error("select() failed with '", strerror(errno), "'");
			exit(-1);
		}

		vector<size_t> finished         { };
		for (auto fd : threads.descriptor_set()) {

			if (!FD_ISSET(fd, &fds))
					continue;

			auto it = threads.find_worker_by_fd(fd);

			if (it == threads.end()) {
				error("worker not found");
				exit(-2);
			}

			uint8_t buf[16*1024] { };
			auto res { read(fd, buf, sizeof(buf)) };
			if (res < 0) {
				error("read error: fd=", fd);
			} else {
				it->append_result_data(buf, res);
			}

			if (it->done()) {
				finished.push_back(it->worker_no());
			}
		}

		sender.remove_finished_workers(finished);
		threads.remove_finished_workers(finished);

		/* restart more threads when some threads are finished */
		while (threads.count() < data.parallel_workers && threads_started < data.max_workers) {
			auto handle { threads.add_worker() };
			sender.add_worker(handle.first, handle.second);
			++threads_started;
		}
	}

	log("--- test finished ---");
	exit(0);

	return 0;
}


size_t get_param_by_name(const char *name, int argc, const char *argv[], size_t not_found_value)
{
	using namespace Integration_test;

	for (int i=1; i<argc; ++i) {

		if (strcmp(name, argv[i]) == 0) {

			size_t        res { not_found_value };
			stringstream  str { argv[i+1] };
			str >> res;
			return res;
		}
	}

	return not_found_value;
}


int main(int argc, char *argv[])
{
	using namespace Integration_test;

	Thread_data data {
	                   get_param_by_name("-wo", argc, const_cast<const char**>(argv), NUMBER_OF_WORKERS),
	                   get_param_by_name("-pw", argc, const_cast<const char**>(argv), PARALLEL_WORKERS),
	                   get_param_by_name("-ws", argc, const_cast<const char**>(argv), WRITE_SIZE),
	                   get_param_by_name("-ds", argc, const_cast<const char**>(argv), IN_DATA_SIZE)
	                 };
	log("number of workers  (-wo)  : ", data.max_workers);
	log("parallel workers   (-pw)  : ", data.parallel_workers);
	log("write size         (-ws)  : ", data.write_size);
	log("data size          (-ds)  : ", data.buffer_size);
	pthread_t   thr;

	pthread_create(&thr, nullptr, test_runner, &data);

	pthread_join(thr, nullptr);
}
