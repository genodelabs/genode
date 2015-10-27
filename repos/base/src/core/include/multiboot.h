/*
 * \brief  Multiboot handling
 * \author Christian Helmuth
 * \author Alexander Boettcher
 * \date   2006-05-09
 */

/*
 * Copyright (C) 2006-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__MULTIBOOT_H_
#define _CORE__INCLUDE__MULTIBOOT_H_

/* base includes */
#include <util/mmio.h>

/* core includes */
#include <rom_fs.h>

namespace Genode { class Multiboot_info; }

class Genode::Multiboot_info : Mmio
{
	private:

		struct Flags      : Register<0x00, 32> {
			struct Mem_map : Bitfield<6, 1> { };
		};
		struct Mods_count : Register<0x14, 32> { };
		struct Mods_addr  : Register<0x18, 32> { };

		struct Mmap_length: Register<0x2c, 32> { };
		struct Mmap_addr  : Register<0x30, 32> { };

	public:

		Multiboot_info(addr_t mbi) : Mmio(mbi) { }
		Multiboot_info(addr_t mbi, bool strip);

		struct Mmap : Genode::Mmio {
			struct Size   : Register <0x00, 32> { };
			struct Addr   : Register <0x04, 64> { };
			struct Length : Register <0x0c, 64> { };
			struct Type   : Register <0x14,  8> { enum { MEMORY = 1 }; };

			Mmap(addr_t mmap = 0) : Mmio(mmap) { }
		};

		struct Mods : Genode::Mmio {
			struct Start   : Register <0x00, 32> { };
			struct End     : Register <0x04, 32> { };
			struct Cmdline : Register <0x08, 32> { };
			struct Padding : Register <0x0c, 32> { };

			Mods(addr_t mods) : Mmio(mods) { }
		};

	private:

		Mods _get_mod(unsigned i) {
			Genode::addr_t mods_addr = read<Mods_addr>();

			enum { MODS_SIZE_OF = 16 };
			return Mods(mods_addr + i * MODS_SIZE_OF);
		}

	public:

		/**
		 * Number of boot modules
		 */
		unsigned num_modules();

		/* Accessors */
		size_t size() const { return 0x1000; }

		/**
		 * Use boot module num
		 *
		 * The module is marked as invalid in MBI and cannot be gotten again
		 */
		Rom_module get_module(unsigned num);

		/**
		 * Physical ram regions
		 */
		Mmap phys_ram(unsigned i, bool solely_within_4k_base = true) {

			if (!read<Flags::Mem_map>())
				return Mmap(0);

			Mmap_length::access_t const mmap_length = read<Mmap_length>();
			Mmap_addr::access_t const mmap_start  = read<Mmap_addr>();

			for (Genode::uint32_t j = 0, mmap = mmap_start;
			     mmap < mmap_start + mmap_length;) {

				enum { MMAP_SIZE_SIZE_OF = 4, MMAP_SIZE_OF = 4 + 8 + 1 };

				if (solely_within_4k_base &&
				    (mmap + MMAP_SIZE_OF >= Genode::align_addr(base + 1, 12)))
					return Mmap(0);

				Mmap r(mmap);
				mmap += r.read<Mmap::Size>() + MMAP_SIZE_SIZE_OF;

				if (r.read<Mmap::Type>() != Mmap::Type::MEMORY)
					continue;

				if (i == j)
					return r;

				j++;
			}

			return Mmap(0);
		}
};

#endif /* _CORE__INCLUDE__MULTIBOOT_H_ */
