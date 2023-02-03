/*
 * \brief  x86 specific fault info
 * \author Alexander Boettcher
 * \date   2017-07-11
 */

/*
 * Copyright (C) 2017-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

struct Fault_info
{
	Genode::addr_t const ip;
	Genode::addr_t const pf;
	bool           const write;

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

	Genode::addr_t _ip_from_message(seL4_MessageInfo_t &info) const
	{
		auto const fault_type = seL4_MessageInfo_get_label(info);

		if (fault_type == seL4_Fault_UserException)
			return seL4_Fault_UserException_get_FaultIP(seL4_getFault(info));
		else
			return seL4_GetMR(0);
	}

	Genode::addr_t _pf_from_message(seL4_MessageInfo_t &info) const
	{
		auto const fault_type = seL4_MessageInfo_get_label(info);

		if (fault_type == seL4_Fault_UserException)
			return seL4_Fault_UserException_get_Number(seL4_getFault(info));
		else
			return seL4_GetMR(1);
	}

	Fault_info(seL4_MessageInfo_t info)
	:
		ip(_ip_from_message(info)),
		pf(_pf_from_message(info)),
		write(seL4_GetMR(3) & ERR_W)
	{ }

	bool exec_fault() const { return false; }
	bool align_fault() const { return false; }
};
