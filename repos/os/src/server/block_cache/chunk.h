/*
 * \brief  Data structure for storing sparse blocks
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2014-01-06
 *
 * Note: originally taken from ram_fs server, and adapted to cache needs
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHUNK_H_
#define _CHUNK_H_

/* Genode includes */
#include <util/noncopyable.h>
#include <base/allocator.h>
#include <base/exception.h>
#include <util/list.h>
#include <util/string.h>

namespace Cache {

	typedef Genode::uint64_t offset_t;
	typedef Genode::uint64_t size_t;

	/**
	 * Common base class of both 'Chunk' and 'Chunk_index'
	 */
	class Chunk_base : Genode::Noncopyable
	{
		private:

			/*
			 * Noncopyable
			 */
			Chunk_base(Chunk_base const &);
			Chunk_base &operator = (Chunk_base const &);

		public:

			struct Range_exception : Genode::Exception
			{
				offset_t off;
				size_t   size;

				Range_exception(offset_t o, size_t s) : off(o), size(s) {}
			};

			typedef Range_exception Index_out_of_range;
			typedef Range_exception Range_incomplete;

		protected:

			offset_t const _base_offset;
			size_t         _num_entries; /* corresponds to last used entry */
			Chunk_base    *_parent;
			bool const     _zero;        /* marks zero chunk */

			/**
			 * Test if specified range lies within the chunk
			 */
			void assert_valid_range(offset_t start, size_t len,
			                        size_t chunk_size) const
			{
				if (start < _base_offset)
					throw Index_out_of_range(start, len);

				if (start + len > _base_offset + chunk_size)
					throw Index_out_of_range(start, len);
			}

			Chunk_base(offset_t base_offset, Chunk_base *p)
			: _base_offset(base_offset), _num_entries(0), _parent(p),
			  _zero(false) { }

			/**
			 * Construct zero chunk
			 */
			Chunk_base()
			: _base_offset(0), _num_entries(0), _parent(0), _zero(true) { }

		public:

			virtual ~Chunk_base() { }

			/**
			 * Return absolute base offset of chunk in bytes
			 */
			offset_t base_offset() const { return _base_offset; }

			/**
			 * Return true if chunk has no allocated sub chunks
			 */
			bool empty() const { return _num_entries == 0; }

			/**
			 * Return true if this is an unused 'zero' chunk
			 */
			bool zero() const { return _zero; }

			virtual void free(size_t, offset_t) = 0;
	};


	/**
	 * Chunk of bytes used as leaf in hierarchy of chunk indices
	 */
	template <unsigned CHUNK_SIZE, typename POLICY>
	class Chunk : public Chunk_base,
	              public POLICY::Element
	{
		private:

			char        _data[CHUNK_SIZE];
			unsigned    _writes;

		public:

			typedef Range_exception Dirty_chunk;

			static constexpr size_t SIZE = CHUNK_SIZE;

			/**
			 * Construct byte chunk
			 *
			 * \param base_offset  absolute offset of chunk in bytes
			 *
			 * The first argument is unused. Its mere purpose is to make the
			 * signature of the constructor compatible to the constructor
			 * of 'Chunk_index'.
			 */
			Chunk(Genode::Allocator &, offset_t base_offset, Chunk_base *p)
			: Chunk_base(base_offset, p), _writes(0) { }

			/**
			 * Construct zero chunk
			 */
			Chunk() : _writes(0) { }

			/**
			 * Return number of used entries
			 *
			 * The returned value corresponds to the index of the last used
			 * entry + 1. It does not correlate to the number of actually
			 * allocated entries (there may be ranges of zero blocks).
			 */
			size_t used_size() const { return _num_entries; }

			void write(char const *src, size_t len, offset_t seek_offset)
			{
				assert_valid_range(seek_offset, len, SIZE);

				POLICY::write(this);

				/* offset relative to this chunk */
				offset_t const local_offset = seek_offset - base_offset();

				Genode::memcpy(&_data[local_offset], src, len);

				_num_entries = Genode::max(_num_entries, local_offset + len);

				_writes++;
			}

			void read(char *dst, size_t len, offset_t seek_offset) const
			{
				assert_valid_range(seek_offset, len, SIZE);

				POLICY::read(this);

				Genode::memcpy(dst, &_data[seek_offset - base_offset()], len);
			}

			void stat(size_t len, offset_t seek_offset) const
			{
				assert_valid_range(seek_offset, len, SIZE);

				if (_writes == 0)
					throw Range_incomplete(base_offset(), SIZE);
			}

			void sync(size_t len, offset_t seek_offset)
			{
				if (_writes > 1) {
					POLICY::sync(this, (char*)_data);
					_writes = 1;
				}
			}

			void alloc(size_t len, offset_t seek_offset) { }

			void truncate(size_t size)
			{
				assert_valid_range(size, 0, SIZE);

				/*
				 * Offset of the first free position (relative to the beginning
				 * this chunk).
				 */
				offset_t const local_offset = size - base_offset();

				if (local_offset >= _num_entries)
					return;

				Genode::memset(&_data[local_offset], 0, _num_entries - local_offset);

				_num_entries = local_offset;
			}

			void free(size_t, offset_t)
			{
				if (_writes > 1) throw Dirty_chunk(_base_offset, SIZE);

				_num_entries = 0;
				if (_parent) _parent->free(SIZE, _base_offset);
			}
	};


