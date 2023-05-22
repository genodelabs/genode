/*
 * \brief  Broadwell ring buffer
 * \author Josef Soentgen
 * \date   2017-03-15
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RING_BUFFER_H_
#define _RING_BUFFER_H_

/* Genode includes */
#include <base/log.h>

/* local includes */
#include <types.h>
#include <commands.h>


namespace Igd {

	class Ring_buffer;
}


/*
 */
class Igd::Ring_buffer
{
	public:

		using Index = size_t;

	private:

		uint32_t *_dwords;
		Index     _max;
		Index     _tail;
		Index     _head;

	public:

		Ring_buffer(addr_t   base,
		            size_t   length)
		:
			_dwords((uint32_t*)base), _max(length / sizeof (uint32_t)),
			_tail(0), _head(0)
		{
			/* initial clear */
			Genode::memset(_dwords, 0, length);
		}

		/**
		 * Clear ring buffer and reset tail
		 */
		void reset()
		{
			Genode::memset(_dwords, 0, _max * sizeof(uint32_t));
			_tail = 0;
		}

		/**
		 * Clear remaining ring buffer and reset tail
		 */
		void reset_and_fill_zero()
		{
			Genode::size_t const bytes = (_max - _tail) * sizeof(uint32_t);
			Genode::memset(_dwords + _tail, 0, bytes);
			_tail = 0;
		}

		/**
		 * Get current tail
		 *
		 * \return current tail index
		 */
		Index tail() const { return _tail; }

		/**
		 * Get current head
		 *
		 * \return current head index
		 */
		Index head() const { return _head; }

		/**
		 * Update head
		 *
		 * \param head  new head index
		 */
		void update_head(Index head)
		{
			_head = head;
		}

		/**
		 * Update head and set tail to head
		 */
		void reset_to_head(Index head)
		{
			update_head(head);
			_tail = _head;
		}

		/**
		 * Insert new command at given index
		 *
		 * \param cmmd   new command
		 * \param index  index for new command
		 */
		Index insert(Igd::Cmd_header cmd, Index index)
		{
			if (index < _tail) { throw -1; }
			if (index > _max)  { throw -2; }

			_dwords[index] = cmd.value;
			_tail++;

			if (_tail >= _max) {
				Genode::warning("ring buffer wrapped ",
				                "_tail: ", _tail, " ", "_max: ", _max);
				_tail = 0;
			}

			if (_tail == _head) {
				Genode::error("tail: ", Genode::Hex(_tail), " == head: ",
				              Genode::Hex(_head), " in ring buffer");
			}

			return 1;
		}

		/**
		 * Append command to ring buffer
		 *
		 * \param cmd  command
		 *
		 * \return number of command words written
		 */
		Index append(Igd::Cmd_header cmd)
		{
			return insert(cmd, _tail);
		}

		/**
		 * Append command to ring buffer
		 *
		 * \param v  dword value
		 *
		 * \return number of command words written
		 */
		Index append(uint32_t v)
		{
			return insert(Igd::Cmd_header(v), _tail);
		}

		/**
		 * Check if remaing space is enough for number of commands
		 *
		 * \return true if space is sufficient, otherwise false is returned
		 */
		bool avail(Index num) const
		{
			return (_tail + num) < _max;
		}

		/**
		 * Get total number of commands that fit into the ring buffer
		 *
		 * \return number of total commands fitting the ring buffer
		 */
		Index max() const { return _max; }

		/**
		 * Flush range of commands in ring buffer
		 *
		 * \param from  index to start from
		 * \param to    index to end on
		 */
		void flush(Index from, Index to) const
		{
			uint32_t *start = _dwords + from;
			for (Index i = 0; i < (to - from); i++) {
				uint32_t *addr = start++;
				Utils::clflush(addr);
			}
		}

		/*********************
		 ** Debug interface **
		 *********************/

		void dump(size_t dw_limit, unsigned const hw_tail, unsigned const hw_head) const
		{
			using namespace Genode;

			size_t const max = dw_limit ? dw_limit : _max;

			log("Ring_buffer: ", Hex(*_dwords), " max: ", _max, " (limit: ", max, ")",
			    " hardware read: tail=", Genode::Hex(hw_tail),
			    " head=", Genode::Hex(hw_head));

			for (size_t i = 0; i < max; i++) {
				log(Hex(i*4, Hex::PREFIX, Hex::PAD), " ",
				    Hex(_dwords[i], Hex::PREFIX, Hex::PAD),
				    i == _tail ? " T " : "   ",
				    i == _head ? " H " : "   ",
				    i == hw_tail ? " T_HW " : "   ",
				    i == hw_head ? " H_HW " : "   "
				);
			}
		}
};

#endif /* _RING_BUFFER_H_ */
