/*
 * \brief  Section header block
 * \author Johannes Schlatow
 * \date   2022-05-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PCAPNG__SECTION_HEADER_BLOCK_H_
#define _PCAPNG__SECTION_HEADER_BLOCK_H_

/* local includes */
#include <pcapng/block.h>

namespace Pcapng {
	using namespace Genode;

	struct Section_header_block;
}


struct Pcapng::Section_header_block : Block<0x0A0D0D0A>
{
	/**
	 * Layout:   ----- 32-bit ----
	 *           |   0x0A0D0D0A  |
	 *           -----------------
	 *           |     Length    |
	 *           -----------------
	 *           |   0x1A2B3C4D  |
	 *           -----------------
	 *           | Major | Minor |
	 *           -----------------
	 *           | SectionLen Hi |
	 *           -----------------
	 *           | SectionLen Lo |
	 *           -----------------
	 *           |     Length    |
	 *           -----------------
	 */

	uint32_t const _byte_order_magic { 0x1A2B3C4D };
	uint16_t const _major_version    { 1 };
	uint16_t const _minor_version    { 0 };
	uint64_t const _section_length   { 0xFFFFFFFFFFFFFFFF }; /* unspecified */

	enum : size_t {
		MAX_SIZE = block_size(sizeof(Block_base) +
		                      sizeof(_byte_order_magic) +
		                      sizeof(_major_version) +
		                      sizeof(_minor_version) +
		                      sizeof(_section_length))
	};

	Section_header_block()
	{ commit(sizeof(Section_header_block)); }

	/**
	 * XXX instead of using an unspecified section length, we could add an
	 * interface similar to Ctf::Packet_header for append sub-blocks and
	 * keeping track of the length field.
	 */

} __attribute__((packed));

#endif /* _PCAPNG__SECTION_HEADER_BLOCK_H_ */
