/*
 * \brief  Partition table definitions
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Christian Helmuth
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__PARTITION_TABLE_H_
#define _PART_BLOCK__PARTITION_TABLE_H_

#include "block.h"
#include "fsprobe.h"
#include "types.h"

namespace Block {
	struct Partition;
	class  Partition_table;
}


struct Block::Partition : Noncopyable
{
	block_number_t const lba;     /* logical block address on device */
	block_number_t const sectors; /* number of sectors in partitions */

	Fs::Type fs_type { };

	Partition(block_number_t lba,
	          block_number_t sectors,
	          Fs::Type       fs_type)
	: lba(lba), sectors(sectors), fs_type(fs_type) { }
};


class Block::Partition_table : Interface, Noncopyable
{
	protected:

		Sync_read::Handler  &_handler;
		Allocator           &_alloc;
		Session::Info const  _info;

		Fs::Type _fs_type(block_number_t lba)
		{
			/* probe for known file-system types */
			enum { BYTES = 4096 };
			Sync_read fs(_handler, _alloc, lba, BYTES / _info.block_size);
			if (fs.success())
				return Fs::probe((uint8_t *)fs.buffer().start, BYTES);
			else
				return Fs::Type();
		}

	public:

		Partition_table(Sync_read::Handler &handler,
		                Allocator          &alloc,
		                Session::Info       info)
		: _handler(handler), _alloc(alloc), _info(info) { }

		virtual bool           partition_valid(long num)   const = 0;
		virtual block_number_t partition_lba(long num)     const = 0;
		virtual block_number_t partition_sectors(long num) const = 0;

		virtual void generate_report(Xml_generator &xml) const = 0;
};

#endif /* _PART_BLOCK__PARTITION_TABLE_H_ */
