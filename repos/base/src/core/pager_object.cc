/*
 * \brief  Standard implementation of pager object
 * \author Martin Stein
 * \date   2013-11-04
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

/* core includes */
#include <pager.h>

using namespace Genode;


void Pager_object::wake_up()
{
	warning("user-level page fault handling is not supported on this platform");
}


void Pager_object::unresolved_page_fault_occurred()
{
	state.unresolved_page_fault = true;
}
