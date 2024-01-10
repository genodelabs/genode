/*
 * \brief  GUID Partition table definitions
 * \author Josef Soentgen
 * \author Sebastian Sumpf
 * \author Christian Helmuth
 * \date   2014-09-19
 */

/*
 * Copyright (C) 2014-2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__GPT_H_
#define _PART_BLOCK__GPT_H_

#include "partition_table.h"

static bool constexpr verbose = false;

namespace Block {
	class Gpt_partition;
	class Gpt;
};


struct Block::Gpt_partition : Partition
{
	using Uuid = String<40>;
	Uuid guid;
	Uuid type;

	using Name = String<72>; /* use GPT name entry length */
	Name name;

	Gpt_partition(block_number_t lba,
	              block_number_t sectors,
	              Fs::Type       fs_type,
	              Uuid    const &guid,
	              Uuid    const &type,
	              Name    const &name)
	:
		Partition(lba, sectors, fs_type),
		guid(guid), type(type), name(name)
	{ }
};


class Block::Gpt : public Block::Partition_table
{
	private:

		enum { MAX_PARTITIONS = 128 };

		/* contains valid partitions or not constructed */
		Constructible<Gpt_partition> _part_list[MAX_PARTITIONS];

		/**
		 * DCE uuid struct
		 */
		struct Uuid : Mmio<16>
		{
			struct Time_low            : Register<0, 32> { };
			struct Time_mid            : Register<4, 16> { };
			struct Time_hi_and_version : Register<6, 16> { };

			struct Clock_seq_hi_and_reserved : Register<8, 8> { };
			struct Clock_seq_low             : Register<9, 8> { };

			struct Node : Register_array<10, 8, 6, 8> { };

			Uuid() = delete;
			using Mmio::Mmio;

			unsigned time_low() const { return read<Time_low>(); }

			template <typename T> struct Uuid_hex : Genode::Hex {
				Uuid_hex(T value) : Genode::Hex(value, OMIT_PREFIX, PAD) { } };

			void print(Output &out) const
			{
				Genode::print(out, Uuid_hex(read<Time_low>()),
				              "-", Uuid_hex(read<Time_mid>()),
				              "-", Uuid_hex(read<Time_hi_and_version>()),
				              "-", Uuid_hex(read<Clock_seq_hi_and_reserved>()),
				                   Uuid_hex(read<Clock_seq_low>()),
				              "-");

				for (unsigned i = 0; i < 6; i++)
					Genode::print(out, Uuid_hex(read<Node>(i)));
			}
			static constexpr size_t size() { return 16; }
		};


		/**
		 * GUID parition table header
		 */
		struct Gpt_hdr : Mmio<92>
		{
			struct Sig      : Register<0, 64> { };  /* identifies GUID Partition Table */
			struct Revision : Register<8, 32> { };  /* GPT specification revision */
			struct Hdr_size : Register<12, 32> { }; /* size of GPT header */
			struct Hdr_crc  : Register<16, 32> { }; /* CRC32 of GPT header */
			struct Reserved : Register<20,32> { };  /* must be zero */
			struct Hdr_lba  : Register<24, 64> {
				enum { LBA = 1 }; };                  /* LBA that contains this header */

			struct Backup_hdr_lba : Register<32, 64> { }; /* LBA of backup GPT header */
			struct Part_lba_start : Register<40, 64> { }; /* first LBA usable for partitions */
			struct Part_lba_end   : Register<48, 64> { }; /* last LBA usable for partitions */

			Uuid guid() { return Uuid(range_at(56)); } /* GUID to identify the disk */

			struct Gpe_lba    : Register<72, 64> { }; /* first LBA of GPE array */
			struct Entries    : Register<80, 32> { }; /* number of entries in GPE array */
			struct Entry_size : Register<84, 32> { }; /* size of each GPE */
			struct Gpe_crc    : Register<88, 32> { }; /* CRC32 of GPE array */

			Gpt_hdr() = delete;
			using Mmio::Mmio;

			uint64_t part_lba_start() const { return read<Part_lba_start>(); }
			uint64_t part_lba_end()   const { return read<Part_lba_end>(); }
			uint64_t gpe_lba()        const { return read<Gpe_lba>(); }
			uint32_t entries()        const { return read<Entries>(); }
			uint32_t entry_size()     const { return read<Entry_size>(); }

			uint32_t crc32(addr_t buf, size_t size)
			{
				uint8_t const *p = reinterpret_cast<uint8_t const*>(buf);
				uint32_t crc = ~0U;

				while (size--) {
					crc ^= *p++;
					for (uint32_t j = 0; j < 8; j++)
						crc = (-int32_t(crc & 1) & 0xedb88320) ^ (crc >> 1);
				}

				return crc ^ ~0U;
			}

