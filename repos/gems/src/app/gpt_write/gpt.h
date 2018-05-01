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

#ifndef _GPT_H_
#define _GPT_H_

/* Genode includes */
#include <base/fixed_stdint.h>

/* local includes */
#include <util.h>

namespace Gpt {

	using namespace Genode;

	/***************
	 ** Datatypes **
	 ***************/

	using Type  = Genode::String<32>;
	using Label = Genode::String<32>;

	/**
	 * DCE uuid struct
	 */
	struct Uuid
	{
		enum { UUID_NODE_LEN = 6 };

		uint32_t time_low;
		uint16_t time_mid;
		uint16_t time_hi_and_version;
		uint8_t  clock_seq_hi_and_reserved;
		uint8_t  clock_seq_low;
		uint8_t  node[UUID_NODE_LEN];

		bool valid() const {
			return time_low != 0 && time_hi_and_version != 0;
		}
	} __attribute__((packed));


	/**
	 * GUID parition table header
	 */
	struct Header
	{
		char     signature[8];   /* identifies GUID Partition Table */
		uint32_t revision;       /* GPT specification revision */
		uint32_t size;           /* size of GPT header */
		uint32_t crc;            /* CRC32 of GPT header */
		uint32_t reserved;       /* must be zero */
		uint64_t lba;            /* LBA that contains this header */
		uint64_t backup_lba;     /* LBA of backup GPT header */
		uint64_t part_lba_start; /* first LBA usable for partitions */
		uint64_t part_lba_end;   /* last LBA usable for partitions */
		Uuid     guid;           /* GUID to identify the disk */
		uint64_t gpe_lba;        /* first LBA of GPE array */
		uint32_t gpe_num;        /* number of entries in GPE array */
		uint32_t gpe_size;       /* size of each GPE */
		uint32_t gpe_crc;        /* CRC32 of GPE array */

		/* the remainder of the struct must be zero */

		bool valid(bool primary = true)
		{
			/* check sig */
			if (strcmp(signature, "EFI PART", 8) != 0) {
				return false;
			}

			/* check header crc */
			uint32_t const save_crc = crc;
			crc = 0;
			if (Util::crc32(this, size) != save_crc) {
				error("wrong header checksum");
				return false;
			}

			crc = save_crc;

			/* check header lba */
			if ((primary ? lba : backup_lba) != 1) {
				return false;
			}

			return true;
		}

		bool entries_valid(void const *entries) const
		{
			size_t const length = gpe_num * gpe_size;
			return Util::crc32(entries, length) == gpe_crc ? true : false;
		}
	} __attribute__((packed));


	/**
	 * GUID partition entry format
	 */
	struct Entry
	{
		enum { NAME_LEN = 36 };
		Uuid     type;           /* partition type GUID */
		Uuid     guid;           /* unique partition GUID */
		uint64_t lba_start;      /* start of partition */
		uint64_t lba_end;        /* end of partition */
		uint64_t attributes;     /* partition attributes */
		uint16_t name[NAME_LEN]; /* partition name in UTF-16LE */

		bool valid() const { return type.valid(); }

		/**
		 * Extract all valid ASCII characters in the name entry
		 */
		bool read_name(char *dest, size_t dest_len) const
		{
			return !!Util::extract_ascii(dest, dest_len, name, NAME_LEN);
		}

		/**
		 * Write ASCII to name field
		 */
		bool write_name(Label const &label)
		{
			return !!Util::convert_ascii(name, NAME_LEN,
			                             (uint8_t*)label.string(),
			                             label.length() - 1);
		}

		uint64_t length() const { return lba_end - lba_start + 1; }
	} __attribute__((packed));


	/*****************
	 ** Definitions **
	 *****************/

	enum { REVISION = 0x00010000u, };

	enum {
		MIN_ENTRIES  = 128u,
		MAX_ENTRIES  = MIN_ENTRIES,
		ENTRIES_SIZE = sizeof(Entry) * MAX_ENTRIES,
		PGPT_LBA = 1,
	};


