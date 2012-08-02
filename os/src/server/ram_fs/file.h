/*
 * \brief  File node
 * \author Norman Feske
 * \date   2012-04-11
 */

#ifndef _FILE_H_
#define _FILE_H_

/* Genode includes */
#include <base/allocator.h>

/* local includes */
#include <node.h>
#include <chunk.h>

namespace File_system {

	class File : public Node
	{
		private:

			typedef Chunk<4096>                     Chunk_level_3;
			typedef Chunk_index<128, Chunk_level_3> Chunk_level_2;
			typedef Chunk_index<64,  Chunk_level_2> Chunk_level_1;
			typedef Chunk_index<64,  Chunk_level_1> Chunk_level_0;

			Chunk_level_0 _chunk;

			file_size_t _length;

		public:

			File(Allocator &alloc, char const *name)
			: _chunk(alloc, 0), _length(0) { Node::name(name); }

			size_t read(char *dst, size_t len, seek_off_t seek_offset)
			{
				file_size_t const chunk_used_size = _chunk.used_size();

				if (seek_offset >= _length)
					return 0;

				/*
				 * Constrain read transaction to available chunk data
				 *
				 * Note that 'chunk_used_size' may be lower than '_length'
				 * because 'Chunk' may have truncated tailing zeros.
				 */
				if (seek_offset + len >= _length)
					len = _length - seek_offset;

				file_size_t read_len = len;

				if (seek_offset + read_len > chunk_used_size) {
					if (chunk_used_size >= seek_offset)
						read_len = chunk_used_size - seek_offset;
					else
						read_len = 0;
				}

				_chunk.read(dst, read_len, seek_offset);

				/* add zero padding if needed */
				if (read_len < len)
					memset(dst + read_len, 0, len - read_len);

				return len;
			}

			size_t write(char const *src, size_t len, seek_off_t seek_offset)
			{
				if (seek_offset == (seek_off_t)(~0))
					seek_offset = _chunk.used_size();

				if (seek_offset + len >= Chunk_level_0::SIZE)
					throw Size_limit_reached();

				_chunk.write(src, len, (size_t)seek_offset);

				/*
				 * Keep track of file length. We cannot use 'chunk.used_size()'
				 * as file length because trailing zeros may by represented
				 * by zero chunks, which do not contribute to 'used_size()'.
				 */
				_length = max(_length, seek_offset + len);
				return 0;
			}

			file_size_t length() const { return _length; }

			void truncate(file_size_t size)
			{
				if (size < _chunk.used_size())
					_chunk.truncate(size);
				else
					_length = size;
			}
	};
}

#endif /* _FILE_H_ */
