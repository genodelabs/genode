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

#include <util/xml_node.h>

#include "session.h"

namespace Cpu {
	class Config;

	using Genode::Xml_node;
}

class Cpu::Config {

	public:

		static void apply(Xml_node const &, Child_list &);
};

#endif /* _CONFIG_H_ */