	/***************
	 ** Functions **
	 ***************/

	/**
	 * Convert type string to UUID
	 *
	 * \param type  type string
	 *
	 * \return  UUID in case the type is known, otherwise exception is thrown
	 *
	 * \throw Invalid_type
	 */
	Uuid type_to_uuid(Type const &type)
	{
		struct {
			char const *type;
			Uuid        uuid;
		} _gpt_types[] = {
			/* EFI System Partition */
			{ "EFI",   0xC12A7328, 0xF81F, 0x11D2, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B },
			/* BIOS Boot Partition (GRUB) */
			{ "BIOS",  0x21686148, 0x6449, 0x6E6F, 0x74, 0x4E, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49 },
			/* Basic Data Partition (FAT32, exFAT, NTFS, ...) */
			{ "BDP",   0xEBD0A0A2, 0xB9E5, 0x4433, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 },
			/* Linux Filesystem Data (for now used by Genode for Ext2 rootfs) */
			{ "Linux", 0x0FC63DAF, 0x8483, 0x4772, 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 },
		};

		size_t const num = sizeof(_gpt_types) / sizeof(_gpt_types[0]);
		for (size_t i = 0; i < num; i++) {
			if (type == _gpt_types[i].type) {
				return _gpt_types[i].uuid;
			}
		}

		struct Invalid_type : Genode::Exception { };
		throw Invalid_type();
	}


	/**
	 * Generate random UUID (RFC 4122 4.4)
	 *
	 * \return  random UUID
	 */
	Uuid generate_uuid()
	{
		uint8_t buf[sizeof(Uuid)];
		Util::get_random(buf, sizeof(buf));

		Uuid uuid;
		Genode::memcpy(&uuid, buf, sizeof(buf));

		uuid.time_hi_and_version =
			(uuid.time_hi_and_version & 0x0fff) | 0x4000;
		uuid.clock_seq_hi_and_reserved =
			(uuid.clock_seq_hi_and_reserved & 0x3f) | 0x80;

		return uuid;
	}


	/**
	 * Get block gap to next logical entry
	 *
	 * \param header   pointer to GPT header
	 * \param entry    pointer to current entry
	 * \param entries  pointer to entries
	 *
	 * \return the number of free blocks to the next logical entry
	 */
	Genode::uint64_t gap_length(Gpt::Header const &header,
	                            Gpt::Entry  const *entries,
	                            Gpt::Entry  const *entry)
	{
		using namespace Genode;

		/* add one block => end == start */
		uint64_t const end_lba = entry ? entry->lba_end + 1 : ~0ull;

		enum { INVALID_START = ~0ull, };
		uint64_t next_start_lba = INVALID_START;

		for (uint32_t i = 0; i < header.gpe_num; i++) {
			Entry const *e = (entries + i);

			if (!e->valid() || e == entry) { continue; }

			/*
			 * Check if the entry starts afterwards and save the
			 * entry with the smallest distance.
			 */
			if (e->lba_start >= end_lba) {
				next_start_lba = min(next_start_lba, e->lba_start);
			}
		}

		/*
		 * Use stored next start LBA or paritions end LBA from header,
		 * if there is no other entry or we are the only one.
		 */
		return (next_start_lba == INVALID_START ? header.part_lba_end
		                                        : next_start_lba) - end_lba;
	}


	/**
	 * Find free GPT entry
	 *
	 * \param header   reference to GPT header
	 * \param entries  pointer to memory containing GPT entries
	 *
	 * \return  if a free entry is found a pointer to its memory
	 *          is returned, otherwise a null pointer is returned
	 */
	Entry *find_free(Header const &header, Entry *entries)
	{
		if (!entries) { return nullptr; }

		Entry *result = nullptr;
		for (size_t i = 0; i < header.gpe_num; i++) {
			Entry *e = (entries + i);

			if (e->valid()) { continue; }

			result = e;
			break;
		}

		return result;
	}


