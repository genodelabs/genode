/*
 * \brief  VirtualBox runtime (RT)
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/env.h>

/* VirtualBox includes */
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/time.h>
#include <internal/iprt.h>


static void *alloc_mem(size_t cb, const char *pszTag, bool executable = false)
{
	void * local_addr = nullptr;

	using namespace Genode;

	try {
		Ram_dataspace_capability ds = env()->ram_session()->alloc(cb);
		Assert(ds.valid());

		size_t        const whole_size = 0;
		Genode::off_t const offset     = 0;
		bool          const any_addr   = false;
		void *              any_local_addr = nullptr;

		local_addr = env()->rm_session()->attach(ds, whole_size, offset,
		                                         any_addr, any_local_addr,
		                                         executable);

		if (!local_addr)
			PERR("size=0x%zx, tag=%s -> %p", cb, pszTag, local_addr);
		Assert(local_addr);

	} catch (...) {
		Assert(!"Could not allocate RTMem* memory ");
	}

	return local_addr;
}


/*
 * Called by the recompiler to allocate executable RAM
 */
void *RTMemExecAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
	return alloc_mem(cb, pszTag, true);
}


void *RTMemPageAllocZTag(size_t cb, const char *pszTag) RT_NO_THROW
{
	/*
	 * The RAM dataspace freshly allocated by 'RTMemExecAllocTag' is zeroed
	 * already.
	 */
	return alloc_mem(cb, pszTag);
}


void *RTMemPageAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
	return alloc_mem(cb, pszTag);
}


void RTMemPageFree(void *pv, size_t cb) RT_NO_THROW
{
	PERR("%s %p+%zx", __func__, pv, cb);
}

#include <iprt/buildconfig.h>

uint32_t RTBldCfgVersionMajor(void) { return VBOX_VERSION_MAJOR; }
uint32_t RTBldCfgVersionMinor(void) { return VBOX_VERSION_MINOR; }
uint32_t RTBldCfgVersionBuild(void) { return VBOX_VERSION_BUILD; }
uint32_t RTBldCfgRevision(void)     { return ~0; }


extern "C" DECLHIDDEN(int) rtProcInitExePath(char *pszPath, size_t cchPath)
{
	Genode::strncpy(pszPath, "/virtualbox", cchPath);
	return 0;
}
