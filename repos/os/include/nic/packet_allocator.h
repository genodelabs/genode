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
};

#endif /* _INCLUDE__NIC__PACKET_ALLOCATOR__ */