	template <unsigned NUM_ENTRIES, typename ENTRY_TYPE, typename POLICY>
	class Chunk_index : public Chunk_base
	{
		public:

			typedef ENTRY_TYPE Entry;

			static constexpr size_t ENTRY_SIZE = ENTRY_TYPE::SIZE;
			static constexpr size_t SIZE       = ENTRY_SIZE*NUM_ENTRIES;

		private:

			/*
			 * Noncopyable
			 */
			Chunk_index(Chunk_index const &);
			Chunk_index &operator = (Chunk_index const &);

			Genode::Allocator &_alloc;

			Entry * _entries[NUM_ENTRIES];

			/**
			 * Return instance of a zero sub chunk
			 */
			static Entry &_zero_chunk()
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
			Entry &_alloc_entry(unsigned index)
			{
				if (index >= NUM_ENTRIES)
					throw Index_out_of_range(base_offset() + index*ENTRY_SIZE,
					                         ENTRY_SIZE);

				if (_entries[index])
					return *_entries[index];

				offset_t entry_offset = base_offset() + index*ENTRY_SIZE;

				for (;;) {
					try {
						_entries[index] = new (&_alloc)
							Entry(_alloc, entry_offset, this);
						break;
					} catch(Genode::Allocator::Out_of_memory) {
						POLICY::flush(sizeof(Entry));
					}
				}

				_num_entries = Genode::max(_num_entries, index + 1);

				return *_entries[index];
			}

			/**
			 * Return sub chunk at given index
			 */
			Entry &_entry(unsigned index) const
			{
				if (index >= NUM_ENTRIES)
					throw Index_out_of_range(base_offset() + index*ENTRY_SIZE,
                                             ENTRY_SIZE);

				if (_entries[index])
					return *_entries[index];

				throw Range_incomplete(base_offset() + index*ENTRY_SIZE,
				                       ENTRY_SIZE);
			}

			/**
			 * Return sub chunk at given index (for syncing only)
			 *
			 * This function transparently provides a zero sub chunk for any
			 * index that is not populated by a real chunk.
			 */
			Entry &_entry_for_syncing(unsigned index) const
			{
				if (index >= NUM_ENTRIES)
					throw Index_out_of_range(base_offset() + index*ENTRY_SIZE,
                                             ENTRY_SIZE);

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
			unsigned _index_by_offset(offset_t offset) const
			{
				return (offset - base_offset()) / ENTRY_SIZE;
			}

			/**
			 * Apply operation 'func' to a range of entries
			 */
			template <typename THIS, typename DATA, typename FUNC>
			static void _range_op(THIS &obj, DATA *data, size_t len,
			                      offset_t seek_offset, FUNC const &func)
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
					offset_t const local_seek_offset =
						seek_offset - obj.base_offset() - index*ENTRY_SIZE;

					/* available capacity at 'entry' starting at seek offset */
					offset_t const capacity = ENTRY_SIZE - local_seek_offset;
					size_t   const curr_len = Genode::min(len, capacity);

					/* apply functor (read or write) to entry */
					func(entry, data, curr_len, seek_offset);

					/* advance to next entry */
					len         -= curr_len;
					data        += curr_len;
					seek_offset += curr_len;
				}
			}

