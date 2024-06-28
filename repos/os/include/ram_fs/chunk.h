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

		/*
		 * Use 'size_t' instead of 'seek_off_t' because we can never seek
		 * outside the addressable RAM
		 */
		struct Seek { size_t value; };

	protected:

		Seek const _base_offset { ~0UL };

		size_t _num_entries = 0; /* corresponds to last used entry */

		/**
		 * Test if specified range lies within the chunk
		 */
		void assert_valid_range(Seek start, size_t len,
		                        size_t chunk_size) const
		{
			if (zero()) return;

			if (start.value < _base_offset.value)
				throw Index_out_of_range();

			if (start.value + len > _base_offset.value + chunk_size)
				throw Index_out_of_range();
		}

		Chunk_base(Seek base_offset) : _base_offset(base_offset) { }

		/**
		 * Construct zero chunk
		 *
		 * A zero chunk is a chunk that cannot be written to. When reading
		 * from it, it returns zeros. Because there is a single zero chunk
		 * for each chunk type, the base offset is meaningless. We use a
		 * base offset of ~0 as marker to identify zero chunks.
		 */
		Chunk_base() { }

	public:

		/**
		 * Return absolute base offset of chunk in bytes
		 */
		Seek base_offset() const { return _base_offset; }

		/**
		 * Return true if chunk is a read-only zero chunk
		 */
		bool zero() const { return _base_offset.value == ~0UL; }

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
		Chunk(Allocator &, Seek base_offset)
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
		size_t used_size() const { return _num_entries; }

		void write(Const_byte_range_ptr const &src, Seek at)
		{
			assert_valid_range(at, src.num_bytes, SIZE);

			/* offset relative to this chunk */
			size_t const local_offset = at.value - base_offset().value;

			memcpy(&_data[local_offset], src.start, src.num_bytes);

			_num_entries = max(_num_entries, local_offset + src.num_bytes);
		}

		void read(Byte_range_ptr const &dst, Seek at) const
		{
			assert_valid_range(at, dst.num_bytes, SIZE);

			memcpy(dst.start, &_data[at.value - base_offset().value], dst.num_bytes);
		}

		void truncate(Seek at)
		{
			assert_valid_range(at, 0, SIZE);

			/*
			 * Offset of the first free position (relative to the beginning
			 * this chunk).
			 */
			size_t const local_offset = at.value - base_offset().value;

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

		using Entry = ENTRY_TYPE;

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

			Seek const entry_offset { base_offset().value + index*ENTRY_SIZE };

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
		unsigned _index_by_offset(Seek offset) const
		{
			return (unsigned)((offset.value - base_offset().value) / ENTRY_SIZE);
		}

		/**
		 * Apply operation 'func' to a range of entries
		 */
		template <typename THIS, typename RANGE_PTR, typename FUNC>
		static void _range_op(THIS &obj, RANGE_PTR const &range_ptr,
		                      Seek at, FUNC const &func)
		{
			/*
			 * Depending on whether this function is called for reading
			 * (const function) or writing (non-const function), the
			 * operand type is const or non-const Entry. The correct type
			 * is embedded as a trait in the 'FUNC' functor type.
			 */
			using Const_qualified_entry = typename FUNC::Entry;

			auto   data_ptr = range_ptr.start;
			size_t len      = range_ptr.num_bytes;

			obj.assert_valid_range(at, len, SIZE);

			while (len > 0) {

				unsigned const index = obj._index_by_offset(at);

				Const_qualified_entry &entry = FUNC::lookup(obj, index);

				/*
				 * Calculate byte offset relative to the chunk
				 *
				 * We cannot use 'entry.base_offset()' for this calculation
				 * because in the const case, the lookup might return a
				 * zero chunk, which has no defined base offset. Therefore,
				 * we calculate the base offset via index*ENTRY_SIZE.
				 */
				size_t const local_seek_offset =
					at.value - obj.base_offset().value - index*ENTRY_SIZE;

				/* available capacity at 'entry' starting at seek offset */
				size_t const capacity = ENTRY_SIZE - local_seek_offset;
				size_t const curr_len = min(len, capacity);

				/* apply functor (read or write) to entry */
				func(entry, RANGE_PTR(data_ptr, curr_len), at);

				/* advance to next entry */
				len      -= curr_len;
				data_ptr += curr_len;
				at.value += curr_len;
			}
		}

		struct Write_func
		{
			using Entry = ENTRY_TYPE;

			static Entry &lookup(Chunk_index &chunk, unsigned i) {
				return chunk._entry_for_writing(i); }

			void operator () (Entry &entry, Const_byte_range_ptr const &src, Seek at) const
			{
				entry.write(src, at);
			}
		};

		struct Read_func
		{
			using Entry = ENTRY_TYPE const;

			static Entry &lookup(Chunk_index const &chunk, unsigned i) {
				return chunk._entry_for_reading(i); }

			void operator () (Entry &entry, Byte_range_ptr const &dst, Seek at) const
			{
				if (entry.zero())
					memset(dst.start, 0, dst.num_bytes);
				else
					entry.read(dst, at);
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
		Chunk_index(Allocator &alloc, Seek base_offset)
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
		size_t used_size() const
		{
			if (_num_entries == 0)
				return 0;

			/* size of entries that lie completely within the used range */
			size_t const size_whole_entries = ENTRY_SIZE*(_num_entries - 1);

			Entry *last_entry = _entries[_num_entries - 1];
			if (!last_entry)
				return size_whole_entries;

			return size_whole_entries + last_entry->used_size();
		}

		/**
		 * Write data to chunk
		 */
		void write(Const_byte_range_ptr const &src, Seek at)
		{
			_range_op(*this, src, at, Write_func());
		}

		/**
		 * Read data from chunk
		 */
		void read(Byte_range_ptr const &dst, Seek at) const
		{
			_range_op(*this, dst, at, Read_func());
		}

		/**
		 * Truncate chunk to specified size in bytes
		 *
		 * This function can be used to shrink a chunk only. Specifying a
		 * 'size' larger than 'used_size' has no effect. The value returned
		 * by 'used_size' refers always to the position of the last byte
		 * written to the chunk.
		 */
		void truncate(Seek at)
		{
			unsigned const trunc_index = _index_by_offset(at);

			if (trunc_index >= _num_entries)
				return;

			for (unsigned i = trunc_index + 1; i < _num_entries; i++)
				_destroy_entry(i);

			/* traverse into sub chunks */
			if (_entries[trunc_index])
				_entries[trunc_index]->truncate(at);

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
