/*
 * \brief  MBR partition table definitions
 * \author Sebastian Sumpf
 * \author Stefan Kalkowski
 * \author Josef Soentgen
 * \author Norman Feske
 * \author Christian Helmuth
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

#include "partition_table.h"

namespace Block {
	struct Mbr_partition;
	struct Mbr;
};


struct Block::Mbr_partition : Partition
{
	uint8_t const type;

	Mbr_partition(block_number_t lba,
	              block_number_t sectors,
	              Fs::Type       fs_type,
	              uint8_t        type)
	:
		Partition(lba, sectors, fs_type),
		type(type)
	{ }
};


class Block::Mbr : public Partition_table
{
	public:

		enum class Parse_result { MBR, PROTECTIVE_MBR, NO_MBR };

	private:

		/**
		 * Partition table entry format
		 */
		struct Partition_record : Mmio<16>
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

			using Mmio::Mmio;

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
		struct Boot_record : Mmio<512>
		{
			struct Magic : Register<510, 16>
			{
				enum { NUMBER = 0xaa55 };
			};

			Boot_record() = delete;

			using Mmio::Mmio;

			bool valid() const
			{
				/* magic number of partition table */
				return read<Magic>() == Magic::NUMBER;
			}

			Byte_range_ptr record(unsigned index) const
			{
				return range_at(446 + (index * Partition_record::size()));
			}
		};

		enum { MAX_PARTITIONS = 32 };

		Constructible<Mbr_partition> _part_list[MAX_PARTITIONS];

		template <typename FUNC>
		void _parse_extended(Partition_record const &record, FUNC const &f) const
		{
			Reconstructible<Partition_record const> r(record.range());
			unsigned lba = r->lba();
			unsigned last_lba = 0;

			/* first logical partition number */
			int nr = 5;
			do {
				Sync_read s(_handler, _alloc, lba, 1);
				if (!s.success())
					return;

				Boot_record const ebr(s.buffer());
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
		Parse_result _parse_mbr(Boot_record const &mbr, FUNC const &f) const
		{
			for (int i = 0; i < 4; i++) {
				Partition_record const r(mbr.record(i));

				if (!r.valid())
					continue;

				if (r.protective())
					return Parse_result::PROTECTIVE_MBR;

				f(i + 1, r, 0);

				if (r.extended())
					_parse_extended(r, f);
			}

			return Parse_result::MBR;
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

		Parse_result parse()
		{
			Sync_read s(_handler, _alloc, 0, 1);
			if (!s.success())
				return Parse_result::NO_MBR;

			/* check for MBR */
			Boot_record const mbr(s.buffer());
			if (!mbr.valid())
				return Parse_result::NO_MBR;

			return _parse_mbr(mbr, [&] (int nr, Partition_record const &r, unsigned offset)
			{
				if (!r.extended()) {
					block_number_t const lba = r.lba() + offset;

					_part_list[nr - 1].construct(lba, r.sectors(), _fs_type(lba), r.type());
				}

				log("MBR Partition ", nr, ": LBA ",
				    r.lba() + offset, " (",
				    r.sectors(), " blocks) type: ",
				    Hex(r.type(), Hex::OMIT_PREFIX));
			});
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
				Mbr_partition const &part = *_part_list[i];

				xml.attribute("number",     i + 1);
				xml.attribute("start",      part.lba);
				xml.attribute("length",     part.sectors);
				xml.attribute("block_size", _info.block_size);
				xml.attribute("type",       part.type);

				if (part.fs_type.valid())
					xml.attribute("file_system", part.fs_type);
			};

			xml.attribute("type", "mbr");

			_for_each_valid_partition([&] (unsigned i) {
				xml.node("partition", [&] {
					gen_partition_attr(xml, i); }); });
		}
};

#endif /* _PART_BLOCK__MBR_H_ */
