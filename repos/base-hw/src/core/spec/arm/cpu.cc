/*
 * \brief  CPU driver for core
 * \author Martin stein
 * \date   2014-07-14
 */

/*
 * Copyright (C) 2011-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <cpu.h>

using namespace Genode;

unsigned Cpu::primary_id() { return 0; }


unsigned Cpu::executing_id() { return primary_id(); }


addr_t Cpu::Context::translation_table() const {
	return Ttbr0::Ba::masked(ttbr0); }


void Cpu::Context::translation_table(addr_t const t) {
	ttbr0 = Arm::Ttbr0::init(t); }


bool Cpu::User_context::in_fault(addr_t & va, addr_t & w) const
{
	switch (cpu_exception) {

	case PREFETCH_ABORT:
		{
			/* check if fault was caused by a translation miss */
			Ifsr::access_t const fs = Ifsr::Fs::get(Ifsr::read());
			if (fs != Ifsr::section && fs != Ifsr::page)
				return false;

			/* fetch fault data */
			w = 0;
			va = ip;
			return true;
		}
	case DATA_ABORT:
		{
			/* check if fault was caused by translation miss */
			Dfsr::access_t const fs = Dfsr::Fs::get(Dfsr::read());
			if (fs != Dfsr::section && fs != Dfsr::page)
				return false;

			/* fetch fault data */
			Dfsr::access_t const dfsr = Dfsr::read();
			w = Dfsr::Wnr::get(dfsr);
			va = Dfar::read();
			return true;
		}

	default:
		return false;
	};
}
