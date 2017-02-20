/*
 * \brief  Partition table definitions
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLK__PARTITION_TABLE_H_
#define _PART_BLK__PARTITION_TABLE_H_

#include <base/env.h>
#include <base/log.h>
#include <block_session/client.h>

#include "driver.h"

namespace Block {
	struct Partition;
	class  Partition_table;
}


struct Block::Partition
{
	Genode::uint64_t lba;     /* logical block address on device */
	Genode::uint64_t sectors; /* number of sectors in patitions */

	Partition(Genode::uint64_t l, Genode::uint64_t s)
	: lba(l), sectors(s) { }
};


struct Block::Partition_table
{
		class Sector
		{
			private:

				Session_client    &_session;
				Packet_descriptor  _p;

			public:

				Sector(Driver       &driver,
				       unsigned long blk_nr,
				       unsigned long count,
				       bool          write = false)
				: _session(driver.session()),
				  _p(_session.dma_alloc_packet(driver.blk_size() * count),
				     write ? Packet_descriptor::WRITE : Packet_descriptor::READ,
				     blk_nr, count)
				{
					_session.tx()->submit_packet(_p);
					_p = _session.tx()->get_acked_packet();
					if (!_p.succeeded())
						Genode::error("Could not access block ",
						              (unsigned long long)_p.block_number());
				}

				~Sector() { _session.tx()->release_packet(_p); }

				template <typename T> T addr() {
					return reinterpret_cast<T>(_session.tx()->packet_content(_p)); }
		};

		Genode::Heap & heap;
		Driver       & driver;

		Partition_table(Genode::Heap & h, Driver & d)
		: heap(h), driver(d) {}

		virtual Partition *partition(int num) = 0;

		virtual bool parse() = 0;
};

#endif /* _PART_BLK__PARTITION_TABLE_H_ */
