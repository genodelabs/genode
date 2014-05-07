/*
 * \brief  Dataspace object for object pool
 * \author Christian Prochaska
 * \date   2011-09-12
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _DATASPACE_OBJECT_H_
#define _DATASPACE_OBJECT_H_

#include <base/object_pool.h>
#include <dataspace/capability.h>

namespace Gdb_monitor {

	using namespace Genode;

	class Rm_session_component;

	class Dataspace_object : public Object_pool<Dataspace_object>::Entry
	{
		private:

			Rm_session_component *_rm_session_component;

		public:

			Dataspace_object(Dataspace_capability ds_cap, Rm_session_component *rm_session_component)
			: Object_pool<Dataspace_object>::Entry(ds_cap),
			  _rm_session_component(rm_session_component) { }

			Rm_session_component *rm_session_component() { return _rm_session_component; }
	};

}

#endif /* _DATASPACE_OBJECT_H_ */
