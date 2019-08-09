/*
 * \brief  Poor man's partition probe for known file system
 * \author Josef Soentgen
 * \date   2018-05-03
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _PART_BLOCK__FSPROBE_H_
#define _PART_BLOCK__FSPROBE_H_

/* Genode includes */
#include <base/env.h>
#include <base/log.h>

namespace Fs {

	using namespace Genode;

	using Type = Genode::String<32>;

	Type probe(uint8_t *buffer, size_t len);

	/**
	 * Probe for Ext2/3/4
	 */
	Type _probe_extfs(uint8_t *p, size_t len)
	{
		if (len < 4096) { return Type(); }

		/* super block starts a byte offset 1024 */
		p += 0x400;

		bool const found_ext_sig = p[0x38] == 0x53 && p[0x39] == 0xEF;

		enum {
			COMPAT_HAS_JOURNAL      = 0x004u,
			INCOMPAT_EXTENTS        = 0x040u,
			RO_COMPAT_METADATA_CSUM = 0x400u,
		};

		uint32_t const compat    = *(uint32_t*)(p + 0x5C);
		uint32_t const incompat  = *(uint32_t*)(p + 0x60);
		uint32_t const ro_compat = *(uint32_t*)(p + 0x64);

		/* the feature flags should denote a given Ext version */

		bool const ext3 = compat & COMPAT_HAS_JOURNAL;
		bool const ext4 = ext3 && ((incompat & INCOMPAT_EXTENTS)
		                       &&  (ro_compat & RO_COMPAT_METADATA_CSUM));

		if      (found_ext_sig && ext4) { return Type("Ext4"); }
		else if (found_ext_sig && ext3) { return Type("Ext3"); }
		else if (found_ext_sig)         { return Type("Ext2"); }

		return Type();
	}

	/**
	 * Probe for FAT16/32
	 */
	Type _probe_fatfs(uint8_t *p, size_t len)
	{
		if (len < 512) { return Type(); }

		/* at least the checks ring true when mkfs.vfat is used... */
		bool const found_boot_sig = p[510] == 0x55 && p[511] == 0xAA;

		bool const fat16  =     p[38] == 0x28 || p[38] == 0x29;
		bool const fat32  =    (p[66] == 0x28 || p[66] == 0x29)
		                    && (p[82] == 'F' && p[83] == 'A');
		bool const gemdos =    (p[0] == 0xe9);

		if (found_boot_sig && fat32) { return Type("FAT32");  }
		if (found_boot_sig && fat16) { return Type("FAT16");  }
		if (gemdos)                  { return Type("GEMDOS"); }

		return Type();
	}
}


Fs::Type Fs::probe(uint8_t *p, size_t len)
{
	/* Ext2/3/4 */
	{
		Type t = _probe_extfs(p, len);
		if (t.valid()) { return t; }
	}

	/* FAT16/32 */
	{
		Type t = _probe_fatfs(p, len);
		if (t.valid()) { return t; }
	}

	return Type();
}

#endif /* _PART_BLOCK__FSPROBE_H_ */
