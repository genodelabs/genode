/*
 * \brief  Base EMAC driver for the Xilinx EMAC PS used on Zynq devices
 * \author Timo Wischer
 * \author Johannes Schlatow
 * \date   2015-03-10
 */

/*
 * Copyright (C) 2015-2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__DRIVERS__NIC__GEM__BUFFER_DESCRIPTOR_H_
#define _INCLUDE__DRIVERS__NIC__GEM__BUFFER_DESCRIPTOR_H_

/* Genode includes */
#include <base/attached_ram_dataspace.h>
#include <util/mmio.h>

using namespace Genode;


class Buffer_descriptor : protected Attached_ram_dataspace, protected Mmio
{
	public:
		static const size_t BUFFER_DESC_SIZE = 0x08;
		static const size_t BUFFER_SIZE      = 1600;

	private:
		size_t _buffer_count;
		size_t _head_idx { 0 };
		size_t _tail_idx { 0 };

	protected:
		typedef struct {
			uint32_t addr;
			uint32_t status;
		} descriptor_t;

		descriptor_t* const _descriptors;

		/* set the maximum descriptor index */
		inline
		void _max_index(size_t max_index) { _buffer_count = max_index+1; };

		/* get the maximum descriptor index */
		inline
		unsigned _max_index() { return _buffer_count-1; }

		inline
		void _advance_head()
		{
			_head_idx = (_head_idx+1) % _buffer_count;
		}

		inline
		void _advance_tail()
		{
			_tail_idx = (_tail_idx+1) % (_buffer_count);
		}

		inline
		descriptor_t& _head()
		{
			return _descriptors[_head_idx];
		}

		inline
		descriptor_t& _tail()
		{
			return _descriptors[_tail_idx];
		}

		size_t _queued() const
		{
			if (_head_idx >= _tail_idx)
				return _head_idx - _tail_idx;
			else
				return _head_idx + _buffer_count - _tail_idx;
		}

		size_t _head_index() const { return _head_idx; }
		size_t _tail_index() const { return _tail_idx; }

		void _reset_head() { _head_idx = 0; }
		void _reset_tail() { _tail_idx = 0; }

	private:

		/*
		 * Noncopyable
		 */
		Buffer_descriptor(Buffer_descriptor const &);
		Buffer_descriptor &operator = (Buffer_descriptor const &);


	public:
		/*
		 * start of the ram spave contains all buffer descriptors
		 * after that the data spaces for the ethernet packages are following
		 */
		Buffer_descriptor(Genode::Env &env, const size_t buffer_count = 1)
		:
			Attached_ram_dataspace(env.ram(), env.rm(), BUFFER_DESC_SIZE * buffer_count, UNCACHED),
			Genode::Mmio( reinterpret_cast<addr_t>(local_addr<void>()) ),
			_buffer_count(buffer_count),
			_descriptors(local_addr<descriptor_t>())
		{ }

		addr_t phys_addr() { return Dataspace_client(cap()).phys_addr(); }
};

#endif /* _INCLUDE__DRIVERS__NIC__GEM__BUFFER_DESCRIPTOR_H_ */
