/*
 * \brief  thread sends input date to workers.
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

#ifndef INTEGRATION_TEST_INPUT_SENDER_H
#define INTEGRATION_TEST_INPUT_SENDER_H


/* stdcxx includes */
#include <algorithm>
#include <chrono>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

/* libc includes */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* local includes */
#include "stdcxx_log.h"
#include "definitions.h"


namespace Integration_test
{
	using namespace std;

	struct Input_info
	{
		size_t   worker_no;
		ssize_t  num_bytes;
		size_t   bytes_sent;
		int      pipe_fd;

		Input_info(size_t no, int fd)
		:
			worker_no  { no },
			num_bytes  { IN_DATA_SIZE },
			bytes_sent { 0 },
			pipe_fd    { fd }
		{ }
	};

	class Runner;
	class Input_sender;
};


class Integration_test::Runner
{
	private:

		using Work_iter  = std::vector<Input_info>::iterator;

		mutex              _lock           { };
		vector<Input_info> _workers        { };
		vector<char>       _send_data;

		Work_iter _find_worker(size_t no)
		{
			return find_if(_workers.begin(),
			               _workers.end(),
			               [no] (Input_info const &e) { return no == e.worker_no; });
		}

	public:

		Runner()
		:
			_send_data ( static_cast<vector<char>::size_type>(IN_DATA_SIZE) )
		{
			random_device                  rd;
			mt19937                        gen     { rd() };
			uniform_int_distribution<char> distrib { ' ', '}' };

			for (auto &e : _send_data) {
				e = distrib(gen);
			}
		}

		void add_worker(size_t no, int fd)
		{
			lock_guard<mutex> guard { _lock };
			_workers.emplace_back(no, fd);
		}

		void remove_worker(size_t w)
		{
			lock_guard<mutex> guard { _lock };

			auto it { _find_worker(w) };
			if (it == _workers.end()) {
				return;
			}

			_workers.erase(it);
		}

		void run()
		{
			while (true)
			{
				{
					lock_guard<mutex> guard { _lock };
					for (auto &worker : _workers) {

						if (worker.bytes_sent < _send_data.size()) {

							size_t cnt    { min(static_cast<size_t>(WRITE_SIZE),
							                    _send_data.size()-worker.bytes_sent) };
							ssize_t w_res { write(worker.pipe_fd,
							                      _send_data.data()+worker.bytes_sent, cnt) };

							if(w_res < 0) {
								error("error: send data to worker ", worker.worker_no, " write failed");
								exit(-3);
							}

							worker.bytes_sent += w_res;
						}
					}
				}

				/* allow access to _workers from the outside */
				this_thread::sleep_for(chrono::milliseconds(50));
			}
		}
};


class Integration_test::Input_sender
{
	private:

		Runner     _runner { };
		thread     _thread;

		Input_sender(Input_sender  &) = delete;
		Input_sender(Input_sender &&) = delete;

	public:

		Input_sender()
		:
			_thread { &Runner::run, &_runner }
		{
		}

		void add_worker(size_t no, int fd)
		{
			_runner.add_worker(no, fd);
		}

		void remove_finished_workers(vector<size_t> &workers)
		{
			for (auto e : workers) {
				_runner.remove_worker(e);
			}
		}
};

#endif  /* INTEGRATION_TEST_INPUT_SENDER_H */
