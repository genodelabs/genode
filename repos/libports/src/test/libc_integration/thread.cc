/*
 * \brief  worker thread
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

/* stdcxx includes */
#include <vector>

/* local includes */
#include "thread.h"
#include "fd_set.h"


/*
 * Thread-function for the Test_worker.
 */
void *worker_func(void *ptr)
{
	using namespace Integration_test;
	using namespace std;

	Work_info       work_info     { *reinterpret_cast<Work_info*>(ptr) };

	int const       max_fd        { work_info.pipe_in_fd };
	auto const      count         { max(1ul, work_info.num_bytes * 1024 / IN_DATA_SIZE) };
	size_t          bytes_read    { 0 };
	size_t          bytes_written { 0 };
	vector<uint8_t> data_out      { };

	while (bytes_read < IN_DATA_SIZE) {

		/* read data from input pipe */
		fd_set fds { };
		FD_ZERO(&fds);
		FD_SET(work_info.pipe_in_fd, &fds);
		auto num_ready { select(max_fd+1, &fds, nullptr, nullptr, nullptr) };

		if (num_ready < 0) {
			error("error: worker ", work_info.worker_no, " select failed");
			exit(-6);
		}

		char buf[1024] { };
		auto r_res     { read(work_info.pipe_in_fd, buf, sizeof(buf)) };
		if (r_res < 0) {
			error("error: worker ", work_info.worker_no, " read failed");
			exit(-7);
		}

		bytes_read += r_res;

		for (size_t i=0; i<count && data_out.size() < work_info.num_bytes; ++i) {
			data_out.push_back(buf[random()%r_res]);
		}

		if (data_out.size() < bytes_written) {
			error("error: worker ", work_info.worker_no, " unexpected state");
			exit(-8);
		}

		/* write part of response */
		size_t  cnt   { min(static_cast<size_t>(work_info.buffer_size),
		                    data_out.size() - bytes_written) };
		ssize_t w_res { write(work_info.pipe_out_fd,
		                      data_out.data()+bytes_written,
		                      cnt) };

		if (w_res < 0) {
			error("error: worker ", work_info.worker_no, " write failed ----", work_info.num_bytes-bytes_written,"--",work_info.pipe_out_fd);
			exit(-8);
		}

		bytes_written += w_res;

		/*
		 * Get out early when the expected num_bytes got written already.
		 * The receiver will pthread_join this thread and wait for.
		 */
		if (bytes_written >= work_info.num_bytes) {
			return nullptr;
		}

		if (bytes_written >= data_out.size()) {
			break;
		}
	}

	/* ensure enough data is present */
	while (data_out.size() < static_cast<size_t>(work_info.num_bytes)) {
		data_out.push_back(data_out[random()%data_out.size()]);
	}

	/* simulate output creation requiring some time */
	usleep(1000ull*random()%300);

	/* write remaining output bytes */
	while (bytes_written < data_out.size()) {

		size_t  cnt   { min(static_cast<size_t>(work_info.buffer_size),
		                    data_out.size() - bytes_written) };

		ssize_t w_res { write(work_info.pipe_out_fd,
		                      data_out.data()+bytes_written, cnt) };

		if(w_res < 0) {
			error("error: worker ", work_info.worker_no, " write failed");
			exit(-9);
		}

		bytes_written += w_res;
	}

	return nullptr;
}


Integration_test::Test_worker::Test_worker(size_t num_bytes, size_t worker_no, size_t buffer_size)
:
	_work_info { .num_bytes    = num_bytes,
	             .worker_no    = worker_no,
	             .buffer_size  = buffer_size,
	             .pipe_in_fd   = _pipe_in.read_fd(),
	             .pipe_out_fd  = _pipe_out.write_fd() }
{
	pthread_create(&_thread, nullptr, worker_func, &_work_info);
}
