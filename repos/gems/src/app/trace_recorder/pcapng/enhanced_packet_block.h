/*
 * \brief  Enhanced packet block
 * \author Johannes Schlatow
 * \date   2022-05-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PCAPNG__ENHANCED_PACKET_BLOCK_H_
#define _PCAPNG__ENHANCED_PACKET_BLOCK_H_

/* local includes */
#include <trace_recorder_policy/pcapng.h>

/* Genode includes */
#include <pcapng/block.h>

namespace Pcapng {
	using namespace Genode;

	struct Enhanced_packet_block;
}


/* converts Traced_packet into a Pcapng block structure */
struct Pcapng::Enhanced_packet_block : Block<0x6>
{
	/**
	 * Layout:   -------- 32-bit -------
	 *           |      0x00000006     |
	 *           -----------------------
	 *           |        Length       |
	 *           -----------------------
	 *           |     Interface ID    |
	 *           -----------------------
	 *           |   Timestamp High    |
	 *           -----------------------
	 *           |   Timestamp Low     |
	 *           -----------------------
	 *           |   Captured Length   |
	 *           -----------------------
	 *           |   Original Length   |
	 *           -----------------------
	 *           |    Packet Data      |
	 *           |        ...          |
	 *           |      (padded)       |
	 *           -----------------------
	 *           |        Length       |
	 *           -----------------------
	 */

	uint32_t      _interface_id;
	uint32_t      _timestamp_high;
	uint32_t      _timestamp_low;
	Traced_packet _data;

	enum {
		MAX_CAPTURE_LENGTH = 1600,
		MAX_SIZE = block_size(sizeof(Block_base) +
		                      sizeof(_interface_id) +
		                      sizeof(_timestamp_high) +
		                      sizeof(_timestamp_low) +
		                      sizeof(Traced_packet) +
		                      MAX_CAPTURE_LENGTH)
	};

	Enhanced_packet_block(uint32_t interface_id, Traced_packet const &packet, uint64_t timestamp)
	: _interface_id(interface_id),
	  _timestamp_high((uint32_t)(timestamp >> 32)),
	  _timestamp_low(timestamp & 0xFFFFFFFF),
	  _data(packet)
	{ commit(sizeof(Enhanced_packet_block) + _data.data_length()); }

} __attribute__((packed));

#endif /* _PCAPNG__ENHANCED_PACKET_BLOCK_H_ */
