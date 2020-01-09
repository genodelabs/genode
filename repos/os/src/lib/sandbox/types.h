/*
 * \brief  Common types used within init
 * \author Norman Feske
 * \date   2017-03-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__SANDBOX__TYPES_H_
#define _LIB__SANDBOX__TYPES_H_

#include <util/string.h>
#include <util/list.h>
#include <session/session.h>

namespace Sandbox {

	class Child;

	using namespace Genode;
	using Genode::size_t;
	using Genode::strlen;

	struct Prio_levels { long value; };

	typedef List<List_element<Sandbox::Child> > Child_list;
}

#endif /* _LIB__SANDBOX__TYPES_H_ */
