/*
 * \brief   Protection-domain facility
 * \author  Norman Feske
 * \date    2009-10-02
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
	PWRN("not implemented");
	return -1;
}


void Platform_pd::unbind_thread(Platform_thread *thread)
{
	PWRN("not implemented");
}


Platform_pd::Platform_pd(bool core)
{
	PWRN("not yet implemented");
}


Platform_pd::Platform_pd(char const *, signed pd_id, bool create)
{
	PWRN("not yet implemented");
}


Platform_pd::~Platform_pd()
{
	PWRN("not yet implemented");
}
