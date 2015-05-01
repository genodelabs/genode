/*
 * \brief   Protection-domain facility
 * \author  Norman Feske
 * \date    2015-05-01
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <platform_pd.h>
#include <platform.h>
#include <util.h>

using namespace Genode;


int Platform_pd::bind_thread(Platform_thread *thread)
{
	PDBG("not implemented");
	return 0;
}


void Platform_pd::unbind_thread(Platform_thread *thread)
{
	PDBG("not implemented");
}


Platform_pd::Platform_pd(Allocator * md_alloc, size_t ram_quota,
                         char const *, signed pd_id, bool create)
{
	PDBG("not implemented");
}


Platform_pd::~Platform_pd()
{
	PWRN("not implemented");
}
