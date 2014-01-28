/*
 * \brief  Environment reinitialization
 * \author Norman Feske
 * \author Martin Stein
 * \date   2012-02-16
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* env includes */
#include <platform_env.h>

/* Genode includes */
#include <util/construct_at.h>
#include <base/crt0.h>
#include <rm_session/connection.h>

void prepare_reinit_main_thread();

void reinit_main_thread();

namespace Genode
{
	extern bool inhibit_tracing;

	Rm_session * env_context_area_rm_session();
}


void Genode::Platform_env::reinit(Native_capability::Dst dst,
                                  long local_name)
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
	Native_capability::Raw *raw = (Native_capability::Raw *)(&_parent_cap);
	raw->dst                    = dst;
	raw->local_name             = local_name;

	/*
	 * Re-initialize 'Platform_env' members
	 */
	Expanding_parent_client * const p = &_parent_client;
	construct_at<Expanding_parent_client>(p, parent_cap(), *this);
	construct_at<Resources>(&_resources, _parent_client);

	/*
	 * Keep information about dynamically allocated memory but use the new
	 * resources as backing store. Note that the capabilites of the already
	 * allocated backing-store dataspaces are rendered meaningless. But this is
	 * no problem because they are used by the 'Heap' destructor only, which is
	 * never called for heap instance of 'Platform_env'.
	 */
	_heap.reassign_resources(&_resources.ram, &_resources.rm);
}


void
Genode::Platform_env::
reinit_main_thread(Rm_session_capability & context_area_rm)
{
	/* reinitialize context area RM session */
	Rm_session * const rms = env_context_area_rm_session();
	Rm_session_client * const rmc = dynamic_cast<Rm_session_client *>(rms);
	construct_at<Rm_session_client>(rmc, context_area_rm);

	/* re-enable tracing */
	inhibit_tracing = false;

	/* reinitialize main-thread object */
	::reinit_main_thread();
}
