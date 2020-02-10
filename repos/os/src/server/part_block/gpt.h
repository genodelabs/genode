/*
 * \brief  GUID Partition table definitions
 * \author Josef Soentgen
 * \author Sebastian Sumpf
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

#include <base/env.h>
#include <base/log.h>
#include <block_session/client.h>
#include <util/misc_math.h>
#include <util/mmio.h>
#include <util/utf8.h>

#include "partition_table.h"
#include "fsprobe.h"

static bool constexpr verbose = false;

namespace Block {
	class Gpt;
};

class Block::Gpt : public Block::Partition_table
{
	private:

		enum { MAX_PARTITIONS = 128 };

		/* contains valid partitions or not constructed */
		Constructible<Partition> _part_list[MAX_PARTITIONS];

		typedef Block::Partition_table::Sector Sector;

		/**
		 * DCE uuid struct
		 */
		struct Uuid : Mmio
		{
			struct Time_low            : Register<0, 32> { };
			struct Time_mid            : Register<4, 16> { };
			struct Time_hi_and_version : Register<6, 16> { };

			struct Clock_seq_hi_and_reserved : Register<8, 8> { };
			struct Clock_seq_low             : Register<9, 8> { };

			struct Node : Register_array<10, 8, 6, 8> { };

			Uuid() = delete;
			Uuid(addr_t base) : Mmio(base) { };

			unsigned time_low() const { return read<Time_low>(); }

			template<typename T> struct Uuid_hex : Genode::Hex {
				Uuid_hex<T>(T value) : Genode::Hex(value, OMIT_PREFIX, PAD) { } };

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
		struct Gpt_hdr : Mmio

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

			Uuid guid() { return Uuid(base() + 56); } /* GUID to identify the disk */

			struct Gpe_lba    : Register<72, 64> { }; /* first LBA of GPE array */
			struct Entries    : Register<80, 32> { }; /* number of entries in GPE array */
			struct Entry_size : Register<84, 32> { }; /* size of each GPE */
			struct Gpe_crc    : Register<88, 32> { }; /* CRC32 of GPE array */

			Gpt_hdr() = delete;
			Gpt_hdr(addr_t base) : Mmio(base) { };

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

			bool valid(Partition_table::Sector_data &data, bool check_primary = true)
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
				Sector gpe(data, gpe_lba(), length / data.block.info().block_size);
				if (crc32(gpe.addr<addr_t>(), length) != read<Gpe_crc>())
					return false;

				if (check_primary) {
					/* check backup gpt header */
					Sector backup_hdr(data, read<Backup_hdr_lba>(), 1);
					Gpt_hdr backup(backup_hdr.addr<addr_t>());
					if (!backup.valid(data, false)) {
						warning("Backup GPT header is corrupted");
					}
				}

				return true;
			}

			/* the remainder of the LBA must be zero */
		};


		/**
		 * GUID partition entry format
		 */
		struct Gpt_entry : Mmio
		{
			enum { NAME_LEN = 36 };

			Uuid  type() const { return Uuid(base()); }                /* partition type GUID */
			Uuid  guid() const { return Uuid(base()+ Uuid::size()); }  /* unique partition GUID */

			struct Lba_start : Register<32, 64> { };                     /* start of partition */
			struct Lba_end   : Register<40, 64> { };                     /* end of partition */
			struct Attr      : Register<48, 64> { };                     /* partition attributes */
			struct Name      : Register_array<56, 16, NAME_LEN, 16> { }; /* partition name in UNICODE-16 */

			Gpt_entry() = delete;
			Gpt_entry(addr_t base) : Mmio(base) { }

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
		 * Calculate free blocks until the start of the logical next entry
		 *
		 * \param header   pointer to GPT header
		 * \param entry    pointer to current entry
		 * \param entries  pointer to entries
		 * \param num      number of entries
		 *
		 * \return the number of free blocks to the next logical entry
		 */
		uint64_t _calculate_gap(Gpt_hdr   const &header,
		                        Gpt_entry const &entry,
		                        Gpt_entry const &entries,
		                        Genode::uint32_t num,
		                        Genode::uint64_t total_blocks)
		{
			/* add one block => end == start */
			uint64_t const end_lba = entry.lba_end() + 1;

			enum { INVALID_START = ~0ull, };
			uint64_t next_start_lba = INVALID_START;

			for (uint32_t i = 0; i < num; i++) {
				Gpt_entry const e(entries.base() + i * header.entry_size());

				if (!e.valid() || e.base() == entry.base()) { continue; }

				/*
				 * Check if the entry starts afterwards and save the
				 * entry with the smallest distance.
				 */
				if (e.lba_start() >= end_lba) {
					next_start_lba = min(next_start_lba, e.lba_start());
				}
			}

			/* sanity check if GPT is broken */
			if (end_lba > header.part_lba_end()) { return 0; }

			/* if the underyling Block device changes we might be able to expand more */
			uint64_t const part_end = max(header.part_lba_end(), total_blocks);

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
		uint64_t _calculate_used(Gpt_hdr const &header,
		                                 Gpt_entry const &entries,
		                                 uint32_t num)
		{
			uint64_t used = 0;

			for (uint32_t i = 0; i < num; i++) {
				Gpt_entry const e(entries.base() + i * header.entry_size());

				if (!e.valid()) { continue; }

				uint64_t const v = (e.lba_end() - e.lba_start()) + 1;
				used += v;
			}

			return used;
		}


		/**
		 * Parse the GPT header
		 */
		void _parse_gpt(Gpt_hdr &gpt)
		{
			if (!(gpt.valid(data)))
				throw Exception();

			Sector entry_array(data, gpt.gpe_lba(),
			                   gpt.entries() * gpt.entry_size() / block.info().block_size);
			Gpt_entry entries(entry_array.addr<addr_t>());

			for (int i = 0; i < MAX_PARTITIONS; i++) {

				Gpt_entry e(entries.base() + i * gpt.entry_size());

				if (!e.valid())
					continue;

				uint64_t start  = e.lba_start();
				uint64_t length = e.lba_end() - e.lba_start() + 1; /* [...) */

				_part_list[i].construct(start, length);

				log("Partition ", i + 1, ": LBA ", start, " (", length,
				    " blocks) type: '", e.type(),
				    "' name: '", e, "'");
			}

			/* Report the partitions */
			if (reporter.enabled())
			{
				Reporter::Xml_generator xml(reporter, [&] () {
					xml.attribute("type", "gpt");

					uint64_t const total_blocks = block.info().block_count;
					xml.attribute("total_blocks", total_blocks);

					uint64_t const gpt_total =
						(gpt.part_lba_end() - gpt.part_lba_start()) + 1;
					xml.attribute("gpt_total", gpt_total);

					uint64_t const gpt_used =
						_calculate_used(gpt, entries, gpt.entries());
					xml.attribute("gpt_used", gpt_used);

					for (int i = 0; i < MAX_PARTITIONS; i++) {
						Gpt_entry e(entries.base() + i * gpt.entry_size());

						if (!e.valid()){
							continue;
						}

						enum { BYTES = 4096, };
						Sector fs(data, e.lba_start(), BYTES / block.info().block_size);

						Fs::Type fs_type = Fs::probe(fs.addr<uint8_t*>(), BYTES);

						String<40>                  guid { e.guid() };
						String<40>                  type { e.type() };
						String<Gpt_entry::NAME_LEN> name { e };

						xml.node("partition", [&] () {
							xml.attribute("number", i + 1);
							xml.attribute("name", name);
							xml.attribute("type", type);
							xml.attribute("guid", guid);;
							xml.attribute("start", e.lba_start());
							xml.attribute("length", e.lba_end() - e.lba_start() + 1);
							xml.attribute("block_size", block.info().block_size);

							uint64_t const gap = _calculate_gap(gpt, e, entries,
							                                            gpt.entries(),
							                                            total_blocks);
							if (gap) { xml.attribute("expandable", gap); }

							if (fs_type.valid()) {
								xml.attribute("file_system", fs_type);
							}
						});
					}
				});
			}
		}

	public:

		using Partition_table::Partition_table;

		Partition &partition(long num) override
		{
			num -= 1;

			if (num < 0 || num > MAX_PARTITIONS)
				throw -1;

			if (!_part_list[num].constructed())
				throw -1;

			return *_part_list[num];
		}

		bool parse() override
		{
			block.sigh(io_sigh);

			Sector s(data, Gpt_hdr::Hdr_lba::LBA, 1);
			Gpt_hdr hdr(s.addr<addr_t>());
			_parse_gpt(hdr);

			for (unsigned num = 0; num < MAX_PARTITIONS; num++)
				if (_part_list[num].constructed())
					return true;
			return false;
		}
};

#endif /* _PART_BLOCK__GUID_PARTITION_TABLE_H_ */