			void dump_hdr(bool check_primary)
			{
				if (!verbose) return;

				log("GPT ", check_primary ? "primary" : "backup", " header:");
				log(" rev: ",            read<Revision>());
				log(" size: ",           read<Hdr_size>());
				log(" crc: ",        Hex(read<Hdr_crc>(), Hex::OMIT_PREFIX));
				log(" reserved: ",       read<Reserved>());
				log(" hdr lba: ",        read<Hdr_lba>());
				log(" bak lba: ",        read<Backup_hdr_lba>());
				log(" part start lba: ", read<Part_lba_start>());
				log(" part end lba: ",   read<Part_lba_end>());
				log(" guid: ",           guid());
				log(" gpe lba: ",        read<Gpe_lba>());
				log(" entries: ",        read<Entries>());
				log(" entry size: ",     read<Entry_size>());
				log(" gpe crc: ",    Hex(read<Gpe_crc>(), Hex::OMIT_PREFIX));
			}

			bool valid(Sync_read::Handler &handler, Allocator &alloc,
			           size_t block_size, bool check_primary = true)
			{
				dump_hdr(check_primary);

				/* check sig */
				uint64_t const magic = 0x5452415020494645; /* "EFI PART" - ascii */;
				if (read<Sig>() != magic) {
					return false;
				}

				/* check header crc */
				uint32_t crc = read<Hdr_crc>();
				write<Hdr_crc>(0);
				if (crc32(base(), read<Hdr_size>()) != crc) {
					error("Wrong GPT header checksum");
					return false;
				}

				/* check header lba */
				if (check_primary)
					if (read<Hdr_lba>() != Hdr_lba::LBA)
						return false;

				/* check GPT entry array */
				size_t length = entries() * entry_size();
				Sync_read gpe(handler, alloc, gpe_lba(), length / block_size);
				if (!gpe.success()
				 || crc32((addr_t)gpe.buffer().start, length) != read<Gpe_crc>())
					return false;

				if (check_primary) {
					/* check backup gpt header */
					Sync_read backup_hdr(handler, alloc, read<Backup_hdr_lba>(), 1);
					if (!backup_hdr.success())
						return false;

					Gpt_hdr backup(backup_hdr.buffer());
					if (!backup.valid(handler, alloc, block_size, false))
						warning("Backup GPT header is corrupted");
				}

				return true;
			}

			/* the remainder of the LBA must be zero */
		};


		/* state needed for generating the partitions report */
		uint64_t _gpt_part_lba_end { 0 };
		uint64_t _gpt_total        { 0 };
		uint64_t _gpt_used         { 0 };


		/**
		 * GUID partition entry format
		 */
		struct Gpt_entry : Mmio<56 + 36 * 2>
		{
			enum { NAME_LEN = 36 };

			Uuid  type() const { return Uuid(range()); }                 /* partition type GUID */
			Uuid  guid() const { return Uuid(range_at(Uuid::size())); }  /* unique partition GUID */

			struct Lba_start : Register<32, 64> { };                     /* start of partition */
			struct Lba_end   : Register<40, 64> { };                     /* end of partition */
			struct Attr      : Register<48, 64> { };                     /* partition attributes */
			struct Name      : Register_array<56, 16, NAME_LEN, 16> { }; /* partition name in UNICODE-16 */

			Gpt_entry() = delete;
			using Mmio::Mmio;

			uint64_t lba_start() const { return read<Lba_start>(); }
			uint64_t lba_end() const { return read<Lba_end>(); }

			bool valid() const
			{
				if (type().time_low() == 0x00000000)
					return false;

				return true;
			}

			/**
			 * Extract UTF-8 for name entry
			 */
			void print(Output &out) const
			{
				for (unsigned i = 0; i < NAME_LEN; i++) {
					uint32_t utf16 = read<Name>(i);

					if (utf16 == 0) break;

					Codepoint code { utf16 };
					code.print(out);
				}
			}
		};


		/**
		 * Iterate over each constructed partition and execute FN
		 */
		template <typename FN>
		void _for_each_valid_partition(FN const &fn) const
		{
			for (unsigned i = 0; i < MAX_PARTITIONS; i++)
				if (_part_list[i].constructed())
					fn(i);
		};


