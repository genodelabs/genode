/*
 * \brief  GUID Partition table definitions
 * \author Josef Soentgen
 * \date   2014-09-19
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLK__GPT_H_
#define _PART_BLK__GPT_H_

#include <base/env.h>
#include <base/log.h>
#include <block_session/client.h>
#include <util/misc_math.h>

#include "driver.h"
#include "partition_table.h"
#include "fsprobe.h"

namespace {

static bool const verbose = false;

/* simple bitwise CRC32 checking */
static inline Genode::uint32_t crc32(void const * const buf, Genode::size_t size)
{
	Genode::uint8_t const *p = static_cast<Genode::uint8_t const*>(buf);
	Genode::uint32_t crc = ~0U;

	while (size--) {
		crc ^= *p++;
		for (Genode::uint32_t j = 0; j < 8; j++)
			crc = (-Genode::int32_t(crc & 1) & 0xedb88320) ^ (crc >> 1);
	}

	return crc ^ ~0U;
}

} /* anonymous namespace */


class Gpt : public Block::Partition_table
{
	private:

		enum { MAX_PARTITIONS = 128 };

		/* contains pointers to valid partitions or 0 */
		Block::Partition *_part_list[MAX_PARTITIONS] { 0 };

		typedef Block::Partition_table::Sector Sector;


		/**
		 * DCE uuid struct
		 */
		struct Uuid
		{
			enum { UUID_NODE_LEN = 6 };

			Genode::uint32_t time_low;
			Genode::uint16_t time_mid;
			Genode::uint16_t time_hi_and_version;
			Genode::uint8_t  clock_seq_hi_and_reserved;
			Genode::uint8_t  clock_seq_low;
			Genode::uint8_t  node[UUID_NODE_LEN];

			char const *to_string()
			{
				static char buffer[37 + 1];

				Genode::snprintf(buffer, sizeof(buffer),
				                 "%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
				                 time_low, time_mid, time_hi_and_version,
				                 clock_seq_hi_and_reserved, clock_seq_low,
				                 node[0], node[1], node[2], node[3], node[4], node[5]);

				return buffer;
			}

		} __attribute__((packed));


		/**
		 * GUID parition table header
		 */
		struct Gpt_hdr
		{
			enum { HEADER_LBA = 1 };

			char             _sig[8];         /* identifies GUID Partition Table */
			Genode::uint32_t _revision;       /* GPT specification revision */
			Genode::uint32_t _hdr_size;       /* size of GPT header */
			Genode::uint32_t _hdr_crc;        /* CRC32 of GPT header */
			Genode::uint32_t _reserved;       /* must be zero */
			Genode::uint64_t _hdr_lba;        /* LBA that contains this header */
			Genode::uint64_t _backup_hdr_lba; /* LBA of backup GPT header */
			Genode::uint64_t _part_lba_start; /* first LBA usable for partitions */
			Genode::uint64_t _part_lba_end;   /* last LBA usable for partitions */
			Uuid             _guid;           /* GUID to identify the disk */
			Genode::uint64_t _gpe_lba;        /* first LBA of GPE array */
			Genode::uint32_t _entries;        /* number of entries in GPE array */
			Genode::uint32_t _entry_size;     /* size of each GPE */
			Genode::uint32_t _gpe_crc;        /* CRC32 of GPE array */

			void dump_hdr(bool check_primary)
			{
				if (!verbose) return;

				using namespace Genode;

				log("GPT ", check_primary ? "primary" : "backup", " header:");
				log(" rev: ", (unsigned) _revision);
				log(" size: ", (unsigned) _hdr_size);
				log(" crc: ", Hex(_hdr_crc, Hex::OMIT_PREFIX));
				log(" reserved: ", (unsigned) _reserved);
				log(" hdr lba: ", (unsigned long long) _hdr_lba);
				log(" bak lba: ", (unsigned long long) _backup_hdr_lba);
				log(" part start lba: ", (unsigned long long) _part_lba_start);
				log(" part end lba: ", (unsigned long long) _part_lba_end);
				log(" guid: ", _guid.to_string());
				log(" gpe lba: ", (unsigned long long) _gpe_lba);
				log(" entries: ", (unsigned) _entries);
				log(" entry size: ", (unsigned) _entry_size);
				log(" gpe crc: ", Hex(_gpe_crc, Hex::OMIT_PREFIX));
			}

