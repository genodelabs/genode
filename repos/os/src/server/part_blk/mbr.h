/*
 * \brief  MBR partition table definitions
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLK__MBR_H_
#define _PART_BLK__MBR_H_

#include <base/env.h>
#include <base/log.h>
#include <block_session/client.h>

#include "partition_table.h"
#include "fsprobe.h"


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
			enum {
				INVALID = 0,
				EXTENTED_CHS = 0x5, EXTENTED_LBA = 0xf, PROTECTIVE = 0xee
			};
			Genode::uint8_t  _unused[4];
			Genode::uint8_t  _type;       /* partition type */
			Genode::uint8_t  _unused2[3];
			Genode::uint32_t _lba;        /* logical block address */
			Genode::uint32_t _sectors;    /* number of sectors */

			bool valid()      { return _type != INVALID; }
			bool extended()   { return _type == EXTENTED_CHS
			                        || _type == EXTENTED_LBA; }
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

		template <typename FUNC>
		void _parse_extended(Partition_record *record, FUNC const &f)
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
				f(nr++, logical, lba);
			}

			/*
			 * the second record points to the next EBR
			 * (relative form this EBR)
			 */
			r = &(ebr->_records[1]);
			lba += ebr->_records[1]._lba;

			} while (r->valid());
		}

		template <typename FUNC>
		void _parse_mbr(Mbr *mbr, FUNC const &f)
		{

			for (int i = 0; i < 4; i++) {
				Partition_record *r = &(mbr->_records[i]);

				if (!r->valid())
					continue;

				if (r->protective())
					throw Protective_mbr_found();

				f(i + 1, r, 0);

				if (r->extended()) {
					_parse_extended(r, f);
				}
			}
		}

	public:

		using Partition_table::Partition_table;

		Block::Partition *partition(int num) {
			return (num < MAX_PARTITIONS) ? _part_list[num] : 0; }

		bool parse()
		{
			Sector s(driver, 0, 1);
			Mbr *mbr = s.addr<Mbr *>();

			/* no partition table, use whole disc as partition 0 */
			bool const mbr_valid = mbr->valid();
			if (!mbr_valid) {
				_part_list[0] = new (&heap)
					Block::Partition(0, driver.blk_cnt() - 1);
			} else {
				_parse_mbr(mbr, [&] (int i, Partition_record *r, unsigned offset) {
					Genode::log("Partition ", i, ": LBA ",
					            (unsigned int) r->_lba + offset, " (",
					            (unsigned int) r->_sectors, " blocks) type: ",
					            Genode::Hex(r->_type, Genode::Hex::OMIT_PREFIX));
					if (!r->extended())
						_part_list[i] = new (&heap)
							Block::Partition(r->_lba + offset, r->_sectors);
				});
			}

			/* Report the partitions */
			if (reporter.enabled()) {

				enum { PROBE_BYTES = 4096, };
				Genode::size_t const block_size = driver.blk_size();

				Genode::Reporter::Xml_generator xml(reporter, [&] () {

					if (mbr_valid) {
						xml.attribute("type", "mbr");

						_parse_mbr(mbr, [&] (int i, Partition_record *r, unsigned offset) {

							Sector fs(driver, r->_lba + offset, PROBE_BYTES / block_size);
							Fs::Type fs_type = Fs::probe(fs.addr<Genode::uint8_t*>(), PROBE_BYTES);

							xml.node("partition", [&] {
								xml.attribute("number", i);
								xml.attribute("type", r->_type);
								xml.attribute("start", r->_lba + offset);
								xml.attribute("length", r->_sectors);
								xml.attribute("block_size", driver.blk_size());

								if (fs_type.valid()) {
									xml.attribute("file_system", fs_type);
								}
							});
						});
					} else {
						xml.attribute("type", "disk");

						enum { PART_NUM = 0, };
						Block::Partition const &disk = *_part_list[PART_NUM];

						Sector fs(driver, disk.lba, PROBE_BYTES / block_size);
						Fs::Type fs_type = Fs::probe(fs.addr<Genode::uint8_t*>(), PROBE_BYTES);

						xml.node("partition", [&] {
							xml.attribute("number", PART_NUM);
							xml.attribute("start",  disk.lba);
							xml.attribute("length", disk.sectors + 1);
							xml.attribute("block_size", driver.blk_size());

							if (fs_type.valid()) {
								xml.attribute("file_system", fs_type);
							}
						});
					}
				});
			}

			for (unsigned num = 0; num < MAX_PARTITIONS; num++)
				if (_part_list[num])
					return true;
			return false;
		}
};

#endif /* _PART_BLK__MBR_H_ */
