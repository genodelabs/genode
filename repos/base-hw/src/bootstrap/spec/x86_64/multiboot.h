/*
 * \brief  Multiboot handling
 * \author Christian Helmuth
 * \author Alexander Boettcher
 * \date   2006-05-09
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__BOOTSTRAP__SPEC__X86_64__MULTIBOOT_H_
#define _SRC__BOOTSTRAP__SPEC__X86_64__MULTIBOOT_H_

/* base includes */
#include <util/mmio.h>

namespace Genode { class Multiboot_info; }


class Genode::Multiboot_info : Mmio<0x34>
{
	private:

		struct Flags : Register<0x00, 32>
		{
			struct Mem_map : Bitfield<6, 1> { };
		};

		struct Mmap_length: Register<0x2c, 32> { };
		struct Mmap_addr  : Register<0x30, 32> { };

	public:

		enum {
			MAGIC = 0x2badb002,
		};

		Multiboot_info(addr_t mbi) : Mmio({(char *)mbi, Mmio::SIZE}) { }
		Multiboot_info(addr_t mbi, bool strip);

		struct Mmap : Genode::Mmio<0x1c>
		{
			struct Size   : Register <0x00, 32> { };
			struct Addr   : Register <0x04, 64> { };
			struct Length : Register <0x0c, 64> { };
			struct Type   : Register <0x14,  8> { enum { MEMORY = 1 }; };

			Mmap(addr_t mmap = 0) : Mmio({(char *)mmap, Mmio::SIZE}) { }
		};

		/**
		 * Physical ram regions
		 */
		addr_t phys_ram_mmap_base(unsigned i, bool solely_within_4k_base = true)
		{
			if (!read<Flags::Mem_map>())
				return 0;

			Mmap_length::access_t const mmap_length = read<Mmap_length>();
			Mmap_addr::access_t const mmap_start  = read<Mmap_addr>();

			for (Genode::uint32_t j = 0, mmap = mmap_start;
			     mmap < mmap_start + mmap_length;) {

				enum { MMAP_SIZE_SIZE_OF = 4, MMAP_SIZE_OF = 4 + 8 + 1 };

				if (solely_within_4k_base &&
				    (mmap + MMAP_SIZE_OF >= Genode::align_addr(base() + 1, 12)))
					return 0;

				Mmap r(mmap);
				addr_t last_mmap = mmap;
				mmap += r.read<Mmap::Size>() + MMAP_SIZE_SIZE_OF;

				if (r.read<Mmap::Type>() != Mmap::Type::MEMORY)
					continue;

				if (i == j)
					return last_mmap;

				j++;
			}

			return 0;
		}
};

#endif /* _SRC__BOOTSTRAP__SPEC__X86_64__MULTIBOOT_H_ */
