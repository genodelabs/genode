/*
 * \brief  Protection-domain facility
 * \author Norman Feske
 * \date   2009-10-02
 */

/*
 * Copyright (C) 2009-2011 Genode Labs GmbH
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


static int id_cnt;


Platform_pd::Platform_pd(signed pd_id, bool create)
: _thread_cnt(0), _id(++id_cnt), _pd_sel(0) { }


Platform_pd::~Platform_pd()
{ }
