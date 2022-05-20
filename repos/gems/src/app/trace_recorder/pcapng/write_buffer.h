/*
 * \brief  Convenience helper for batching pcapng blocks before writing to file
 * \author Johannes Schlatow
 * \date   2022-05-16
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PCAPNG__WRITE_BUFFER_H_
#define _PCAPNG__WRITE_BUFFER_H_

/* Genode includes */
#include <util/attempt.h>
#include <os/vfs.h>

namespace Pcapng
{
	using namespace Genode;

	template <unsigned>
	class Write_buffer;
}

template <unsigned BUFSIZE>
class Pcapng::Write_buffer
{
	public:

		enum class Append_error  { OUT_OF_MEM, OVERFLOW };
		struct Append_ok { };
		using Append_result  = Attempt<Append_ok, Append_error>;

	private:

		size_t _total_length    { 0 };
		char   _buffer[BUFSIZE] { };

	public:

		template <typename T, typename... ARGS>
		Append_result append(ARGS &&... args)
		{
			if (T::MAX_SIZE > BUFSIZE || _total_length > BUFSIZE - T::MAX_SIZE)
				return Append_error::OUT_OF_MEM;

			void *ptr = &_buffer[_total_length];

			T const &block = *construct_at<T>(ptr, args...);

			if (block.size() > T::MAX_SIZE) {
				error("block size of ", block.size(), " exceeds reserved size ", (unsigned)T::MAX_SIZE);
				return Append_error::OVERFLOW;
			}

			_total_length += block.size();

			return Append_ok();
		}

		void write_to_file(Genode::New_file &dst, Directory::Path const &path)
		{
			if (_total_length == 0)
				return;

			if (dst.append(_buffer, _total_length) != New_file::Append_result::OK)
				error("Write error for ", path);

			clear();
		}

		void clear()  { _total_length = 0; }
};

#endif /* _PCAPNG__WRITE_BUFFER_H_ */
