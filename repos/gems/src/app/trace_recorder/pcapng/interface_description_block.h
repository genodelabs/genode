/*
 * \brief  Interface description block
 * \author Johannes Schlatow
 * \date   2022-05-12
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PCAPNG__INTERFACE_DESCRIPTION_BLOCK_H_
#define _PCAPNG__INTERFACE_DESCRIPTION_BLOCK_H_

/* local includes */
#include <pcapng/block.h>
#include <pcapng/option.h>

/* genode includes */
#include <trace_recorder_policy/pcapng.h>

namespace Pcapng {
	using namespace Genode;

	struct Interface_description_block;
}


struct Pcapng::Interface_description_block : Block<0x1>
{
	/**
	 * Layout:   -------- 32-bit -------
	 *           |      0x00000001     |
	 *           -----------------------
	 *           |        Length       |
	 *           -----------------------
	 *           | LinkType | Reserved |
	 *           -----------------------
	 *           |       SnapLen       |
	 *           -----------------------
	 *           |  0x0002  | NameLen  |
	 *           -----------------------
	 *           |        Name         |
	 *           |        ...          |
	 *           |      (padded)       |
	 *           -----------------------
	 *           |  0x0001  |  0x0000  |
	 *           -----------------------
	 *           |        Length       |
	 *           -----------------------
	 */

	uint16_t const _link_type;
	uint16_t const _reserved   { 0 };
	uint32_t       _snaplen;
	uint32_t       _data[0]    { };

	enum {
		MAX_SIZE = block_size(sizeof(Block_base) +
		                      Interface_name::MAX_NAME_LEN +
		                      sizeof(_link_type) +
		                      sizeof(_reserved) +
		                      sizeof(_snaplen) +
		                      sizeof(Option_ifname) +
		                      sizeof(Option_end))
	};

	Interface_description_block(Interface_name const &name, uint32_t snaplen)
	: _link_type(name._link_type),
	  _snaplen(snaplen)
	{
		Option_ifname &opt_ifname = *construct_at<Option_ifname>(&_data[0], name);
		Option_end    &opt_end    = *construct_at<Option_end>   (&_data[opt_ifname.total_length()/4]);

		commit((uint32_t)sizeof(Interface_description_block) + opt_ifname.total_length() + opt_end.total_length());
	}

} __attribute__((packed));

#endif /* _PCAPNG__INTERFACE_DESCRIPTION_BLOCK_H_ */
