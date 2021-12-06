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

namespace Nic { struct Packet_allocator; }


/**
 * Packet allocator used for packet streaming in nic sessions.
 */
struct Nic::Packet_allocator : Genode::Packet_allocator
{
	enum { DEFAULT_PACKET_SIZE = 1600 };

	/**
	 * Constructor
	 *
	 * \param md_alloc  Meta-data allocator
	 */
	Packet_allocator(Genode::Allocator *md_alloc)
	: Genode::Packet_allocator(md_alloc, DEFAULT_PACKET_SIZE) {}

	/**
	 * Override because ethernet frame headers are 14 bytes (src/dst mac (12) +
	 * ethertype (2)) causing the IP header to be 2 byte aligned, leading to
	 * problems on platforms that require load/store operations to be naturally
	 * aligned when reading, for example, 4 byte IP addresses. Therefore, we align
	 * the allocation to 2 bytes, so the IP header is aligned to 4.
	 */
	Alloc_result try_alloc(Genode::size_t size) override
	{
		Alloc_result result = Genode::Packet_allocator::try_alloc(size + 2);

		result.with_result([&] (void *content) {
			result = Alloc_result { reinterpret_cast<void *>((Genode::uint8_t *)content + 2) };
			},
			[](Alloc_error){}
		);

		return result;
	}

	void free(void *addr, Genode::size_t size) override
	{
		Genode::Packet_allocator::free((Genode::uint8_t *)addr - 2, size + 2);
	}


};

#endif /* _INCLUDE__NIC__PACKET_ALLOCATOR__ */
