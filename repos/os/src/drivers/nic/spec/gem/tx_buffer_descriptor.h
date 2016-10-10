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

#ifndef _INCLUDE__DRIVERS__NIC__GEM__TX_BUFFER_DESCRIPTOR_H_
#define _INCLUDE__DRIVERS__NIC__GEM__TX_BUFFER_DESCRIPTOR_H_

#include <base/log.h>
#include <timer_session/connection.h>

#include "buffer_descriptor.h"

using namespace Genode;


class Tx_buffer_descriptor : public Buffer_descriptor
{
	private:
		enum { BUFFER_COUNT = 2 };

		struct Addr : Register<0x00, 32> {};
		struct Status : Register<0x04, 32> {
			struct Length  : Bitfield<0, 14> {};
			struct Last_buffer  : Bitfield<15, 1> {};
			struct Wrap  : Bitfield<30, 1> {};
			struct Used  : Bitfield<31, 1> {};
		};

		class Package_send_timeout : public Genode::Exception {};


	public:
		Tx_buffer_descriptor() : Buffer_descriptor(BUFFER_COUNT)
		{
			for (unsigned int i=0; i<BUFFER_COUNT; i++) {
				_descriptors[i].status = Status::Used::bits(1) | Status::Last_buffer::bits(1);
			}
			_descriptors[BUFFER_COUNT-1].status |= Status::Wrap::bits(1);
		}


		void add_to_queue(const char* const packet, const size_t size)
		{
			if (size > MAX_PACKAGE_SIZE) {
				warning("Ethernet package to big. Not sent!");
				return;
			}

			/* wait until the used bit is set (timeout after 200ms) */
			uint32_t timeout = 200;
			while ( !Status::Used::get(_current_descriptor().status) ) {
				if (timeout <= 0) {
					throw Package_send_timeout();
				}
				timeout--;

				static Timer::Connection timer;
				timer.msleep(1);
			}


			memcpy(_current_buffer(), packet, size);

			_current_descriptor().status &=  Status::Length::clear_mask();
			_current_descriptor().status |=  Status::Length::bits(size);

			/* unset the unset bit */
			_current_descriptor().status &=  Status::Used::clear_mask();

			_increment_descriptor_index();
		}
};

#endif /* _INCLUDE__DRIVERS__NIC__GEM__TX_BUFFER_DESCRIPTOR_H_ */
