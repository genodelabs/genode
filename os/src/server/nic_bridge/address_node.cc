/*
 * \brief  Address-node holds a client-specific session-component.
 * \author Stefan Kalkowski
 * \date   2010-08-25
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/lock_guard.h>

#include "address_node.h"
#include "component.h"

/**
 * Let the client a receive a network packet.
 *
 * \param addr  start address network packet.
 * \param size  size of network packet.
 */
template <unsigned LEN>
void Net::Address_node<LEN>::receive_packet(void *addr, Genode::size_t size)
{
	Genode::Lock::Guard lock_guard(*_component->rx_lock());

	Nic::Session::Rx::Source *source = _component->rx_source();

	while (true) {
		/* flush remaining acknowledgements */
		while (source->ack_avail())
			source->release_packet(source->get_acked_packet());

		try {
			/* allocate packet in rx channel */
			Packet_descriptor rx_packet = source->alloc_packet(size);

			Genode::memcpy((void*)source->packet_content(rx_packet),
			               (void*)addr, size);
			source->submit_packet(rx_packet);
			return;
		} catch (Nic::Session::Rx::Source::Packet_alloc_failed) { }
	}
}


/**
 * Let the compiler know, for which template instances this implementation
 * is used for.
 */
template void Net::Ipv4_address_node::receive_packet(void *addr, Genode::size_t size);
template void Net::Mac_address_node::receive_packet(void *addr, Genode::size_t size);
