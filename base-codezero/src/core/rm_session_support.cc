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

/* core includes */
#include <rm_session_component.h>
#include <util.h>

/* Codezero includes */
#include <codezero/syscalls.h>

using namespace Genode;
using namespace Codezero;


void Rm_client::unmap(addr_t core_local_base, addr_t virt_base, size_t size)
{
	l4_unmap((void *)virt_base, size >> get_page_size_log2(), badge());
}
