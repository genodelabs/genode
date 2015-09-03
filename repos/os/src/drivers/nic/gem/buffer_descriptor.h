/*
 * \brief  Base EMAC driver for the Xilinx EMAC PS used on Zynq devices
 * \author Timo Wischer
 * \date   2015-03-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__DRIVERS__NIC__GEM__BUFFER_DESCRIPTOR_H_
#define _INCLUDE__DRIVERS__NIC__GEM__BUFFER_DESCRIPTOR_H_

/* Genode includes */
#include <os/attached_ram_dataspace.h>
#include <util/mmio.h>

using namespace Genode;


class Buffer_descriptor : protected Attached_ram_dataspace, protected Mmio
{
	public:
		static const size_t BUFFER_DESC_SIZE = 0x08;
		static const size_t MAX_PACKAGE_SIZE = 1600;
		static const size_t BUFFER_SIZE = BUFFER_DESC_SIZE + MAX_PACKAGE_SIZE;

	private:
		const size_t _buffer_count;
		const size_t _buffer_offset;

		unsigned int _descriptor_index;
		char* const _buffer;

	protected:
		typedef struct {
			uint32_t addr;
			uint32_t status;
		} descriptor_t;

		descriptor_t* const _descriptors;



		void _increment_descriptor_index()
		{
			_descriptor_index++;
			_descriptor_index %= _buffer_count;
		}


		descriptor_t& _current_descriptor()
		{
			return _descriptors[_descriptor_index];
		}


		char* const _current_buffer()
		{
			char* const buffer = &_buffer[MAX_PACKAGE_SIZE * _descriptor_index];
			return buffer;
		}


	public:
		/*
		 * start of the ram spave contains all buffer descriptors
		 * after that the data spaces for the ethernet packages are following
		 */
		Buffer_descriptor(const size_t buffer_count = 1) :
			Attached_ram_dataspace(env()->ram_session(), BUFFER_SIZE * buffer_count, UNCACHED),
			Genode::Mmio( reinterpret_cast<addr_t>(local_addr<void>()) ),
			_buffer_count(buffer_count),
			_buffer_offset(BUFFER_DESC_SIZE * buffer_count),
			_descriptor_index(0),
			_buffer(local_addr<char>() + _buffer_offset),
			_descriptors(local_addr<descriptor_t>())
		{
			/*
			 * Save address of _buffer.
			 * Has to be the physical address and not the virtual addresse,
			 * because the dma controller of the nic will use it.
			 * Ignore lower two bits,
			 * because this are status bits.
			 */
			for (unsigned int i=0; i<buffer_count; i++) {
				_descriptors[i].addr = (phys_addr_buffer(i)) & ~0x3;
			}
		}


		addr_t phys_addr() { return Dataspace_client(cap()).phys_addr(); }

		addr_t phys_addr_buffer(const unsigned int index)
		{
			return phys_addr() + _buffer_offset + MAX_PACKAGE_SIZE * index;
		}
};

#endif /* _INCLUDE__DRIVERS__NIC__GEM__BUFFER_DESCRIPTOR_H_ */
