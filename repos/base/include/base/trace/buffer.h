/*
 * \brief  Event tracing buffer
 * \author Norman Feske
 * \author Johannes Schlatow
 * \date   2013-08-09
 *
 * The trace buffer is shared between the traced component (producer) and
 * the trace monitor (consumer). It basically is a lock-free/wait-free
 * single-producer single-consumer (spsc) ring buffer. There are a couple of
 * differences to a a standard spsc ring buffer:
 *
 *  - If the buffer is full, we want to overwrite the oldest data.
 *  - We do not care about the consumer as it might not even exist. Hence,
 *    the tail pointer shall be managed locally by the consumer.
 *  - The buffer entries have variable length.
 *
 * As a consequence of the variable length, the entry length needs to be stored
 * in each entry. A zero-length entry marks the head of the buffer (the entry
 * that is written next). Moreover, we may need some padding at the end of the
 * buffer if the entry does not fit in the remaing space. To distinguish the
 * padding from the buffer head, it is marked by a length field with a maximum
 * unsigned value.
 *
 * Let's have a look at the layout of a non-full buffer. The zero-length field
 * marks the head. The consumer can stop reading when it sees this.
 *
 * +------------------------+------------+-------------+-----+-----------------+
 * | len1             data1 | len2 data2 | len3  data3 |  0  | empty           |
 * +------------------------+------------+-------------+-----+-----------------+
 *
 * Now, when the next entry does not fit into the remaining buffer space, it
 * wraps around and starts at the beginning. The unused space at the end is
 * padded:
 *
 * +------------------------+------------+-------------+-----+-----------------+
 * | len4 data4 | 0 | empty | len2 data2 | len3  data3 | MAX | padding         |
 * +------------------------+------------+-------------+-----+-----------------+
 *
 * If the consumer detects the padding it skips it and continues at the
 * beginning. Note, that the padding is not present if there is less than a
 * length field left at the end of the buffer.
 *
 * A potential consumer is supposed read new buffer entries fast enough as,
 * otherwise, it will miss some entries. We count the buffer wrap arounds to
 * detect this.
 *
 * Unfortunately, we cannot easily ensure that the producer does not overwrite
 * entries that are currently read out and, even worse, void the consumer's
 * tail pointer in the process. Also, it cannot be implicitly detected by
 * looking at the wrapped count. Imagine the consumer stopped in the middle of
 * the buffer since there are no more entries and resumes reading when the
 * producer wrapped once and almost caught up with the consumers position. The
 * consumer sees that the buffer wrapped only once but can still be corruped
 * by the producer.
 *
 * In order to prevent this, we need a way to determine on the producer side
 * where the consumer currently resides in the buffer and a) prevent that the
 * producer overwrites these parts of the buffer and b) still be able to
 * write the most recent buffer entry to some place.
 *
 * One way to approach this could be to store the tail pointer in the buffer
 * and ensure that the head never overtakes the tail. The producer could
 * continue writing from the start of the buffer instead. A critical corner
 * case, however, occurs when the tail pointer resides at the very beginning
 * of the buffer. In this case, newly produced entries must be dropped.
 *
 * Another approach is to split the buffer into two equal
 * partitions. The foreground partition is the one currently written so that
 * the background partition can be read without memory corruption. When the
 * foreground partition is full, the producer switches the partitions and starts
 * overwriting old entries in the former background partition. By locking the
 * background partition, the consumer makes sure that the producer does not
 * switch partitions. This way we assure that the head pointer never overtakes
 * the tail pointer. In case the background partition is locked when the
 * producer wants to switch partitions, it starts overwriting the foreground
 * partition. The producer increments a counter for each partition whenever it
 * overwrites the very first entry. This way the consumer is able to detect if
 * it lost some events.
 *
 * The consumer is also able to lock the foreground partition so that it does
 * not need to wait for the producer to fill it and switch partitions. Yet,
 * it must never lock both partitions as this would stall the producer. We
 * ensure this making the unlock-background-lock-foreground operation atomic.
 * In case the consumer crashed when a lock is held, the producer is still able
 * to use half of the buffer. Care must be taken, however, to eliminate a race
 * between the producer wrapping and the consumer switching to the foreground
 * buffer.
 */

