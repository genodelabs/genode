/*
 * \brief  Component providing a Terminal session via SSH
 * \author Josef Soentgen
 * \author Pirmin Duss
 * \date   2019-05-29
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SSH_TERMINAL_UTIL_H_
#define _SSH_TERMINAL_UTIL_H_

/* Genode includes */
#include <util/string.h>
#include <libc/component.h>

/* libc includes */
#include <pthread.h>
#include <unistd.h>
#include <time.h>


namespace Util
{
	using Filename = Genode::String<256>;

	template <size_t C>
	struct Buffer;

	/*
	 * get the current time from the libc backend.
	 */
	char const *get_time();

	struct Pthread_mutex;
}


struct Util::Pthread_mutex
{
	public:

		class Guard
		{
			private:

				Pthread_mutex &_mutex;

			public:

				explicit Guard(Pthread_mutex &mutex) : _mutex(mutex) { _mutex.lock(); }

				~Guard() { _mutex.unlock(); }
		};

	private:

		pthread_mutex_t _mutex;

	public:

		Pthread_mutex() { pthread_mutex_init(&_mutex, nullptr); }

		~Pthread_mutex() { pthread_mutex_destroy(&_mutex); }

		void lock()   { pthread_mutex_lock(&_mutex); }
		void unlock() { pthread_mutex_unlock(&_mutex); }
};


template <size_t C>
struct Util::Buffer
{
	Util::Pthread_mutex _mutex   { };
	char                _data[C] { };
	size_t              _head    { 0 };
	size_t              _tail    { 0 };

	size_t read_avail()   const { return _head > _tail ? _head - _tail : 0; }
	size_t write_avail()  const { return _head <= C ? C - _head : 0; }
	char const *content() const { return &_data[_tail]; }

	void append(char c)    { _data[_head++] = c; }
	void consume(size_t n) { _tail += n; }
	void reset()           { _head = _tail = 0; }

	Util::Pthread_mutex &mutex() { return _mutex; }
};

#endif /* _SSH_TERMINAL_UTIL_H_ */
