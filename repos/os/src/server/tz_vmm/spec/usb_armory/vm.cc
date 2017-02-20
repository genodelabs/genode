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


void Vm::on_vmm_entry()
{
	_led.direction(Gpio::Session::OUT);
	_led.write(false);
}


void Vm::_load_kernel_surroundings()
{
	Attached_rom_dataspace dtb(_env, "dtb");
	memcpy((void*)(_ram.local() + DTB_OFFSET), dtb.local_addr<void>(),
	       dtb.size());
}
