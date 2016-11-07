/*
 * \brief  Interface to create statistic about received and transmitted
 *         packets of NIC components
 * \author Alexander Boettcher 
 * \date   2013-03-26
 */

/*
 * Copyright (C) 2013-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__NIC__STAT_H_
#define _INCLUDE__NIC__STAT_H_

#include <base/stdint.h>
#include <net/ethernet.h>

#include <timer_session/connection.h>

namespace Nic { class Measurement; }


class Nic::Measurement
{
	private:

		Timer::Connection &_timer;

		Net::Mac_address _mac;

		struct stat
		{
			Genode::uint64_t size;
			unsigned long count;
		} _stat, _drop;

		Genode::addr_t _timestamp;

		enum status {
			FOR_US,
			IS_MAGIC,
			UNKNOWN
		};

		enum status _check(Net::Ethernet_frame *, Genode::size_t);
	public:

		Measurement(Timer::Connection &timer)
		:
			_timer(timer), _timestamp(0)
		{
			_stat.size = _stat.count = _drop.size = _drop.count = 0;
		}

		void set_mac(void * mac)
		{
			Genode::memcpy(_mac.addr, mac, 6);
		}

		void data(Net::Ethernet_frame *, Genode::size_t);
};

#endif /* _INCLUDE__NIC__STAT_H_ */
