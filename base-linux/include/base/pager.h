/*
 * \brief  Paging-server framework
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-04-28
 *
 * Linux dummies
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__PAGER_H_
#define _INCLUDE__BASE__PAGER_H_

#include <base/signal.h>
#include <pager/capability.h>
#include <cap_session/cap_session.h>

namespace Genode {

	struct Pager_object
	{
		virtual ~Pager_object() { }

		void exception_handler(Signal_context_capability) { }
	};

	class Pager_activation_base { };
	struct Pager_entrypoint
	{
		Pager_entrypoint(Cap_session *, Pager_activation_base *) { }

		Pager_object *obj_by_cap(Pager_capability) { return 0; }
	};
	template <int FOO> class Pager_activation : public Pager_activation_base { };
}

#endif /* _INCLUDE__BASE__PAGER_H_ */
