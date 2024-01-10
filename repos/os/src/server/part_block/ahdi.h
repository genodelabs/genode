/*
 * \brief  Atari ST partition scheme (AHDI)
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2019-08-09
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__AHDI_H_
#define _PART_BLOCK__AHDI_H_

#include "partition_table.h"

namespace Block {
	struct Ahdi_partition;
	struct Ahdi;
}


struct Block::Ahdi_partition : Partition
{
	using Type = String<4>;
	Type type;

	Ahdi_partition(block_number_t lba,
	               block_number_t sectors,
	               Fs::Type       fs_type,
	               Type    const &type)
	:
		Partition(lba, sectors, fs_type),
		type(type)
	{ }
};


class Block::Ahdi : public Partition_table
{
	private:

		/**
		 * 32-bit big-endian value
		 */
		struct Be32
		{
			uint8_t b0, b1, b2, b3;

			uint32_t value() const {
				return ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16)
				     | ((uint32_t)b2 <<  8) | ((uint32_t)b3 <<  0); }

		} __attribute__((packed));

		struct Partition_record
		{
			uint8_t _flags;
			uint8_t _id0, _id1, _id2;
			Be32    start;  /* first block */
			Be32    length; /* in blocks */

			Ahdi_partition::Type id() const
			{
				return { Char(_id0), Char(_id1), Char(_id2) };
			}

			bool bootable() const { return _flags & 1; }

			bool valid() const
			{
				return start.value() > 0
				    && (id() == "BGM" || id() == "GEM" || id() == "LNX");
			}

		} __attribute__((packed));

		enum { MAX_PARTITIONS = 4 };

		struct Root_sector
		{
			uint8_t          boot_code[0x156];
			Partition_record icd_partitions[8];
			uint8_t          unused[0xc];
			Be32             disk_blocks;
			Partition_record partitions[MAX_PARTITIONS];

		} __attribute__((packed));

		Constructible<Ahdi_partition> _part_list[MAX_PARTITIONS];

		bool _valid(Sync_read const &sector)
		{
			bool any_partition_valid = false;

			Root_sector const root = *(Root_sector const *)sector.buffer().start;
			for (unsigned i = 0; i < MAX_PARTITIONS; i++)
				if (root.partitions[i].valid())
					any_partition_valid = true;

			return any_partition_valid;
		}

		template <typename FUNC>
		void _parse_ahdi(Sync_read const &sector, FUNC const &fn)
		{
			Root_sector &root = *(Root_sector *)sector.buffer().start;

			for (unsigned i = 0; i < MAX_PARTITIONS; i++) {
				Partition_record const &part = root.partitions[i];
				if (!part.valid())
					continue;

				fn(i, part);
			}
		}

		template <typename FN>
		void _for_each_valid_partition(FN const &fn) const
		{
			for (unsigned i = 0; i < MAX_PARTITIONS; i++)
				if (_part_list[i].constructed())
					fn(i);
		};

	public:

		using Partition_table::Partition_table;

		bool parse()
		{
			Sync_read s(_handler, _alloc, 0, 1);

			if (!s.success() || !_valid(s))
				return false;

			_parse_ahdi(s, [&] (unsigned i, Partition_record const &r) {
				block_number_t lba    = r.start.value();
				block_number_t length = r.length.value();

				Ahdi_partition::Type type = r.id();

				_part_list[i].construct(lba, length, _fs_type(lba), type);

				log("AHDI Partition ", i + 1, ": LBA ", lba, " (", length,
				    " blocks) type: '", type, "'");
			});

			return true;
		}

		bool partition_valid(long num) const override
		{
			/* 1-based partition number to 0-based array index */
			num -= 1;

			if (num < 0 || num >= MAX_PARTITIONS)
				return false;

			return _part_list[num].constructed();
		}

		block_number_t partition_lba(long num) const override
		{
			return partition_valid(num) ? _part_list[num - 1]->lba : 0;
		}

		block_number_t partition_sectors(long num) const override
		{
			return partition_valid(num) ? _part_list[num - 1]->sectors : 0;
		}

		void generate_report(Xml_generator &xml) const override
		{
			auto gen_partition_attr = [&] (Xml_generator &xml, unsigned i)
			{
				Ahdi_partition const &part = *_part_list[i];

				xml.attribute("number",     i + 1);
				xml.attribute("start",      part.lba);
				xml.attribute("length",     part.sectors);
				xml.attribute("block_size", _info.block_size);
				xml.attribute("type",       part.type);

				if (part.fs_type.valid())
					xml.attribute("file_system", part.fs_type);
			};

			xml.attribute("type", "ahdi");

			_for_each_valid_partition([&] (unsigned i) {
				xml.node("partition", [&] {
					gen_partition_attr(xml, i); }); });
		}
};

#endif /* _PART_BLOCK__AHDI_H_ */