			bool valid(Block::Driver &driver, bool check_primary = true)
			{
				dump_hdr(check_primary);

				/* check sig */
				if (Genode::strcmp(_sig, "EFI PART", 8) != 0)
					return false;

				/* check header crc */
				Genode::uint32_t crc = _hdr_crc;
				_hdr_crc = 0;
				if (crc32(this, _hdr_size) != crc) {
					Genode::error("Wrong GPT header checksum");
					return false;
				}

				/* check header lba */
				if (check_primary)
					if (_hdr_lba != HEADER_LBA)
						return false;

				/* check GPT entry array */
				Genode::size_t length = _entries * _entry_size;
				Sector gpe(driver, _gpe_lba, length / driver.blk_size());
				if (crc32(gpe.addr<void *>(), length) != _gpe_crc)
					return false;

				if (check_primary) {
					/* check backup gpt header */
					Sector backup_hdr(driver, _backup_hdr_lba, 1);
					if (!backup_hdr.addr<Gpt_hdr*>()->valid(driver, false)) {
						Genode::warning("Backup GPT header is corrupted");
					}
				}

				return true;
			}

			/* the remainder of the LBA must be zero */
		} __attribute__((packed));


		/**
		 * GUID partition entry format
		 */
		struct Gpt_entry
		{
			enum { NAME_LEN = 36 };
			Uuid             _type;           /* partition type GUID */
			Uuid             _guid;           /* unique partition GUID */
			Genode::uint64_t _lba_start;      /* start of partition */
			Genode::uint64_t _lba_end;        /* end of partition */
			Genode::uint64_t _attr;           /* partition attributes */
			Genode::uint16_t _name[NAME_LEN]; /* partition name in UNICODE-16 */

			bool valid() const
			{
				if (_type.time_low == 0x00000000)
					return false;

				return true;
			}

