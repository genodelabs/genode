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
	protected:

		Genode::size_t _block_size() const { return 512; }

		Block::sector_t _block_count() const {
			return card_info().capacity_mb() * 1024 * 2; }

	public:

		Driver_base(Genode::Ram_allocator &ram)
		: Block::Driver(ram) { }

		/*******************
		 ** Block::Driver **
		 *******************/

		Block::Session::Info info() const override
		{
			return { .block_size  = _block_size(),
			         .block_count = _block_count(),
			         .align_log2  = Genode::log2(_block_size()),
			         .writeable   = true };
		}
};

#endif /* _DRIVER_BASE_H_ */
