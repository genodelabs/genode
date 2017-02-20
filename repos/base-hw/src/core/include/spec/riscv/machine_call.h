/**
 * \brief  Calls supported by machine mode (or SBI interface in RISC-V)
 * \author Sebastian Sumpf
 * \author Martin Stein
 * \date   2015-06-14
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MACHINE_CALL_H_
#define _MACHINE_CALL_H_

/* base-hw includes */
#include <kernel/interface.h>

namespace Machine {

	using namespace Kernel;

	/**
	 * SBI calls to machine mode.
	 *
	 * Keep in sync with mode_transition.s.
	 */
	constexpr Call_arg call_id_put_char()      { return 0x100; }
	constexpr Call_arg call_id_set_sys_timer() { return 0x101; }
	constexpr Call_arg call_id_is_user_mode()  { return 0x102; }

	inline void put_char(Genode::uint64_t const c) {
		call(call_id_put_char(), (Call_arg)c); }

	inline void set_sys_timer(addr_t const t) {
		call(call_id_set_sys_timer(), (Call_arg)t); }

	inline bool is_user_mode() { return call(call_id_is_user_mode()); }
}

#endif /* _MACHINE_CALL_H_ */
