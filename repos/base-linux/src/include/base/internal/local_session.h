/*
 * \brief  Meta data for component-local sessions
 * \author Norman Feske
 * \date   2016-10-13
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCAL_SESSION_H_
#define _INCLUDE__BASE__INTERNAL__LOCAL_SESSION_H_

/* Genode includes */
#include <base/id_space.h>

namespace Genode { struct Local_session; }


struct Genode::Local_session : Parent::Client
{
	private:

		Id_space<Parent::Client>::Element _id_space_element;

		Session_capability _cap;

	public:

		Local_session(Id_space<Parent::Client> &id_space, Parent::Client::Id id,
		              Session &session)
		:
			_id_space_element(*this, id_space, id),
			_cap(Local_capability<Session>::local_cap(&session))
		{ }

		Capability<Session> local_session_cap() { return _cap; }
};


#endif /* _INCLUDE__BASE__INTERNAL__LOCAL_SESSION_H_ */
