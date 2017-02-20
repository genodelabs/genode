/*
 * \brief  Dataspace object for object pool
 * \author Christian Prochaska
 * \date   2011-09-12
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DATASPACE_OBJECT_H_
#define _DATASPACE_OBJECT_H_

#include <base/object_pool.h>
#include <dataspace/capability.h>

namespace Gdb_monitor {

	using namespace Genode;

	class Region_map_component;

	class Dataspace_object;

	typedef Object_pool<Dataspace_object> Dataspace_pool;

	class Dataspace_object : public Dataspace_pool::Entry
	{
		private:

			Region_map_component *_region_map_component;

		public:

			Dataspace_object(Dataspace_capability ds_cap, Region_map_component *region_map_component)
			: Object_pool<Dataspace_object>::Entry(ds_cap),
			  _region_map_component(region_map_component) { }

			Region_map_component *region_map_component() { return _region_map_component; }
	};

}

#endif /* _DATASPACE_OBJECT_H_ */
