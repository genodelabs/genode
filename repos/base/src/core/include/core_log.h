/*
 * \brief  Kernel backend for core log messages
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2016-10-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE_LOG_H_
#define _CORE_LOG_H_

#include <util/string.h>

namespace Genode {
	struct Core_log;

	struct Core_log_range {
		addr_t start;
		addr_t size;
	};

	void init_core_log(Core_log_range const &);
}


struct Genode::Core_log
{
	void out(char const c);

	void output(char const * str);
};

#endif /* _CORE_LOG_H_ */