/*
 * Copyright (C) 2013-2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__TRACE__BUFFER_H_
#define _INCLUDE__BASE__TRACE__BUFFER_H_

#include <base/stdint.h>
#include <cpu_session/cpu_session.h>
#include <util/register.h>

namespace Genode::Trace {
	class Simple_buffer;

	class Partitioned_buffer;

	using Buffer = Partitioned_buffer;
}


/**
 * Buffer shared between CPU client thread and TRACE client
 */
class Genode::Trace::Simple_buffer
{
	private:

		friend class Partitioned_buffer;

		size_t            _head_offset;  /* in bytes, relative to 'entries' */
		size_t            _size;         /* in bytes */
		unsigned volatile _num_entries;  /* number of entries currently in buffer */

		struct _Entry
		{
			enum Type : size_t {
				HEAD     = 0,
				PADDING  = ~(size_t)0
			};

			size_t len;
			char   data[0];

			void mark(Type t)    { len = t; }

			bool head()    const { return len == HEAD; }
			bool padding() const { return len == PADDING; }
		};

		_Entry _entries[0];

		/*
		 * The 'entries' member marks the beginning of the trace buffer
		 * entries. No other member variables must follow.
		 */

		_Entry *_head_entry() { return (_Entry *)((addr_t)_entries + _head_offset); }

		void _buffer_wrapped()
		{
			if (_num_entries == 1)
				error("trace buffer is dangerously small");

			_num_entries = 0;
			_head_offset = 0;

			/* mark first entry as head */
			_head_entry()->mark(_Entry::HEAD);
		}

		template <typename WRAP_FUNC>
		char *_reserve(size_t len, WRAP_FUNC && wrap)
		{
			if (_head_offset + sizeof(_Entry) + len <= _size)
				return _head_entry()->data;

			/* mark unused space as padding */
			if (_head_offset + sizeof(_Entry) <= _size)
				_head_entry()->mark(_Entry::PADDING);

			return wrap();
		}

		template <typename WRAP_FUNC>
		void _commit(size_t len, WRAP_FUNC && wrap)
		{
			/* omit empty entries */
			if (len == 0)
				return;

			/**
			 * remember current length field so that we can write it after we set
			 * the new head
			 */
			size_t *old_head_len = &_head_entry()->len;
			_num_entries++;

			/* advance head offset, wrap when next entry does not fit into buffer */
			_head_offset += sizeof(_Entry) + len;
			if (_head_offset + sizeof(_Entry) > _size)
				wrap();

			/* mark entry next to new entry as head */
			else if (_head_offset + sizeof(_Entry) <= _size)
				_head_entry()->mark(_Entry::HEAD);

			*old_head_len = len;
		}

	public:

		/******************************************
		 ** Functions called from the CPU client **
		 ******************************************/

		void init(size_t size)
		{
			_head_offset = 0;

			/* compute number of bytes available for tracing data */
			size_t const header_size = (addr_t)&_entries - (addr_t)this;

			_size = size - header_size;

			/* mark first entry as head */
			_head_entry()->mark(_Entry::HEAD);

			_num_entries = 0;
		}

		char *reserve(size_t len)
		{
			return _reserve(len, [&] () -> char* {
				_buffer_wrapped();

				return _head_entry()->data;
			});
		}

		void commit(size_t len) {
			return _commit(len, [&] () { _buffer_wrapped(); }); }


		/********************************************
		 ** Functions called from the TRACE client **
		 ********************************************/

		class Entry
		{
			private:
				_Entry const *_entry;

				friend class Simple_buffer;

				Entry(_Entry const *entry) : _entry(entry) { }

				/* entry is padding */
				bool _padding()  const { return _entry->padding(); }

			public:

				/* return invalid entry (checked by last()) */
				static Entry invalid()     { return Entry(0); }

				size_t      length() const { return _entry->len; }
				char const *data()   const { return _entry->data; }

				template <typename T>
				T const    &object() const { return *reinterpret_cast<const T*>(data()); }

				/* return whether entry is valid, i.e. length field is present */
				bool last()     const { return _entry == 0; }

