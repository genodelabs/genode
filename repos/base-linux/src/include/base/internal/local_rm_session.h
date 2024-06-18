/*
 * \brief  Component-local implementation of a RM session
 * \author Norman Feske
 * \date   2016-04-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
	Region_map &_local_rm;
	Allocator  &_md_alloc;

	Local_rm_session(Region_map &local_rm, Allocator &md_alloc,
	                 Id_space<Parent::Client> &id_space, Parent::Client::Id id)
	:
		Local_session(id_space, id, *this),
		_local_rm(local_rm), _md_alloc(md_alloc)
	{ }

	Create_result create(size_t size) override
	{
		try {
			Region_map *rm = new (_md_alloc) Region_map_mmap(true, size);
			return Local_capability<Region_map>::local_cap(rm);
		}
		catch (Out_of_ram)  { return Create_error::OUT_OF_RAM; }
		catch (Out_of_caps) { return Create_error::OUT_OF_CAPS; }
	}

	void destroy(Capability<Region_map> cap) override
	{
		Region_map *rm_ptr = Local_capability<Region_map>::deref(cap);

		/* detach sub region map from local address space */
		Region_map_mmap &rm = static_cast<Region_map_mmap &>(*rm_ptr);
		rm.with_attached_sub_rm_base_ptr([&] (void *base_ptr) {
			_local_rm.detach(addr_t(base_ptr)); });

		Genode::destroy(_md_alloc, &rm);
	}
};

#endif /* _INCLUDE__BASE__INTERNAL__LOCAL_RM_SESSION_H_ */
