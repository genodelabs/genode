/*
 * \brief  Interfaces for initializing libc subsystems
 * \author Norman Feske
 * \date   2016-10-27
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIBC_INIT_H_
#define _LIBC_INIT_H_

/* Genode includes */
#include <base/env.h>

namespace Libc {

	/**
	 * Support for shared libraries
	 */
	void init_dl(Genode::Env &env);
}

#endif /* _LIBC_INIT_H_ */
