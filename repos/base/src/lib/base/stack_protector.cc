/*
 * \brief  Stack protector support
 * \author Emery Hemingway
 * \date   2018-11-30
 *
 * The following is necessary but not sufficient for stack protection,
 * the __stack_chk_guard is initialized to zero and must be reinitialized
 * with a nonce to protect against malicious behavior.
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <base/sleep.h>

extern "C" {

	Genode::uint64_t __stack_chk_guard;

	__attribute__((noreturn)) __attribute__((weak))
	void __stack_chk_fail(void)
	{
		Genode::error("stack protector check failed");
		Genode::sleep_forever();
	}

}
