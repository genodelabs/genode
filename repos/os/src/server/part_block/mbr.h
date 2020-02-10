/*
 * \brief  MBR partition table definitions
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2013-12-04
 */

/*
 * Copyright (C) 2013-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__MBR_H_
#define _PART_BLOCK__MBR_H_

#include <base/env.h>
#include <base/log.h>
#include <block_session/client.h>
#include <util/mmio.h>

#include "partition_table.h"
#include "fsprobe.h"
#include "ahdi.h"

namespace Block {
	struct Mbr_partition_table;
};

struct Block::Mbr_partition_table : public Block::Partition_table
{
	public:

		class Protective_mbr_found { };

	private:

		typedef Block::Partition_table::Sector Sector;

		/**
		 * Partition table entry format
		 */
		struct Partition_record : Mmio
		{
			struct Type : Register<4, 8>
			{
				enum {
					INVALID = 0, EXTENTED_CHS = 0x5, EXTENTED_LBA = 0xf, PROTECTIVE = 0xee
				};
			};

			struct Lba     : Register<8, 32>  { }; /* logical block address */
			struct Sectors : Register<12, 32> { }; /* number of sectors */

			Partition_record() = delete;

			Partition_record(addr_t base)
			: Mmio(base) { }

			bool valid()       const { return read<Type>() != Type::INVALID; }
			bool extended()    const { return read<Type>() == Type::EXTENTED_CHS ||
			                                  read<Type>() == Type::EXTENTED_LBA; }
			bool protective()  const { return read<Type>() == Type::PROTECTIVE; }
			uint8_t  type()    const { return read<Type>(); }
			unsigned lba()     const { return read<Lba>(); }
			unsigned sectors() const { return read<Sectors>(); }

			static constexpr size_t size() { return 16; }
		};

		/**
		 * Master/Extented boot record format
		 */
		struct Mbr : Mmio
		{
			struct Magic : Register<510, 16>
			{
				enum { NUMBER = 0xaa55 };
			};

			Mbr() = delete;

			Mbr(addr_t base) : Mmio(base) { }

			bool valid() const
			{
				/* magic number of partition table */
				return read<Magic>() == Magic::NUMBER;
			}

			addr_t record(unsigned index) const
			{
				return base() + 446 + (index * Partition_record::size());
			}
		};


		enum { MAX_PARTITIONS = 32 };

		/* contains pointers to valid partitions or 0 */
		Constructible<Partition> _part_list[MAX_PARTITIONS];

		template <typename FUNC>
		void _parse_extended(Partition_record const &record, FUNC const &f) const
		{
			Reconstructible<Partition_record const> r(record.base());
			unsigned lba = r->lba();
			unsigned last_lba = 0;

			/* first logical partition number */
			int nr = 5;
			do {
				Sector s(const_cast<Sector_data&>(data), lba, 1);
				Mbr const ebr(s.addr<addr_t>());

				if (!ebr.valid())
					return;

				/* The first record is the actual logical partition. The lba of this
				 * partition is relative to the lba of the current EBR */
				Partition_record const logical(ebr.record(0));
				if (logical.valid() && nr < MAX_PARTITIONS) {
					f(nr++, logical, lba);
				}

				/*
				 * the second record points to the next EBR
				 * (relative form this EBR)
				 */
				r.destruct();
				r.construct(ebr.record(1));
				lba += r->lba() - last_lba;

				last_lba = r->lba();

			} while (r->valid());
		}

		template <typename FUNC>
		void _parse_mbr(Mbr const &mbr, FUNC const &f) const
		{
			for (int i = 0; i < 4; i++) {
				Partition_record const r(mbr.record(i));

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

		Partition &partition(long num) override
		{
			if (num < 0 || num > MAX_PARTITIONS)
				throw -1;

			if (!_part_list[num].constructed())
				throw -1;

			return *_part_list[num];
		}

		template <typename FN>
		void _for_each_valid_partition(FN const &fn) const
		{
			for (unsigned i = 0; i < MAX_PARTITIONS; i++)
				if (_part_list[i].constructed())
					fn(i);
		};

		bool parse() override
		{
			block.sigh(io_sigh);

			Sector s(data, 0, 1);

			/* check for MBR */
			Mbr const mbr(s.addr<addr_t>());
			bool const mbr_valid = mbr.valid();
			if (mbr_valid) {
				_parse_mbr(mbr, [&] (int i, Partition_record const &r, unsigned offset) {
					log("Partition ", i, ": LBA ",
					    r.lba() + offset, " (",
					    r.sectors(), " blocks) type: ",
					    Hex(r.type(), Hex::OMIT_PREFIX));
					if (!r.extended())
						_part_list[i].construct(Partition(r.lba() + offset, r.sectors()));
				});
			}

			/* check for AHDI partition table */
			bool const ahdi_valid = !mbr_valid && Ahdi::valid(s);
			if (ahdi_valid)
				Ahdi::for_each_partition(s, [&] (unsigned i, Partition info) {
					if (i < MAX_PARTITIONS)
						_part_list[i].construct(
							Partition(info.lba, info.sectors)); });

			/* no partition table, use whole disc as partition 0 */
			if (!mbr_valid && !ahdi_valid)
				_part_list[0].construct(
					Partition(0, block.info().block_count - 1));

			/* report the partitions */
			if (reporter.enabled()) {

				auto gen_partition_attr = [&] (Xml_generator &xml, unsigned i)
				{
					Partition const &part = *_part_list[i];

					size_t const block_size = block.info().block_size;

					xml.attribute("number",     i);
					xml.attribute("start",      part.lba);
					xml.attribute("length",     part.sectors);
					xml.attribute("block_size", block_size);

					/* probe for known file-system types */
					enum { PROBE_BYTES = 4096, };
					Sector fs(data, part.lba, PROBE_BYTES / block_size);
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
							if (!_part_list[i].constructed())
								return;

							xml.node("partition", [&] {
								gen_partition_attr(xml, i);
								xml.attribute("type", r.type()); });
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