		/**
		 * Calculate free blocks until the start of the logical next entry
		 *
		 * \param entry   number of current entry
		 * \param total   number of total blocks on the device
		 *
		 * \return the number of free blocks to the next logical entry
		 */
		uint64_t _calculate_gap(uint32_t entry,
		                        uint64_t total_blocks) const
		{
			Partition const &current = *_part_list[entry];

			/* add one block => end == start */
			uint64_t const end_lba = current.lba + current.sectors;

			enum { INVALID_START = ~0ull, };
			uint64_t next_start_lba = INVALID_START;

			_for_each_valid_partition([&] (unsigned i) {

				if (i == entry)
					return;

				Partition const &p = *_part_list[i];

				/*
				 * Check if the entry starts afterwards and save the
				 * entry with the smallest distance.
				 */
				if (p.lba >= end_lba)
					next_start_lba = min(next_start_lba, p.lba);
			});

			/* sanity check if GPT is broken */
			if (end_lba > _gpt_part_lba_end) { return 0; }

			/* if the underlying Block device changes we might be able to expand more */
			uint64_t const part_end = max(_gpt_part_lba_end, total_blocks);

			/*
			 * Use stored next start LBA or paritions end LBA from header,
			 * if there is no other entry or we are the only one.
			 */
			return (next_start_lba == INVALID_START ? part_end
			                                        : next_start_lba) - end_lba;
		}


		/**
		 * Calculate total used blocks
		 *
		 * \param header   pointer to GPT header
		 * \param entries  pointer to entries
		 * \param num      number of entries
		 *
		 * \return the number of used blocks
		 */
		uint64_t _calculate_used(Gpt_hdr   const &header,
		                         Gpt_entry const &entries,
		                         uint32_t num)
		{
			uint64_t used = 0;

			for (uint32_t i = 0; i < num; i++) {
				Gpt_entry const e(entries.range_at(i * header.entry_size()));

				if (!e.valid()) { continue; }

				uint64_t const v = (e.lba_end() - e.lba_start()) + 1;
				used += v;
			}

			return used;
		}


		/**
		 * Parse the GPT header
		 */
		bool _parse_gpt(Gpt_hdr &gpt)
		{
			if (!gpt.valid(_handler, _alloc, _info.block_size))
				return false;

			Sync_read entry_array(_handler, _alloc, gpt.gpe_lba(),
			                      gpt.entries() * gpt.entry_size() / _info.block_size);
			if (!entry_array.success())
				return false;
			Gpt_entry entries(entry_array.buffer());

			_gpt_part_lba_end = gpt.part_lba_end();
			_gpt_total        = (gpt.part_lba_end() - gpt.part_lba_start()) + 1;
			_gpt_used         = _calculate_used(gpt, entries, gpt.entries());

			for (int i = 0; i < MAX_PARTITIONS; i++) {

				Gpt_entry e(entries.range_at(i * gpt.entry_size()));

				if (!e.valid())
					continue;

				block_number_t const lba    = e.lba_start();
				block_number_t const length = e.lba_end() - lba + 1;

				String<40>                  guid { e.guid() };
				String<40>                  type { e.type() };
				String<Gpt_entry::NAME_LEN> name { e };

				_part_list[i].construct(lba, length, _fs_type(lba), guid, type, name);

				log("GPT Partition ", i + 1, ": LBA ", lba, " (", length,
				    " blocks) type: '", type,
				    "' name: '", name, "'");
			}

			return true;
		}

	public:

		using Partition_table::Partition_table;

		bool parse()
		{
			Sync_read s(_handler, _alloc, Gpt_hdr::Hdr_lba::LBA, 1);
			if (!s.success())
				return false;

			Gpt_hdr gpt_hdr(s.buffer());

			if (!_parse_gpt(gpt_hdr))
				return false;

			for (unsigned num = 0; num < MAX_PARTITIONS; num++)
				if (_part_list[num].constructed())
					return true;
			return false;
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
			xml.attribute("type", "gpt");

			uint64_t const total_blocks = _info.block_count;
			xml.attribute("total_blocks", total_blocks);

			xml.attribute("gpt_total", _gpt_total);
			xml.attribute("gpt_used",  _gpt_used);

			_for_each_valid_partition([&] (unsigned i) {

				Gpt_partition const &part = *_part_list[i];

				xml.node("partition", [&] () {
					xml.attribute("number",     i + 1);
					xml.attribute("name",       part.name);
					xml.attribute("type",       part.type);
					xml.attribute("guid",       part.guid);
					xml.attribute("start",      part.lba);
					xml.attribute("length",     part.sectors);
					xml.attribute("block_size", _info.block_size);

					uint64_t const gap =
						_calculate_gap(i, total_blocks);
					if (gap)
						xml.attribute("expandable", gap);

					if (part.fs_type.valid())
						xml.attribute("file_system", part.fs_type);
				});
			});
		}
};

#endif /* _PART_BLOCK__GUID_PARTITION_TABLE_H_ */
