/*
 * \brief  Data structure for storing sparse files in RAM
 * \author Norman Feske
 * \date   2012-04-18
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__RAM_FS__CHUNK_H_
#define _INCLUDE__RAM_FS__CHUNK_H_

/* Genode includes */
#include <util/noncopyable.h>
#include <base/allocator.h>
#include <util/string.h>
#include <file_system_session/file_system_session.h>

namespace File_system {

	using namespace Genode;

	using Genode::Noncopyable;

	class Chunk_base;

	template <size_t>           class Chunk;
	template <size_t, typename> class Chunk_index;
}


/**
 * Common base class of both 'Chunk' and 'Chunk_index'
 */
class File_system::Chunk_base : Noncopyable
{
	public:

		class Index_out_of_range { };

	protected:

		seek_off_t const _base_offset;
		size_t           _num_entries; /* corresponds to last used entry */

		/**
		 * Test if specified range lies within the chunk
		 */
		void assert_valid_range(seek_off_t start, size_t len,
		                        file_size_t chunk_size) const
		{
			if (zero()) return;

			if (start < _base_offset)
				throw Index_out_of_range();

			if (start + len > _base_offset + chunk_size)
				throw Index_out_of_range();
		}

		Chunk_base(seek_off_t base_offset)
		: _base_offset(base_offset), _num_entries(0) { }

		/**
		 * Construct zero chunk
		 *
		 * A zero chunk is a chunk that cannot be written to. When reading
		 * from it, it returns zeros. Because there is a single zero chunk
		 * for each chunk type, the base offset is meaningless. We use a
		 * base offset of ~0 as marker to identify zero chunks.
		 */
		Chunk_base() : _base_offset(~0L), _num_entries(0) { }

	public:

		/**
		 * Return absolute base offset of chunk in bytes
		 */
		seek_off_t base_offset() const { return _base_offset; }

		/**
		 * Return true if chunk is a read-only zero chunk
		 */
		bool zero() const { return _base_offset == (seek_off_t)(~0L); }

		/**
		 * Return true if chunk has no allocated sub chunks
		 */
		bool empty() const { return _num_entries == 0; }
};


/**
 * Chunk of bytes used as leaf in hierarchy of chunk indices
 */
template <Genode::size_t CHUNK_SIZE>
class File_system::Chunk : public Chunk_base
{
	private:

		char _data[CHUNK_SIZE];

	public:

		enum { SIZE = CHUNK_SIZE };

		/**
		 * Construct byte chunk
		 *
		 * \param base_offset  absolute offset of chunk in bytes
		 *
		 * The first argument is unused. Its mere purpose is to make the
		 * signature of the constructor compatible to the constructor
		 * of 'Chunk_index'.
		 */
		Chunk(Allocator &, seek_off_t base_offset)
		:
			Chunk_base(base_offset)
		{
			memset(_data, 0, CHUNK_SIZE);
		}

		/**
		 * Construct zero chunk
		 */
		Chunk() { }

		/**
		 * Return number of used entries
		 *
		 * The returned value corresponds to the index of the last used
		 * entry + 1. It does not correlate to the number of actually
		 * allocated entries (there may be ranges of zero blocks).
		 */
		file_size_t used_size() const { return _num_entries; }

		void write(char const *src, size_t len, seek_off_t seek_offset)
		{
			assert_valid_range(seek_offset, len, SIZE);

			/* offset relative to this chunk */
			seek_off_t const local_offset = seek_offset - base_offset();

			memcpy(&_data[local_offset], src, len);

			_num_entries = max(_num_entries, local_offset + len);
		}

		void read(char *dst, size_t len, seek_off_t seek_offset) const
		{
			assert_valid_range(seek_offset, len, SIZE);

			memcpy(dst, &_data[seek_offset - base_offset()], len);
		}

