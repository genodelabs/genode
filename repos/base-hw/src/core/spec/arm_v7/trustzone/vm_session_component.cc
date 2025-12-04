/*
 * \brief  VM session component for 'base-hw'
 * \author Stefan Kalkowski
 * \date   2012-10-08
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <vm_session_component.h>
#include <vm_root.h>

using namespace Core;

template <>
void Vm_session_component<Board::Vm_page_table>::attach_pic(addr_t) { }


Core::Vm_root::Create_result Core::Vm_root::_create_session(const char *args)
{
	using Component = Vm_session_component<Board::Vm_page_table>;
	return *new (md_alloc()) Component(_registry, _vmid_alloc, *ep(),
	                                   session_resources_from_args(args),
	                                   session_label_from_args(args),
	                                   _ram_allocator, _mapped_ram, _local_rm,
	                                   _trace_sources);
}
