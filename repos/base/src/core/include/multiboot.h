/**
 * \brief  GRUB multi-boot information handling
 * \author Christian Helmuth
 * \date   2006-05-09
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__MULTIBOOT_H_
#define _CORE__INCLUDE__MULTIBOOT_H_

#include <rom_fs.h>

#include <base/stdint.h>

namespace Genode {

	class Multiboot_info
	{
		private:

			/* Location of MBI in memory */
			void *_mb_info;

		public:

			/** Standard constructor creates invalid object */
			Multiboot_info() : _mb_info(0) { }

			Multiboot_info(void *mb_info);

			/**
			 * Number of boot modules
			 */
			unsigned num_modules();

			/**
			 * Use boot module num
			 *
			 * The module is marked as invalid in MBI and cannot be gotten again
			 */
			Rom_module get_module(unsigned num);

			/**
			 * Read module info
			 */
			bool check_module(unsigned num, addr_t *start, addr_t *end);

			/**
			 * Debugging (may be removed later)
			 */
			void print_debug();

			/**
			 * Check validity
			 */
			bool valid() { return _mb_info ? true : false; }

			/* Accessors */
			size_t size() const { return 0x1000; }
	};
}

#endif /* _CORE__INCLUDE__MULTIBOOT_H_ */
