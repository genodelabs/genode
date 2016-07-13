/*
 * \brief  Paravirtualized access to serial devices for a Trustzone VM
 * \author Martin Stein
 * \date   2015-10-23
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* local includes */
#include <serial.h>

using namespace Genode;
using namespace Vmm;


void Serial::_push(char const c)
{
	local_addr<char>()[_off] = c;
	_off += sizeof(char);
}


void Serial::_flush()
{
	_push(0);
	log("[vm] ", local_addr<char const>());
	_off = 0;
}


void Serial::_send(Vm_base * const vm)
{
	char const c = vm->smc_arg_2();
	if (c == '\n') { _flush(); }
	else { _push(c); }
	if (_off == WRAP) { _flush(); }
}


void Serial::handle(Vm_base * const vm)
{
	enum { SEND = 0 };
	switch (vm->smc_arg_1()) {
	case SEND: _send(vm); break;
	default:
		Genode::error("Unknown function ", vm->smc_arg_1(), " requested on VMM serial");
		break;
	}
}


Serial::Serial()
:
	Attached_ram_dataspace(env()->ram_session(), BUF_SIZE), _off(0)
{ }
