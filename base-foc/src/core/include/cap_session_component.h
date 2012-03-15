/*
 * \brief  Capability session service
 * \author Stefan Kalkowski
 * \date   2011-01-13
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_
#define _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_

/* Genode includes */
#include <base/rpc_server.h>

namespace Genode {

	class Cap_session_component : public Rpc_object<Cap_session>
	{
		private:

			static long _unique_id_cnt; /* TODO: remove this from generic core code */

		public:

			Native_capability alloc(Native_capability ep);

			void free(Native_capability cap);

			static Native_capability alloc(Cap_session_component *session,
			                               Native_capability ep);
	};
}

#endif /* _CORE__INCLUDE__CAP_SESSION_COMPONENT_H_ */
