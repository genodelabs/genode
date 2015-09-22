/*
 * \brief  Protection-domain facility
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* core includes */
#include <platform_pd.h>

using namespace Genode;


/***************************
 ** Public object members **
 ***************************/

int Platform_pd::bind_thread(Platform_thread *thread)
{
	thread->bind_to_pd(this, _thread_cnt == 0);
	_thread_cnt++;
	return 0;
}


void Platform_pd::unbind_thread(Platform_thread *thread)
{
	PDBG("not implemented");
}


int Platform_pd::assign_parent(Native_capability parent)
{
	if (_parent.valid()) return -1;
	_parent = parent;
	return 0;
}


Platform_pd::Platform_pd(Allocator * md_alloc, char const *,
                         signed pd_id, bool create)
: _thread_cnt(0), _pd_sel(~0UL) { }


Platform_pd::~Platform_pd()
{
	/* invalidate weak pointers to this object */
	Address_space::lock_for_destruction();

	if (_pd_sel == ~0UL) return;

	/* Revoke and free cap, pd is gone */
	Nova::revoke(Nova::Obj_crd(_pd_sel, 0));
	cap_map()->remove(_pd_sel, 0, false);
}
