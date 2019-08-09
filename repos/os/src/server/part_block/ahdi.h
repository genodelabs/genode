/*
 * \brief  Atari ST partition scheme (AHDI)
 * \author Norman Feske
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


struct Ahdi
{
	typedef Block::Partition_table::Sector Sector;

	typedef Genode::uint32_t uint32_t;
	typedef Genode::uint8_t  uint8_t;

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

		typedef Genode::String<4> Id;

		Id id() const
		{
			using Genode::Char;
			return Id(Char(_id0), Char(_id1), Char(_id2));
		}

		bool bootable() const { return _flags & 1; }

		bool valid() const { return id() == "BGM" && start.value() > 0; }

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

	static bool valid(Sector const &sector)
	{
		bool any_partition_valid = false;

		Root_sector const root = *sector.addr<Root_sector const *>();
		for (unsigned i = 0; i < MAX_PARTITIONS; i++)
			if (root.partitions[i].valid())
				any_partition_valid = true;

		return any_partition_valid;
	}

	template <typename FN>
	static void for_each_partition(Sector const &sector, FN const &fn)
	{
		Root_sector &root = *sector.addr<Root_sector *>();

		for (unsigned i = 0; i < MAX_PARTITIONS; i++) {
			Partition_record const &part = root.partitions[i];
			if (part.valid())
				fn(i + 1, Block::Partition(part.start.value(), part.length.value()));
		}
	}
};

#endif /* _PART_BLOCK__AHDI_H_ */
