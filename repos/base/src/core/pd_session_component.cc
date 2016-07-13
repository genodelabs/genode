/*
 * \brief  Core implementation of the PD session interface
 * \author Christian Helmuth
 * \author Norman Feske
 * \date   2006-07-17
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core-local includes */
#include <util.h>
#include <pd_session_component.h>
#include <cpu_session_component.h>

using namespace Genode;


void Pd_session_component::assign_parent(Parent_capability parent)
{
	_pd.assign_parent(parent);
}