				/* return whether data field is invalid */
				bool empty()    const { return last() || _padding() || _entry->head(); }

				/* entry is head (zero length) */
				bool head()     const { return !last() && _entry->head(); }
		};

		/* Return whether buffer has been initialized. */
		bool initialized() const { return _size && _head_offset <= _size; }

		/* Return the very first entry at the start of the buffer. */
		Entry first() const
		{
			/* return invalid entry if buffer is uninitialised */
			if (!initialized())
				return Entry::invalid();

			return Entry(_entries);
		}

		/**
		 * Return the entry that follows the given entry.
		 * Returns an invalid entry if the end of the (used) buffer was reached.
		 * Stops at the head of the buffer.
		 *
		 * The reader must check before on a valid entry whether it is the head
		 * of the buffer (not yet written).
		 */
		Entry next(Entry entry) const
		{
			if (entry.last() || entry._padding())
				return Entry::invalid();

			if (entry.head())
				return entry;

			addr_t const offset = (addr_t)entry.data() - (addr_t)_entries;
			if (offset + entry.length() + sizeof(_Entry) > _size)
				return Entry::invalid();

			return Entry((_Entry const *)((addr_t)entry.data() + entry.length()));
		}
};


class Genode::Trace::Partitioned_buffer
{
	public:
		using Entry = Simple_buffer::Entry;

	private:
		enum { PRIMARY = 0, SECONDARY = 1 };

		/* place consumer and producer state into single word to make switches atomic */
		struct State : Register<32>
		{
			struct Producer : Bitfield<0, 1> { };
			struct Consumer : Bitfield<16,1> { };

			static int toggle_consumer(int old) {
				return Producer::masked(old) | Consumer::bits(~Consumer::get(old)); }

			static int toggle_producer(int old) {
				return Consumer::masked(old) | Producer::bits(~Producer::get(old)); }
		};

		/************
		 ** Member **
		 ************/

		unsigned long long volatile _lost_entries;
		unsigned           volatile _wrapped;
		int                volatile _state;
		int                volatile _consumer_lock;

		size_t         _secondary_offset;
		Simple_buffer  _primary[0];

		/*
		 * The '_primary' member marks the beginning of the trace buffers.
		 * No other member variables must follow.
		 */

		Simple_buffer *_secondary() const {
			return reinterpret_cast<Simple_buffer*>((addr_t)_primary + _secondary_offset); }

		Simple_buffer &_producer()
		{
			if (State::Producer::get(_state) == PRIMARY)
				return *_primary;

			return *_secondary();
		}

		Simple_buffer const &_consumer() const
		{
			if (State::Consumer::get(_state) == PRIMARY)
				return *_primary;

			return *_secondary();
		}

		/**
		 * Switch consumer's partition
		 *
		 * The consumer can always switch but must wait for the producer if the
		 * latter is currently switching into the same partition.
		 */
		Simple_buffer const &_switch_consumer();

		/**
		 * Switch producer's partition
		 *
		 * The producer switches partitions only if the consumer is currently
		 * in the same partition. Otherwise, it wraps and discards all entries
		 * in the current partition.
		 */
		Simple_buffer &_switch_producer();

	public:

		/******************************************
		 ** Functions called from the CPU client **
		 ******************************************/

		void init(size_t len);

		char *reserve(size_t len);

		void commit(size_t len);

		/********************************************
		 ** Functions called from the TRACE client **
		 ********************************************/

		unsigned           wrapped()      const { return _wrapped; }
		unsigned long long lost_entries() const { return _lost_entries; }

		Entry first()       const { return _consumer().first(); }
		bool  initialized() const { return _secondary_offset > 0 && _consumer().initialized(); }

		/**
		 * Return the entry that follows the given entry.
		 * Automatically switches between the partitions if the end of the buffer
		 * was reached. Stops at the head of the buffer.
		 *
		 * The reader must check before on a valid entry whether it is the head
		 * of the buffer (not yet written).
		 */
		Entry next(Entry entry)
		{
			Entry e = _consumer().next(entry);
			if (e.last())
				return _switch_consumer().first();

			return e;
		}
};

#endif /* _INCLUDE__BASE__TRACE__BUFFER_H_ */
