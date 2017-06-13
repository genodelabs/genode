/*
 * \brief  Environment reinitialization
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-02-16
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/construct_at.h>
#include <rm_session/connection.h>

/* base-internal includes */
#include <base/internal/platform_env.h>
#include <base/internal/crt0.h>

void prepare_reinit_main_thread();

void reinit_main_thread();

namespace Genode { extern bool inhibit_tracing; }


void Genode::Platform_env::reinit(Native_capability::Raw raw)
{
	/*
	 * This function is unused during the normal operation of Genode. It is
	 * relevant only for implementing fork semantics such as provided by the
	 * Noux execution environment.
	 *
	 * The function is called by the freshly created process right after the
	 * fork happened.
	 *
	 * The existing 'Platform_env' object contains capabilities that are
	 * meaningful for the forking process but not the new process. Before the
	 * the environment can be used, it must be reinitialized with the resources
	 * provided by the actual parent.
	 */

	/* avoid RPCs by the tracing framework as long as we have no valid env */
	inhibit_tracing = true;

	/* do platform specific preparation */
	prepare_reinit_main_thread();

	/*
	 * Patch new parent capability into the original location as specified by
	 * the linker script.
	 */
	*(Native_capability::Raw *)(&_parent_cap) = raw;

	/*
	 * Re-initialize 'Platform_env' members
	 */
	Expanding_parent_client * const p = &_parent_client;
	construct_at<Expanding_parent_client>(p, parent_cap());
	construct_at<Resources>(&_resources, _parent_client);

	/*
	 * Keep information about dynamically allocated memory but use the new
	 * resources as backing store. Note that the capabilites of the already
	 * allocated backing-store dataspaces are rendered meaningless. But this is
	 * no problem because they are used by the 'Heap' destructor only, which is
	 * never called for heap instance of 'Platform_env'.
	 */
	_heap.reassign_resources(&_resources.pd, &_resources.rm);
}


void
Genode::Platform_env::
reinit_main_thread(Capability<Region_map> &stack_area_rm)
{
	/* reinitialize stack area RM session */
	Region_map * const rms = env_stack_area_region_map;
	Region_map_client * const rmc = dynamic_cast<Region_map_client *>(rms);
	construct_at<Region_map_client>(rmc, stack_area_rm);

	/* reinitialize main-thread object */
	::reinit_main_thread();

	/* re-enable tracing */
	inhibit_tracing = false;
}
