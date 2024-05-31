/*
 * \brief  Ring buffer for Broadwell and newer
 * \author Josef Soentgen
 * \date   2017-03-15
 */

/*
 * Copyright (C) 2017-2024 Genode Labs GmbH
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

	template <typename> class Ring_buffer;
}


/*
 */
template <typename MEMORY>
class Igd::Ring_buffer
{
	public:

		using Index = size_t;

	private:

		MEMORY      & _memory;
		Index const   _max;
		Index         _tail { };
		Index         _head { };

		void with_dwords(auto const &fn) const {
			_memory.with_vrange([&](Byte_range_ptr const &vrange) { fn((uint32_t *)vrange.start); }); }

		void with_dwords(auto const &fn) {
			_memory.with_vrange([&](Byte_range_ptr const &vrange) { fn((uint32_t *)vrange.start); }); }

	public:

		Ring_buffer(MEMORY & memory, size_t const size)
		: _memory(memory),  _max(size / sizeof (uint32_t)) { }

		/**
		 * Clear ring buffer and reset tail
		 */
		void reset()
		{
			with_dwords([&](auto dwords) {
				Genode::memset(dwords, 0, _max * sizeof(uint32_t));
				_tail = 0;
			});
		}

		/**
		 * Clear remaining ring buffer and reset tail
		 */
		void reset_and_fill_zero()
		{
			with_dwords([&](auto dwords) {
				auto const bytes = (_max - _tail) * sizeof(uint32_t);
				Genode::memset(dwords + _tail, 0, bytes);
				_tail = 0;
			});
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

			with_dwords([&](auto dwords) {
				dwords[index] = cmd.value;
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
			});

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

		/*********************
		 ** Debug interface **
		 *********************/

		void dump(size_t dw_limit, unsigned const hw_tail, unsigned const hw_head) const
		{
			using namespace Genode;

			with_dwords([&](auto dwords) {
				size_t const max = dw_limit ? dw_limit : _max;

				log("Ring_buffer: ", Hex(*dwords),
				    " max: ", _max, " (limit: ", max, ")",
				    " hardware read: tail=", Genode::Hex(hw_tail),
				    " head=", Genode::Hex(hw_head));

				for (size_t i = 0; i < max; i++) {
					log(Hex(i*4, Hex::PREFIX, Hex::PAD), " ",
					    Hex(dwords[i], Hex::PREFIX, Hex::PAD),
					    i == _tail ? " T " : "   ",
					    i == _head ? " H " : "   ",
					    i == hw_tail ? " T_HW " : "   ",
					    i == hw_head ? " H_HW " : "   "
					);
				}
			});
		}
};

#endif /* _RING_BUFFER_H_ */