		void truncate(file_size_t size)
		{
			assert_valid_range(size, 0, SIZE);

			/*
			 * Offset of the first free position (relative to the beginning
			 * this chunk).
			 */
			seek_off_t const local_offset = size - base_offset();

			if (local_offset >= _num_entries)
				return;

			memset(&_data[local_offset], 0, _num_entries - local_offset);

			_num_entries = local_offset;
		}
};


template <Genode::size_t NUM_ENTRIES, typename ENTRY_TYPE>
class File_system::Chunk_index : public Chunk_base
{
	public:

		typedef ENTRY_TYPE Entry;

		enum { ENTRY_SIZE = ENTRY_TYPE::SIZE,
		       SIZE       = ENTRY_SIZE*NUM_ENTRIES };

	private:

		/*
		 * Noncopyable
		 */
		Chunk_index(Chunk_index const &);
		Chunk_index &operator = (Chunk_index const &);

		Allocator &_alloc;

		Entry * _entries[NUM_ENTRIES];

		/**
		 * Return instance of a zero sub chunk
		 */
		static Entry const &_zero_chunk()
		{
			static Entry zero_chunk;
			return zero_chunk;
		}

		/**
		 * Return sub chunk at given index
		 *
		 * If there is no sub chunk at the specified index, this function
		 * transparently allocates one. Hence, the returned sub chunk
		 * is ready to be written to.
		 */
		Entry &_entry_for_writing(unsigned index)
		{
			if (index >= NUM_ENTRIES)
				throw Index_out_of_range();

			if (_entries[index])
				return *_entries[index];

			seek_off_t entry_offset = base_offset() + index*ENTRY_SIZE;

			_entries[index] = new (&_alloc) Entry(_alloc, entry_offset);

			_num_entries = max(_num_entries, index + 1);

			return *_entries[index];
		}

		/**
		 * Return sub chunk at given index (for reading only)
		 *
		 * This function transparently provides a zero sub chunk for any
		 * index that is not populated by a real chunk.
		 */
		Entry const &_entry_for_reading(unsigned index) const
		{
			if (index >= NUM_ENTRIES)
				throw Index_out_of_range();

			if (_entries[index])
				return *_entries[index];

			return _zero_chunk();
		}

		/**
		 * Return index of entry located at specified byte offset
		 *
		 * The caller of this function must make sure that the offset
		 * parameter is within the bounds of the chunk.
		 */
		unsigned _index_by_offset(seek_off_t offset) const
		{
			return (offset - base_offset()) / ENTRY_SIZE;
		}

		/**
		 * Apply operation 'func' to a range of entries
		 */
		template <typename THIS, typename DATA, typename FUNC>
		static void _range_op(THIS &obj, DATA *data, size_t len,
		                      seek_off_t seek_offset, FUNC const &func)
		{
			/*
			 * Depending on whether this function is called for reading
			 * (const function) or writing (non-const function), the
			 * operand type is const or non-const Entry. The correct type
			 * is embedded as a trait in the 'FUNC' functor type.
			 */
			typedef typename FUNC::Entry Const_qualified_entry;

			obj.assert_valid_range(seek_offset, len, SIZE);

			while (len > 0) {

				unsigned const index = obj._index_by_offset(seek_offset);

				Const_qualified_entry &entry = FUNC::lookup(obj, index);

				/*
				 * Calculate byte offset relative to the chunk
				 *
				 * We cannot use 'entry.base_offset()' for this calculation
				 * because in the const case, the lookup might return a
				 * zero chunk, which has no defined base offset. Therefore,
				 * we calculate the base offset via index*ENTRY_SIZE.
				 */
				seek_off_t const local_seek_offset =
					seek_offset - obj.base_offset() - index*ENTRY_SIZE;

				/* available capacity at 'entry' starting at seek offset */
				seek_off_t const capacity = ENTRY_SIZE - local_seek_offset;
				seek_off_t const curr_len = min(len, capacity);

				/* apply functor (read or write) to entry */
				func(entry, data, curr_len, seek_offset);

				/* advance to next entry */
				len         -= curr_len;
				data        += curr_len;
				seek_offset += curr_len;
			}
		}

