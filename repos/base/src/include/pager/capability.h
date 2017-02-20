/*
 * \brief  Pager capability type
 * \author Norman Feske
 * \date   2010-01-27
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__PAGER__CAPABILITY_H_
#define _INCLUDE__PAGER__CAPABILITY_H_

#include <base/capability.h>

namespace Genode {

	/*
	 * The 'Pager_capability' type is returned by 'Region_map::add_client' and
	 * passed as argument to 'Cpu_session::set_pager'. It is never invoked or
	 * otherwise used.
	 */
	class Pager_object;
	typedef Capability<Pager_object> Pager_capability;
}

#endif /* _INCLUDE__PAGER__CAPABILITY_H_ */
