/*
 * \brief  Virtual Machine implementation
 * \author Stefan Kalkowski
 * \author Martin Stein
 * \date   2015-02-27
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_rom_dataspace.h>

/* local includes */
#include <vm.h>

using namespace Genode;


void Vm::_load_kernel_surroundings()
{
	/* load initrd */
	enum { INITRD_OFFSET = 0x1000000 };
	Attached_rom_dataspace initrd(_env, "initrd.gz");
	memcpy((void*)(_ram.local() + INITRD_OFFSET),
	       initrd.local_addr<void>(), initrd.size());

	/* load ATAGs */
	Atag tag((void*)(_ram.local() + ATAG_OFFSET));
	tag.setup_mem_tag(_ram.base(), _ram.size());
	tag.setup_cmdline_tag(_cmdline.string());
	tag.setup_initrd2_tag(_ram.base() + INITRD_OFFSET, initrd.size());
	if (_board.value) {
		tag.setup_rev_tag(_board.value); }
	tag.setup_end_tag();
}
