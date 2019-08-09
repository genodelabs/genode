/*
 * \brief  MBR partition table definitions
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__MBR_H_
#define _PART_BLOCK__MBR_H_

#include <base/env.h>
#include <base/log.h>
#include <block_session/client.h>

#include "partition_table.h"
#include "fsprobe.h"
#include "ahdi.h"


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

			bool valid()      const { return _type != INVALID; }
			bool extended()   const { return _type == EXTENTED_CHS
			                              || _type == EXTENTED_LBA; }
			bool protective() const { return _type == PROTECTIVE; }
		} __attribute__((packed));


		/**
		 * Master/Extented boot record format
		 */
		struct Mbr
		{
			Genode::uint8_t  _unused[446];
			Partition_record _records[4];
			Genode::uint16_t _magic;

			bool valid() const
			{
				/* magic number of partition table */
				enum { MAGIC = 0xaa55 };
				return _magic == MAGIC;
			}
		} __attribute__((packed));


		enum { MAX_PARTITIONS = 32 };

		/* contains pointers to valid partitions or 0 */
		Block::Partition *_part_list[MAX_PARTITIONS];

		template <typename FUNC>
		void _parse_extended(Partition_record const &record, FUNC const &f) const
		{
			Partition_record const *r = &record;
			unsigned lba = r->_lba;
			unsigned last_lba = 0;

			/* first logical partition number */
			int nr = 5;
			do {
				Sector s(driver, lba, 1);
				Mbr const &ebr = *s.addr<Mbr *>();

				if (!ebr.valid())
					return;

				/* The first record is the actual logical partition. The lba of this
				 * partition is relative to the lba of the current EBR */
				Partition_record const &logical = ebr._records[0];
				if (logical.valid() && nr < MAX_PARTITIONS) {
					f(nr++, logical, lba);
				}

				/*
				 * the second record points to the next EBR
				 * (relative form this EBR)
				 */
				r = &(ebr._records[1]);
				lba += ebr._records[1]._lba - last_lba;

				last_lba = ebr._records[1]._lba;

			} while (r->valid());
		}

		template <typename FUNC>
		void _parse_mbr(Mbr const &mbr, FUNC const &f) const
		{
			for (int i = 0; i < 4; i++) {
				Partition_record const &r = mbr._records[i];

				if (!r.valid())
					continue;

				if (r.protective())
					throw Protective_mbr_found();

				f(i + 1, r, 0);

				if (r.extended())
					_parse_extended(r, f);
			}
		}

	public:

		using Partition_table::Partition_table;

		Block::Partition *partition(int num) override {
			return (num < MAX_PARTITIONS) ? _part_list[num] : 0; }

		template <typename FN>
		void _for_each_valid_partition(FN const &fn) const
		{
			for (unsigned i = 0; i < MAX_PARTITIONS; i++)
				if (_part_list[i])
					fn(i);
		};

		bool parse() override
		{
			using namespace Genode;

			Sector s(driver, 0, 1);

			/* check for MBR */
			Mbr const &mbr = *s.addr<Mbr *>();
			bool const mbr_valid = mbr.valid();
			if (mbr_valid) {
				_parse_mbr(mbr, [&] (int i, Partition_record const &r, unsigned offset) {
					log("Partition ", i, ": LBA ",
					    (unsigned int) r._lba + offset, " (",
					    (unsigned int) r._sectors, " blocks) type: ",
					    Hex(r._type, Hex::OMIT_PREFIX));
					if (!r.extended())
						_part_list[i] = new (heap)
							Block::Partition(r._lba + offset, r._sectors);
				});
			}

			/* check for AHDI partition table */
			bool const ahdi_valid = !mbr_valid && Ahdi::valid(s);
			if (ahdi_valid)
				Ahdi::for_each_partition(s, [&] (unsigned i, Block::Partition info) {
					if (i < MAX_PARTITIONS)
						_part_list[i] = new (heap)
							Block::Partition(info.lba, info.sectors); });

			/* no partition table, use whole disc as partition 0 */
			if (!mbr_valid && !ahdi_valid)
				_part_list[0] = new (&heap)
					Block::Partition(0, driver.blk_cnt() - 1);

			/* report the partitions */
			if (reporter.enabled()) {

				auto gen_partition_attr = [&] (Xml_generator &xml, unsigned i)
				{
					Block::Partition const &part = *_part_list[i];

					size_t const block_size = driver.blk_size();

					xml.attribute("number",     i);
					xml.attribute("start",      part.lba);
					xml.attribute("length",     part.sectors);
					xml.attribute("block_size", block_size);

					/* probe for known file-system types */
					enum { PROBE_BYTES = 4096, };
					Sector fs(driver, part.lba, PROBE_BYTES / block_size);
					Fs::Type const fs_type =
						Fs::probe(fs.addr<uint8_t*>(), PROBE_BYTES);

					if (fs_type.valid())
						xml.attribute("file_system", fs_type);
				};

				Reporter::Xml_generator xml(reporter, [&] () {

					xml.attribute("type", mbr_valid  ? "mbr"  :
					                      ahdi_valid ? "ahdi" :
					                                   "disk");

					if (mbr_valid) {
						_parse_mbr(mbr, [&] (int i, Partition_record const &r, unsigned) {

							/* nullptr if extended */
							if (!_part_list[i])
								return;

							xml.node("partition", [&] {
								gen_partition_attr(xml, i);
								xml.attribute("type", r._type); });
						});

					} else if (ahdi_valid) {
						_for_each_valid_partition([&] (unsigned i) {
							xml.node("partition", [&] {
								gen_partition_attr(xml, i);
								xml.attribute("type", "bgm"); }); });

					} else {

						xml.node("partition", [&] {
							gen_partition_attr(xml, 0); });
					}
				});
			}

			bool any_partition_valid = false;
			_for_each_valid_partition([&] (unsigned) {
				any_partition_valid = true; });

			return any_partition_valid;
		}
};

#endif /* _PART_BLOCK__MBR_H_ */
