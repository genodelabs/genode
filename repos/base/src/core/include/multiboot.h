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
			struct Type   : Register <0x14,  8> {
				struct Memory   : Bitfield<0, 1> { };
				struct Reserved : Bitfield<1, 1> { };
			};

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
};

#endif /* _CORE__INCLUDE__MULTIBOOT_H_ */
