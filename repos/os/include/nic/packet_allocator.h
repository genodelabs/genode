/*
 * \brief  Fast-bitmap allocator for NIC-session packet streams
 * \author Sebastian Sumpf
 * \date   2012-07-30
 *
 * This allocator can be used with a nic session. It is *not* required though.
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__NIC__PACKET_ALLOCATOR__
#define _INCLUDE__NIC__PACKET_ALLOCATOR__

#include <os/packet_allocator.h>
#include <base/log.h>

namespace Nic { struct Packet_allocator; }


/**
 * Packet allocator used for packet streaming in nic sessions.
 *
 * We override the allocator interface to align the IP packet to a 32-bit
 * address. The ethernet frame header contains src/dst mac (12) + ethertype (2)
 * causing the IP header to be at offset 14 in the packet. This leads to
 * problems on platforms that require load/store operations to be naturally
 * aligned when reading, for example, 4 byte IP addresses. Therefore, we
 * allocate packet size plus OFFSET and offset the returned packet allocation
 * at 2 bytes, which effectively aligns the IP header to 4 bytes.
 *
 * Note, this tweak reduces the usable bytes in the allocated packets to
 * DEFAULT_PACKET_SIZE - OFFSET and assumes word-aligned allocations in the
 * Genode::Packet_allocator. As DEFAULT_PACKET_SIZE is used for the
 * transmission-buffer calculation we could not change it without breaking the
 * API. OFFSET_PACKET_SIZE reflects the actual (usable) packet-buffer size.
 */
struct Nic::Packet_allocator : Genode::Packet_allocator
{
	enum {
		DEFAULT_PACKET_SIZE = 1600,
		OFFSET = 2,
		OFFSET_PACKET_SIZE = DEFAULT_PACKET_SIZE - OFFSET,
	};

	typedef Genode::size_t size_t;

	/**
	 * Constructor
	 *
	 * \param md_alloc  Meta-data allocator
	 */
	Packet_allocator(Genode::Allocator *md_alloc)
	: Genode::Packet_allocator(md_alloc, DEFAULT_PACKET_SIZE) {}

	Alloc_result try_alloc(size_t size) override
	{
		if (!size || size > OFFSET_PACKET_SIZE) {
			Genode::error("unsupported NIC packet size ", size);
			return Alloc_result { Alloc_error::DENIED };
		}

		Alloc_result result = Genode::Packet_allocator::try_alloc(size + OFFSET);

		result.with_result(
			[&] (void *content) {
				/* assume word-aligned packet buffer and offset packet by 2 bytes */
				if ((Genode::addr_t)content & 0b11) {
					Genode::error("NIC packet allocation not word-aligned");
					result = { Alloc_error::DENIED };
				} else {
					result = Alloc_result {
						reinterpret_cast<void *>((Genode::uint8_t *)content + OFFSET) };
				}
			},
			[] (Alloc_error) { }
		);

		return result;
	}

	void free(void *addr, size_t size) override
	{
		if (!size || size > OFFSET_PACKET_SIZE) {
			Genode::error("unsupported NIC packet size ", size);
			return;
		}

		Genode::Packet_allocator::free((Genode::uint8_t *)addr - OFFSET, size + OFFSET);
	}


};

#endif /* _INCLUDE__NIC__PACKET_ALLOCATOR__ */
