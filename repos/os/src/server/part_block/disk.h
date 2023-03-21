/*
 * \brief  Entire disk a partition 0
 * \author Christian Helmuth
 * \date   2023-03-17
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__DISK_H_
#define _PART_BLOCK__DISK_H_

#include "partition_table.h"

namespace Block { struct Disk; }


class Block::Disk : public Partition_table
{
	private:

		Partition _part { 0, _info.block_count, _fs_type(0) };

	public:

		Disk(Sync_read::Handler &handler,
		     Allocator          &alloc,
		     Session::Info       info)
		:
			Partition_table(handler, alloc, info)
		{
			log("DISK Partition 0: LBA ", _part.lba, " (", _part.sectors, " blocks)");
		}

		bool partition_valid(long num) const override
		{
			return num == 0;
		}

		block_number_t partition_lba(long num) const override
		{
			return partition_valid(num) ? _part.lba : 0;
		}

		block_number_t partition_sectors(long num) const override
		{
			return partition_valid(num) ? _part.sectors : 0;
		}

		void generate_report(Xml_generator &xml) const override
		{
			xml.attribute("type", "disk");

			xml.node("partition", [&] {
				xml.attribute("number",     0);
				xml.attribute("start",      _part.lba);
				xml.attribute("length",     _part.sectors);
				xml.attribute("block_size", _info.block_size);

				if (_part.fs_type.valid())
					xml.attribute("file_system", _part.fs_type);
			});
		}
};

#endif /* _PART_BLOCK__DISK_H_ */
