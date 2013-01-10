/*
 * \brief  RM-session implementation
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
#include <rm_session_component.h>

using namespace Genode;


void Rm_client::unmap(addr_t core_local_base, addr_t virt_base, size_t size)
{
	PWRN("not implemented");
}
