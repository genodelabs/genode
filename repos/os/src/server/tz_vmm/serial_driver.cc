/*
 * \brief  Paravirtualized access to serial device for a Trustzone VM
 * \author Martin Stein
 * \date   2015-10-23
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* local includes */
#include <serial_driver.h>

using namespace Genode;


void Serial_driver::_flush()
{
	_push(0);
	log("[vm] ", _buf.local_addr<char const>());
	_off = 0;
}


void Serial_driver::_send(Vm_base &vm)
{
	char const c = vm.smc_arg_2();
	if (c == '\n') {
		_flush();
	} else {
		_push(c); }

	if (_off == BUF_SIZE - 1) {
		_flush(); }
}


void Serial_driver::handle_smc(Vm_base &vm)
{
	enum { SEND = 0 };
	switch (vm.smc_arg_1()) {
	case SEND: _send(vm); break;
	default: error("unknown serial-driver function ", vm.smc_arg_1()); }
}
