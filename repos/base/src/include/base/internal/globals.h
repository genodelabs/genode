/*
 * \brief  Interfaces to library-global objects
 * \author Norman Feske
 * \date   2016-04-29
 *
 * \deprecated  This header should be removed once we have completed the
 *              transition to the modernized API.
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__GLOBALS_H_
#define _INCLUDE__BASE__INTERNAL__GLOBALS_H_

namespace Genode {

	class Region_map;
	class Ram_session;
	class Env;

	extern Region_map  *env_stack_area_region_map;
	extern Ram_session *env_stack_area_ram_session;

	void init_signal_thread(Env &);
	void init_log();
}

#endif /* _INCLUDE__BASE__INTERNAL__GLOBALS_H_ */