			/**
			 * Extract all valid ASCII characters in the name entry
			 */
			char const *name()
			{
				static char buffer[NAME_LEN + 1];

				char *p = buffer;
				Genode::size_t i = 0;

				for (Genode::size_t u = 0; u < NAME_LEN && _name[u] != 0; u++) {
					Genode::uint32_t utfchar = _name[i++];

					if ((utfchar & 0xf800) == 0xd800) {
						unsigned int c = _name[i];
						if ((utfchar & 0x400) != 0 || (c & 0xfc00) != 0xdc00)
							utfchar = 0xfffd;
						else
							i++;
					}

					*p++ = (utfchar < 0x80) ? utfchar : '.';
				}

				*p++ = 0;

				return buffer;
			}

		} __attribute__((packed));


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
		Genode::uint64_t _calculate_gap(Gpt_hdr   const *header,
		                                Gpt_entry const *entry,
		                                Gpt_entry const *entries,
		                                Genode::uint32_t num,
		                                Genode::uint64_t total_blocks)
		{
			using namespace Genode;

			/* add one block => end == start */
			uint64_t const end_lba = entry->_lba_end + 1;

			enum { INVALID_START = ~0ull, };
			uint64_t next_start_lba = INVALID_START;

			for (uint32_t i = 0; i < num; i++) {
				Gpt_entry const *e = (entries + i);

				if (!e->valid() || e == entry) { continue; }

				/*
				 * Check if the entry starts afterwards and save the
				 * entry with the smallest distance.
				 */
				if (e->_lba_start >= end_lba) {
					next_start_lba = min(next_start_lba, e->_lba_start);
				}
			}

			/* sanity check if GPT is broken */
			if (end_lba > header->_part_lba_end) { return 0; }

			/* if the underyling Block device changes we might be able to expand more */
			Genode::uint64_t const part_end = max(header->_part_lba_end, total_blocks);

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
		Genode::uint64_t _calculate_used(Gpt_hdr const *,
		                                 Gpt_entry const *entries,
		                                 Genode::uint32_t num)
		{
			using namespace Genode;

			uint64_t used = 0;

			for (uint32_t i = 0; i < num; i++) {
				Gpt_entry const *e = (entries + i);

				if (!e->valid()) { continue; }

				uint64_t const v = (e->_lba_end - e->_lba_start) + 1;
				used += v;
			}

			return used;
		}


		/**
		 * Parse the GPT header
		 */
		void _parse_gpt(Gpt_hdr *gpt)
		{
			if (!(gpt->valid(driver)))
				throw Genode::Exception();

			Sector entry_array(driver, gpt->_gpe_lba,
			                   gpt->_entries * gpt->_entry_size / driver.blk_size());
			Gpt_entry *entries = entry_array.addr<Gpt_entry *>();

			for (int i = 0; i < MAX_PARTITIONS; i++) {
				Gpt_entry *e = (entries + i);

				if (!e->valid())
					continue;

				Genode::uint64_t start  = e->_lba_start;
				Genode::uint64_t length = e->_lba_end - e->_lba_start + 1; /* [...) */

				_part_list[i] = new (&heap) Block::Partition(start, length);

				Genode::log("Partition ", i + 1, ": LBA ", start, " (", length,
				            " blocks) type: '", e->_type.to_string(),
				            "' name: '", e->name(), "'");
			}

			/* Report the partitions */
			if (reporter.enabled())
			{
				Genode::Reporter::Xml_generator xml(reporter, [&] () {
					xml.attribute("type", "gpt");

					Genode::uint64_t const total_blocks = driver.blk_cnt();
					xml.attribute("total_blocks", total_blocks);

					Genode::uint64_t const gpt_total =
						(gpt->_part_lba_end - gpt->_part_lba_start) + 1;
					xml.attribute("gpt_total", gpt_total);

					Genode::uint64_t const gpt_used =
						_calculate_used(gpt, entries, gpt->_entries);
					xml.attribute("gpt_used", gpt_used);

					for (int i = 0; i < MAX_PARTITIONS; i++) {
						Gpt_entry *e = (entries + i);

						if (!e->valid()){
							continue;
						}

						enum { BYTES = 4096, };
						Sector fs(driver, e->_lba_start, BYTES / driver.blk_size());
						Fs::Type fs_type = Fs::probe(fs.addr<Genode::uint8_t*>(), BYTES);

						xml.node("partition", [&] () {
							xml.attribute("number", i + 1);
							xml.attribute("name", e->name());
							xml.attribute("type", e->_type.to_string());
							xml.attribute("guid", e->_guid.to_string());
							xml.attribute("start", e->_lba_start);
							xml.attribute("length", e->_lba_end - e->_lba_start + 1);
							xml.attribute("block_size", driver.blk_size());

							Genode::uint64_t const gap = _calculate_gap(gpt, e, entries,
							                                            gpt->_entries,
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

		Block::Partition *partition(int num) {
			return (num <= MAX_PARTITIONS && num > 0) ? _part_list[num-1] : 0; }

		bool parse()
		{
			Sector s(driver, Gpt_hdr::HEADER_LBA, 1);
			_parse_gpt(s.addr<Gpt_hdr *>());

			for (unsigned num = 0; num < MAX_PARTITIONS; num++)
				if (_part_list[num])
					return true;
			return false;
		}
};

#endif /* _PART_BLK__GUID_PARTITION_TABLE_H_ */
