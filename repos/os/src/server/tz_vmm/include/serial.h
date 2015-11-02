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

#ifndef _TZ_VMM__INCLUDE__SERIAL_H_
#define _TZ_VMM__INCLUDE__SERIAL_H_

/* Genode includes */
#include <vm_base.h>
#include <os/attached_ram_dataspace.h>

namespace Vmm { class Serial; }

/**
 * Paravirtualized access to serial devices for a Trustzone VM
 */
class Vmm::Serial : private Genode::Attached_ram_dataspace
{
	private:

		enum {
			BUF_SIZE = 4096,
			WRAP     = BUF_SIZE - sizeof(char),
		};

		Genode::addr_t _off;

		void _push(char const c);

		void _flush();

		void _send(Vm_base * const vm);

	public:

		/**
		 * Handle Secure Monitor Call of VM 'vm' on VMM serial
		 */
		void handle(Vm_base * const vm);

		Serial();
};

#endif /* _TZ_VMM__INCLUDE__SERIAL_H_ */
