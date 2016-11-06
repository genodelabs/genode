/*
 * \brief  Component-local implementation of a RM session
 * \author Norman Feske
 * \date   2016-04-19
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__LOCAL_RM_SESSION_H_
#define _INCLUDE__BASE__INTERNAL__LOCAL_RM_SESSION_H_

/* Genode includes */
#include <rm_session/rm_session.h>
#include <base/allocator.h>

/* base-internal includes */
#include <base/internal/local_session.h>
#include <base/internal/region_map_mmap.h>
#include <base/internal/local_capability.h>

namespace Genode { struct Local_rm_session; }


struct Genode::Local_rm_session : Rm_session, Local_session
{
	Allocator &md_alloc;

	Local_rm_session(Allocator &md_alloc, Id_space<Parent::Client> &id_space,
	                 Parent::Client::Id id)
	:
		Local_session(id_space, id, *this), md_alloc(md_alloc)
	{ }

	Capability<Region_map> create(size_t size)
	{
		Region_map *rm = new (md_alloc) Region_map_mmap(true, size);
		return Local_capability<Region_map>::local_cap(rm);
	}

	void destroy(Capability<Region_map> cap)
	{
		Region_map *rm = Local_capability<Region_map>::deref(cap);
		Genode::destroy(md_alloc, rm);
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__LOCAL_RM_SESSION_H_ */