			struct Alloc_func
			{
				typedef ENTRY_TYPE Entry;

				static Entry &lookup(Chunk_index &chunk, unsigned i) {
					return chunk._alloc_entry(i); }

				void operator () (Entry &entry, char const *src, size_t len,
				                  offset_t seek_offset) const
				{
					entry.alloc(len, seek_offset);
				}
			};

			struct Write_func
			{
				typedef ENTRY_TYPE Entry;

				static Entry &lookup(Chunk_index &chunk, unsigned i) {
					return chunk._entry(i); }

				void operator () (Entry &entry, char const *src, size_t len,
				                  offset_t seek_offset) const
				{
					entry.write(src, len, seek_offset);
				}
			};

			struct Read_func
			{
				typedef ENTRY_TYPE const Entry;

				static Entry &lookup(Chunk_index const &chunk, unsigned i) {
					return chunk._entry(i); }

				void operator () (Entry &entry, char *dst, size_t len,
				                  offset_t seek_offset) const {
					entry.read(dst, len, seek_offset); }
			};

			struct Stat_func
			{
				typedef ENTRY_TYPE const Entry;

				static Entry &lookup(Chunk_index const &chunk, unsigned i) {
					return chunk._entry(i); }

				void operator () (Entry &entry, char*, size_t len,
				                  offset_t seek_offset) const {
					entry.stat(len, seek_offset); }
			};

			struct Sync_func
			{
				typedef ENTRY_TYPE Entry;

				static Entry &lookup(Chunk_index const &chunk, unsigned i) {
					return chunk._entry_for_syncing(i); }

				void operator () (Entry &entry, char*, size_t len,
				                  offset_t seek_offset) const
				{
					entry.sync(len, seek_offset);
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
					Genode::destroy(&_alloc, _entries[i]);
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
			Chunk_index(Genode::Allocator &alloc, offset_t base_offset,
			            Chunk_base *p = 0)
			: Chunk_base(base_offset, p), _alloc(alloc) { _init_entries(); }

			/**
			 * Construct zero chunk
			 */
			Chunk_index() : _alloc(*(Genode::Allocator *)0) { }

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
			void write(char const *src, size_t len, offset_t seek_offset) {
				_range_op(*this, src, len, seek_offset, Write_func()); }

			/**
			 * Allocate needed chunks
			 */
			void alloc(size_t len, offset_t seek_offset) {
				_range_op(*this, (char*)0, len, seek_offset, Alloc_func()); }

			/**
			 * Read data from chunk
			 */
			void read(char *dst, size_t len, offset_t seek_offset) const {
				_range_op(*this, dst, len, seek_offset, Read_func()); }

			/**
			 * Check for chunk availability
			 */
			void stat(size_t len, offset_t seek_offset) const {
				_range_op(*this, (char*)0, len, seek_offset, Stat_func()); }

			/**
			 * Synchronize chunk when dirty
			 */
			void sync(size_t len, offset_t seek_offset) const {
				if (zero()) return;
				_range_op(*this, (char*)0, len, seek_offset, Sync_func()); }

			/**
			 * Free chunks
			 */
			void free(size_t len, offset_t seek_offset)
			{
				unsigned const index = _index_by_offset(seek_offset);
				Genode::destroy(&_alloc, _entries[index]);
				_entries[index] = 0;
				for (unsigned i = 0; i < NUM_ENTRIES; i++)
					if (_entries[i])
						return;

				if (_parent) _parent->free(SIZE, _base_offset);
			}

			/**
			 * Truncate chunk to specified size in bytes
			 *
			 * This function can be used to shrink a chunk only. Specifying a
			 * 'size' larger than 'used_size' has no effect. The value returned
			 * by 'used_size' refers always to the position of the last byte
			 * written to the chunk.
			 */
			void truncate(size_t size)
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
};

#endif /* _CHUNK_H_ */
