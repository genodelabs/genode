/*
 * \brief  Generic parts of an SD card block-driver
 * \author Norman Feske
 * \author Timo Wischer
 * \author Martin Stein
 * \date   2017-01-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DRIVER_BASE_H_
#define _DRIVER_BASE_H_

/* Genode includes */
#include <block/component.h>

/* local includes */
#include <sd_card.h>

namespace Sd_card { class Driver_base; }


class Sd_card::Driver_base : public    Block::Driver,
                             protected Host_controller
{
	public:

		Driver_base(Genode::Ram_session &ram)
		: Block::Driver(ram) { }

		/*******************
		 ** Block::Driver **
		 *******************/

		Genode::size_t block_size() override { return 512; }

		Block::sector_t block_count() override {
			return card_info().capacity_mb() * 1024 * 2; }

		Block::Session::Operations ops() override
		{
			Block::Session::Operations ops;
			ops.set_operation(Block::Packet_descriptor::READ);
			ops.set_operation(Block::Packet_descriptor::WRITE);
			return ops;
		}
};

#endif /* _DRIVER_BASE_H_ */
