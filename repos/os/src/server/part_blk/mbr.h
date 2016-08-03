/*
 * \brief  MBR partition table definitions
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PART_BLK__MBR_H_
#define _PART_BLK__MBR_H_

#include <base/env.h>
#include <base/log.h>
#include <block_session/client.h>

#include "partition_table.h"


struct Mbr_partition_table : public Block::Partition_table
{
	public:

		class Protective_mbr_found { };

	private:

		typedef Block::Partition_table::Sector Sector;

		/**
		 * Partition table entry format
		 */
		struct Partition_record
		{
			enum { INVALID = 0, EXTENTED = 0x5, PROTECTIVE = 0xee, };
			Genode::uint8_t  _unused[4];
			Genode::uint8_t  _type;       /* partition type */
			Genode::uint8_t  _unused2[3];
			Genode::uint32_t _lba;        /* logical block address */
			Genode::uint32_t _sectors;    /* number of sectors */

			bool valid()      { return _type != INVALID; }
			bool extented()   { return _type == EXTENTED; }
			bool protective() { return _type == PROTECTIVE; }
		} __attribute__((packed));


		/**
		 * Master/Extented boot record format
		 */
		struct Mbr
		{
			Genode::uint8_t  _unused[446];
			Partition_record _records[4];
			Genode::uint16_t _magic;

			bool valid()
			{
				/* magic number of partition table */
				enum { MAGIC = 0xaa55 };
				return _magic == MAGIC;
			}
		} __attribute__((packed));


		enum { MAX_PARTITIONS = 32 };

		Block::Partition *_part_list[MAX_PARTITIONS]; /* contains pointers to valid
		                                          partitions or 0 */

		void _parse_extented(Partition_record *record)
		{
			Partition_record *r = record;
			unsigned lba = r->_lba;

			/* first logical partition number */
			int nr = 5;
			do {
				Sector s(driver, lba, 1);
				Mbr *ebr = s.addr<Mbr *>();

				if (!(ebr->valid()))
					return;

			/* The first record is the actual logical partition. The lba of this
			 * partition is relative to the lba of the current EBR */
			Partition_record *logical = &(ebr->_records[0]);
			if (logical->valid() && nr < MAX_PARTITIONS) {
				_part_list[nr++] = new (&heap)
					Block::Partition(logical->_lba + lba, logical->_sectors);

				Genode::log("Partition ", nr - 1, ": LBA ", logical->_lba + lba,
				            " (", (unsigned int)logical->_sectors, " blocks) type ",
				            Genode::Hex(logical->_type, Genode::Hex::OMIT_PREFIX));
			}

			/*
			 * the second record points to the next EBR
			 * (relative form this EBR)
			 */
			r = &(ebr->_records[1]);
			lba += ebr->_records[1]._lba;

			} while (r->valid());
		}

		void _parse_mbr(Mbr *mbr)
		{
			/* no partition table, use whole disc as partition 0 */
			if (!(mbr->valid()))
				_part_list[0] = new (&heap)
					Block::Partition(0, driver.blk_cnt() - 1);

			for (int i = 0; i < 4; i++) {
				Partition_record *r = &(mbr->_records[i]);

				if (!r->valid())
					continue;

				Genode::log("Partition ", i + 1, ": LBA ",
				            (unsigned int) r->_lba, " (",
				            (unsigned int) r->_sectors, " blocks) type: ",
				            Genode::Hex(r->_type, Genode::Hex::OMIT_PREFIX));

				if (r->protective())
					throw Protective_mbr_found();

				if (r->extented()) {
					_parse_extented(r);
					continue;
				}

				_part_list[i + 1] = new (&heap)
					Block::Partition(r->_lba, r->_sectors);
			}
		}

	public:

		using Partition_table::Partition_table;

		Block::Partition *partition(int num) {
			return (num < MAX_PARTITIONS) ? _part_list[num] : 0; }

		bool parse()
		{
			Sector s(driver, 0, 1);
			_parse_mbr(s.addr<Mbr *>());
			for (unsigned num = 0; num < MAX_PARTITIONS; num++)
				if (_part_list[num])
					return true;
			return false;
		}
};

#endif /* _PART_BLK__MBR_H_ */
