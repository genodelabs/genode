/*
 * \brief  File node
 * \author Norman Feske
 * \author Josef Soentgen
 * \date   2012-04-11
 */

/*
 * Copyright (C) 2012-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _FILE_H_
#define _FILE_H_

/* Genode includes */
#include <base/allocator.h>
#include <base/trace/types.h>

/* local includes */
#include <node.h>
#include <chunk.h>

namespace File_system {

	/**
	 *
	 *
	 */
	class Changeable_content
	{
		protected:

			/**
			 * This member is used to communicate the state and
			 * must be set true by classes using this class in case
			 * the content has changed.
			 */
			bool _changed;

			/**
			 * This method is called when the content change is
			 * acknowledged. It may be overriden by any class using
			 * this particular class.
			 */
			virtual void _refresh_content() { }

		public:

			Changeable_content() : _changed(false) { }

			/**
			 * Check if the content was changed
			 *
			 * This evaluation has to be made by classes using this
			 * particular class.
			 *
			 * \return true if changed, otherwise false
			 */
			bool changed() const { return _changed; }


			/**
			 * Acknowledge the content has changed
			 */
			void acknowledge_change()
			{
				_changed = false;

				_refresh_content();
			}
	};


	/**
	 * File interface
	 */

	class File : public Node
	{
		public:

			File(char const *name)
			{
				Node::name(name);
			}

			virtual ~File() { }

			/********************
			 ** Node interface **
			 ********************/

			virtual size_t read(char *dst, size_t len, seek_off_t seek_offset) = 0;

			virtual size_t write(char const *src, size_t len, seek_off_t seek_offset) = 0;

			virtual Status status() const = 0;

			/********************
			 ** File interface **
			 ********************/

			virtual file_size_t length() const = 0;

			virtual void truncate(file_size_t size) = 0;
	};

	/**
	 * Memory buffered file
	 *
	 * This file merely exists in memory and grows automatically.
	 */

	class Buffered_file : public File
	{
		private:

			typedef Chunk<4096>                     Chunk_level_3;
			typedef Chunk_index<128, Chunk_level_3> Chunk_level_2;
			typedef Chunk_index<64,  Chunk_level_2> Chunk_level_1;
			typedef Chunk_index<64,  Chunk_level_1> Chunk_level_0;

			Chunk_level_0 _chunk;

			file_size_t _length;

		public:

			Buffered_file(Allocator &alloc, char const *name)
			: File(name), _chunk(alloc, 0), _length(0) { }

			virtual size_t read(char *dst, size_t len, seek_off_t seek_offset)
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

			virtual size_t write(char const *src, size_t len, seek_off_t seek_offset)
			{
				if (seek_offset == (seek_off_t)(~0))
					seek_offset = _chunk.used_size();

				if (seek_offset + len >= Chunk_level_0::SIZE) {
					len = (Chunk_level_0::SIZE-1) - seek_offset;
					Genode::error(name(), ": size limit ", (long)Chunk_level_0::SIZE, " reached");
				}

				_chunk.write(src, len, (size_t)seek_offset);

				/*
				 * Keep track of file length. We cannot use 'chunk.used_size()'
				 * as file length because trailing zeros may by represented
				 * by zero chunks, which do not contribute to 'used_size()'.
				 */
				_length = max(_length, seek_offset + len);

				mark_as_updated();
				return len;
			}

			virtual Status status() const
			{
				Status s;

				s.inode = inode();
				s.size  = _length;
				s.mode  = File_system::Status::MODE_FILE;

				return s;
			}

			virtual file_size_t length() const { return _length; }

			void truncate(file_size_t size)
			{
				if (size < _chunk.used_size())
					_chunk.truncate(size);

				_length = size;

				mark_as_updated();
			}
	};
}

#endif /* _FILE_H_ */
