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

#ifndef INTEGRATION_TEST_THREAD_H
#define INTEGRATION_TEST_THREAD_H


/* stdcxx includes */
#include <algorithm>
#include <list>
#include <memory>
#include <random>
#include <sstream>
#include <vector>

/* local includes */
#include "fd_set.h"
#include "pipe.h"
#include "stdcxx_log.h"
#include "definitions.h"


namespace Integration_test
{
	using namespace std;

	using Worker_handle = pair<size_t, int>;

	struct Work_info
	{
		size_t  num_bytes;
		size_t  worker_no;
		size_t  buffer_size;
		int     pipe_in_fd;
		int     pipe_out_fd;
	};

	class Thread;
	class Test_worker;
	class Thread_list;

	using Work_iter  = std::list<Test_worker>::iterator;

	/*
	 * abstract away difference in libc pthread_t definition
	 * between Genode (BSD libc) and Linux.
	 */
	#if defined (__GENODE__)
		#define THREAD_INIT  nullptr
	#else
		#define THREAD_INIT  0
	#endif
}


class Integration_test::Test_worker
{
	private:

		Pipe             _pipe_in        { };
		Pipe             _pipe_out       { };
		Work_info        _work_info;
		pthread_t        _thread         { THREAD_INIT };
		vector<uint8_t>  _result_data    { };

		void _print_data() const
		{
			ostringstream str { };

			str << std::hex << (int)_result_data[0] << " " <<
			                   (int)_result_data[1] << " " <<
			                   (int)_result_data[2] << " " <<
			       std::dec << " ... (" << _result_data.size()-6 << " bytes) ... " <<
			       std::hex << (int)_result_data[_result_data.size()-3] << " " <<
			                   (int)_result_data[_result_data.size()-2] << " " <<
			                   (int)_result_data[_result_data.size()-1];

			log("Worker ", worker_no(), " data : ", str.str());
		}

		Test_worker(const Integration_test::Test_worker&) = delete;
		Test_worker &operator=(const Integration_test::Test_worker&) = delete;

	public:

		Test_worker(size_t num_bytes, size_t worker_no, size_t buffer_size);

		~Test_worker()
		{
			join();
			_print_data();
		}

		void    join()            const { pthread_join(_thread, nullptr); }
		size_t  worker_no()       const { return _work_info.worker_no; }
		ssize_t number_of_bytes() const { return _work_info.num_bytes; }
		int     read_fd()         const { return _pipe_out.read_fd(); }
		int     write_fd()        const { return _pipe_in.write_fd(); }

		bool    done()            const { return _result_data.size() >=
		                                         _work_info.num_bytes; }

		size_t missing()          const { return _work_info.num_bytes - _result_data.size(); }

		void append_result_data(uint8_t const *buf, size_t count)
		{
			_result_data.insert(_result_data.end(), buf, buf+count);
		}
};


class Integration_test::Thread_list
{
	private:

		size_t                           _buffer_size { };
		list<Test_worker>                _threads     { };
		File_descriptor_set              _fd_set      { };
		random_device                    _rd          { };
		mt19937                          _gen         { _rd() };
		uniform_int_distribution<size_t> _distrib     { 1, BUFFER_SIZE };

		Work_iter _find_worker_by_worker_no(size_t worker_no)
		{
			return find_if(_threads.begin(),
			               _threads.end(),
			               [worker_no] (Test_worker const &e) {
			                  return worker_no == e.worker_no(); });
		}

	public:

		Thread_list(size_t buffer_size)
		:
			_buffer_size { buffer_size }
		{ }

		fd_set fds()           { return _fd_set.fds(); }
		int     max_fd() const { return _fd_set.max_fd(); }
		size_t  count()  const { return _threads.size(); }

		Work_iter end()
		{
			return _threads.end();
		}

		Work_iter find_worker_by_fd(int fd)
		{
			return find_if(_threads.begin(),
			               _threads.end(),
			               [fd] (Test_worker const &e) {
			                  return fd == e.read_fd(); });
		}

		File_descriptor_set &descriptor_set()
		{
			return _fd_set;
		}

		Worker_handle add_worker()
		{

			static size_t worker_no { 0 };
			size_t const num_bytes { _distrib(_gen) };

			_threads.emplace_back(num_bytes, worker_no, _buffer_size);

			/* manage fd_set for receiver thread */
			_fd_set.add_fd(_threads.back().read_fd());

			return { worker_no++, _threads.back().write_fd() };
		}

		void remove_finished_workers(vector<size_t> &workers)
		{
			for (auto no : workers) {
				auto it { _find_worker_by_worker_no(no) };
				if (it == _threads.end()) {
					continue;
				}

				/* manage fd_set for receiver thread */
				_fd_set.remove_fd(it->read_fd());

				_threads.erase(it);
			}
		}
};


#endif /* INTEGRATION_TEST_THREAD_H */
