/*
 * \brief  ARM specific fault info
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
	Genode::addr_t ip         = 0;
	Genode::addr_t pf         = 0;
	bool           data_abort = 0;
	bool           write      = 0;

	Fault_info(seL4_MessageInfo_t msg_info)
	:
		ip(seL4_GetMR(0)),
		pf(seL4_GetMR(1)),
		data_abort(seL4_GetMR(2) != 1),
		/* Instruction Fault Status Register (IFSR) resp. Data FSR (DFSR) */
		write(data_abort && (seL4_GetMR(3) & (1 << 11)))
	{ }
};
