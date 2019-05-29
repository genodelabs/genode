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
}


template <size_t C>
struct Util::Buffer
{
	Genode::Lock _lock    { };
	char         _data[C] { };
	size_t       _head    { 0 };
	size_t       _tail    { 0 };

	size_t read_avail()   const { return _head > _tail ? _head - _tail : 0; }
	size_t write_avail()  const { return _head <= C ? C - _head : 0; }
	char const *content() const { return &_data[_tail]; }

	void append(char c)    { _data[_head++] = c; }
	void consume(size_t n) { _tail += n; }
	void reset()           { _head = _tail = 0; }

	Genode::Lock &lock()   { return _lock; }
};

#endif /* _SSH_TERMINAL_UTIL_H_ */
