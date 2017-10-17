/*
 * \brief  x86 specific fault info
 * \author Alexander Boettcher
 * \date   2017-07-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

struct Fault_info
{
	Genode::addr_t ip    = 0;
	Genode::addr_t pf    = 0;
	bool           write = 0;

	/*
	 * Intel manual: 6.15 EXCEPTION AND INTERRUPT REFERENCE
	 *                    Interrupt 14—Page-Fault Exception (#PF)
	 */
	enum {
		ERR_I = 1 << 4,
		ERR_R = 1 << 3,
		ERR_U = 1 << 2,
		ERR_W = 1 << 1,
		ERR_P = 1 << 0,
	};

	Fault_info(seL4_MessageInfo_t msg_info)
	:
		ip(seL4_GetMR(0)),
		pf(seL4_GetMR(1)),
		write(seL4_GetMR(3) & ERR_W)
	{ }

	bool exec_fault() const { return false; }
};
