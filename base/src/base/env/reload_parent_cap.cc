/*
 * \brief  Environment reinitialization
 * \author Norman Feske
 * \date   2012-02-16
 */

/*
 * Copyright (C) 2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/platform_env.h>
#include <base/crt0.h>


void Genode::Platform_env::reload_parent_cap(Native_capability::Dst dst,
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

	/*
	 * Patch new parent capability into the original location as specified by
	 * the linker script.
	 */
	Native_capability::Raw *raw = (Native_capability::Raw *)(&_parent_cap);

	raw->dst        = dst;
	raw->local_name = local_name;

	/*
	 * Re-initialize 'Platform_env' members
	 */
	_parent_client = Parent_client(Genode::parent_cap());
	_resources     = Resources(_parent_client);

	/*
	 * Keep information about dynamically allocated memory but use the new
	 * resources as backing store. Note that the capabilites of the already
	 * allocated backing-store dataspaces are rendered meaningless. But this is
	 * no problem because they are used by the 'Heap' destructor only, which is
	 * never called for heap instance of 'Platform_env'.
	 */
	_heap.reassign_resources(&_resources.ram, &_resources.rm);
}