		struct Write_func
		{
			typedef ENTRY_TYPE Entry;

			static Entry &lookup(Chunk_index &chunk, unsigned i) {
				return chunk._entry_for_writing(i); }

			void operator () (Entry &entry, char const *src, size_t len,
			                  seek_off_t seek_offset) const
			{
				entry.write(src, len, seek_offset);
			}
		};

		struct Read_func
		{
			typedef ENTRY_TYPE const Entry;

			static Entry &lookup(Chunk_index const &chunk, unsigned i) {
				return chunk._entry_for_reading(i); }

			void operator () (Entry &entry, char *dst, size_t len,
			                  seek_off_t seek_offset) const
			{
				if (entry.zero())
					memset(dst, 0, len);
				else
					entry.read(dst, len, seek_offset);
			}
		};

		void _init_entries()
		{
			for (unsigned i = 0; i < NUM_ENTRIES; i++)
				_entries[i] = 0;
		}

		void _destroy_entry(unsigned i)
		{
			if (_entries[i] && (i < _num_entries)) {
				destroy(&_alloc, _entries[i]);
				_entries[i] = 0;
			}
		}

	public:

		/**
		 * Constructor
		 *
		 * \param alloc        allocator to use for allocating sub-chunk
		 *                     indices and chunks
		 * \param base_offset  absolute offset of the chunk in bytes
		 */
		Chunk_index(Allocator &alloc, seek_off_t base_offset)
		: Chunk_base(base_offset), _alloc(alloc) { _init_entries(); }

		/**
		 * Construct zero chunk
		 */
		Chunk_index() : _alloc(*(Allocator *)0) { }

		/**
		 * Destructor
		 */
		~Chunk_index()
		{
			for (unsigned i = 0; i < NUM_ENTRIES; i++)
				_destroy_entry(i);
		}

		/**
		 * Return size of chunk in bytes
		 *
		 * The returned value corresponds to the position after the highest
		 * offset that was written to.
		 */
		file_size_t used_size() const
		{
			if (_num_entries == 0)
				return 0;

			/* size of entries that lie completely within the used range */
			file_size_t const size_whole_entries = ENTRY_SIZE*(_num_entries - 1);

			Entry *last_entry = _entries[_num_entries - 1];
			if (!last_entry)
				return size_whole_entries;

			return size_whole_entries + last_entry->used_size();
		}

		/**
		 * Write data to chunk
		 */
		void write(char const *src, size_t len, seek_off_t seek_offset)
		{
			_range_op(*this, src, len, seek_offset, Write_func());
		}

		/**
		 * Read data from chunk
		 */
		void read(char *dst, size_t len, seek_off_t seek_offset) const
		{
			_range_op(*this, dst, len, seek_offset, Read_func());
		}

		/**
		 * Truncate chunk to specified size in bytes
		 *
		 * This function can be used to shrink a chunk only. Specifying a
		 * 'size' larger than 'used_size' has no effect. The value returned
		 * by 'used_size' refers always to the position of the last byte
		 * written to the chunk.
		 */
		void truncate(file_size_t size)
		{
			unsigned const trunc_index = _index_by_offset(size);

			if (trunc_index >= _num_entries)
				return;

			for (unsigned i = trunc_index + 1; i < _num_entries; i++)
				_destroy_entry(i);

			/* traverse into sub chunks */
			if (_entries[trunc_index])
				_entries[trunc_index]->truncate(size);

			_num_entries = trunc_index + 1;

			/*
			 * If the truncated at a chunk boundary, we can release the
			 * empty trailing chunk at 'trunc_index'.
			 */
			if (_entries[trunc_index] && _entries[trunc_index]->empty()) {
				_destroy_entry(trunc_index);
				_num_entries--;
			}
		}
};

#endif /* _INCLUDE__RAM_FS__CHUNK_H_ */
