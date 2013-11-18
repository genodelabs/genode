/*
 * \brief  Virtual Machine Monitor VM state definition
 * \author Stefan Kalkowski
 * \date   2012-06-25
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _SRC__SERVER__VMM__INCLUDE__VM_STATE_H_
#define _SRC__SERVER__VMM__INCLUDE__VM_STATE_H_

/* Genode includes */
#include <cpu/cpu_state.h>

struct Vm_state : Genode::Cpu_state_modes
{
	Genode::addr_t dfar;    /* data fault address             */
	Genode::addr_t ttbr[2]; /* translation table base regs    */
	Genode::addr_t ttbrc;   /* translation table base control */
};

#endif /* _SRC__SERVER__VMM__INCLUDE__VM_STATE_H_ */
