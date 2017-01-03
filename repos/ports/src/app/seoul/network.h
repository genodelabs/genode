/*
 * \brief  Network receive handler per MAC address
 * \author Markus Partheymueller
 * \author Alexander Boettcher
 * \date   2012-07-31
 */

/*
 * Copyright (C) 2012 Intel Corporation
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

#ifndef _NETWORK_H_
#define _NETWORK_H_

/* base includes */
#include <base/heap.h>

/* os includes */
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>

/* local includes */
#include "synced_motherboard.h"

namespace Seoul {
	class Network;
}

class Seoul::Network
{
	private:

		enum {
			PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
			BUF_SIZE    = Nic::Session::QUEUE_SIZE * PACKET_SIZE,
		};

		Synced_motherboard   &_motherboard;
		Nic::Packet_allocator _tx_block_alloc;
		Nic::Connection       _nic;

		Genode::Signal_handler<Network> const _packet_avail;
		void const *                          _forward_pkt = nullptr;

		void _handle_packets();

	public:

		Network(Genode::Env &, Genode::Heap &, Synced_motherboard &);

		Nic::Mac_address mac_address() { return _nic.mac_address(); }

		bool transmit(void const * const packet, Genode::size_t len);
};

#endif /* _NETWORK_H_ */