	/**
	 * Get last valid entry
	 *
	 * \param header   reference to GPT header
	 * \param entries  pointer to memory containing GPT entries
	 *
	 *
	 * \return  if a free entry is found a pointer to its memory
	 *          is returned, otherwise a null pointer is returned
	 */
	Entry const *find_last_valid(Header const &header, Entry const *entries)
	{
		if (!entries) { return nullptr; }

		Entry const *result = nullptr;
		for (size_t i = 0; i < header.gpe_num; i++) {
			Entry const * const e = (entries + i);

			if (e->valid()) { result = e; }
		}

		return result;
	}


	/**
	 * Get next free entry
	 */
	Entry *find_next_free(Header const &header, Entry *entries,
	                      Entry  const *entry)
	{
		if (!entries || !entry) { return nullptr; }

		size_t const num = (entry - entries + 1);
		if (num >= header.gpe_num) { return nullptr; }

		Entry *result = nullptr;
		for (size_t i = num; i < header.gpe_num; i++) {
			Entry * const e = (entries + i);

			if (e->valid()) { continue; }

			result = e;
			break;
		}

		return result;
	}


	/**
	 * Lookup GPT entry
	 *
	 * \param entries  pointer to memory containing GPT entries
	 * \param num      number of GPT entries
	 * \param label    name of the partition in the entry
	 *
	 * \return  if entry is found a pointer to its memory location
	 *          is returned, otherwise a null pointer is returned
	 */
	Entry *lookup_entry(Entry *entries, size_t num, Label const &label)
	{
		if (!entries) { return nullptr; }

		Entry *result = nullptr;

		char tmp[48];

		for (size_t i = 0; i < num; i++) {
			Entry *e = (entries + i);

			if (!e->valid()) { continue; }

			if (!e->read_name(tmp, sizeof(tmp))) { continue; }

			if (Genode::strcmp(label.string(), tmp) == 0) {
				result = e;
				break;
			}
		}

		return result;
	}


	/**
	 * Get number of entry
	 *
	 * \param entries  pointer to memory containing GPT entries
	 * \param entry    pointer to memory containing GPT entry
	 *
	 * \return  number of entry
	 */
	uint32_t entry_num(Entry const *entries, Entry const *e)
	{
		return e - entries + 1;
	}


	/**
	 * Get used blocks
	 *
	 * \param header   reference to GPT header
	 * \param entries  pointer to memory containing GPT entries
	 *
	 * \return  if a free entry is found a pointer to its memory
	 *          is returned, otherwise a null pointer if returned
	 */
	uint64_t get_used_blocks(Header const &header, Entry const *entries)
	{
		if (!entries) { return ~0ull; }

		uint64_t result = 0;

		for (size_t i = 0; i < header.gpe_num; i++) {
			Entry const * const e = (entries + i);

			if (!e->valid()) { continue; }

			uint64_t const blocks = e->lba_end - e->lba_start;
			result += blocks;
		}

		return result;
	}


	/**
	 * Check if given GPT header and entries are valid
	 *
	 * \param header   reference to header
	 * \param entries  pointer to memory containing GPT entries
	 * \param primary  if true, header is assumed to be the primary
	 *                 and otherwise to be the backup header
	 *
	 * \return true if header and entries are valid, otherwise false
	 */
	bool valid(Header &header, Entry const *e, bool primary)
	{
		return    header.valid(primary)
		       && header.entries_valid(e) ? true : false;
	}

	/**
	 * Update CRC32 fields
	 *
	 * \param header   reference to header
	 * \param entries  pointer to memory containing GPT entries
	 */
	void update_crc32(Header &header, void const *entries)
	{
		size_t const len = header.gpe_size * header.gpe_num;
		header.gpe_crc = Util::crc32(entries, len);
		header.crc = 0;
		header.crc = Util::crc32(&header, header.size);
	}

} /* namespace Gpt */

#endif /* _GPT_H_ */
