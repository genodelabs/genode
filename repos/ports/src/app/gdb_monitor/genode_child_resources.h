/*
 * \brief  Genode child resources provided to GDB monitor
 * \author Christian Prochaska
 * \date   2011-03-10
 */

/*
 * Copyright (C) 2011-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _GENODE_CHILD_RESOURCES_H_
#define _GENODE_CHILD_RESOURCES_H_

#include "region_map_component.h"

namespace Gdb_monitor {
	class Cpu_session_component;
	class Genode_child_resources;
}

class Gdb_monitor::Genode_child_resources
{

	private:

		Cpu_session_component *_cpu_session_component = 0;
		Region_map_component  *_region_map_component = 0;

	public:

		void cpu_session_component(Cpu_session_component *cpu_session_component)
		{
			_cpu_session_component = cpu_session_component;
		}

		void region_map_component(Region_map_component *region_map_component)
		{
			_region_map_component = region_map_component;
		}

		Cpu_session_component *cpu_session_component() { return _cpu_session_component; }
		Region_map_component *region_map_component() { return _region_map_component; }
};

#endif /* _GENODE_CHILD_RESOURCES_H_ */
