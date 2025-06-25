/*
 * \brief  Config evaluation
 * \author Alexander Boettcher
 * \date   2020-07-20
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <base/node.h>

#include "session.h"

namespace Cpu {
	class Config;

	using Genode::Node;
}

class Cpu::Config {

	public:

		static void apply(Node const &, Child_list &);
		static void apply_for_thread(Node const &, Cpu::Session &, Thread::Name const &);
};

#endif /* _CONFIG_H_ */
