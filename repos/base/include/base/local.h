/*
 * \brief  Interfaces for the component-local environment
 * \author Norman Feske
 * \date   2025-04-05
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__LOCAL_H_
#define _INCLUDE__BASE__LOCAL_H_

#include <region_map/region_map.h>
#include <util/allocation.h>

namespace Genode::Local { struct Constrained_region_map; }


/**
 * Access to the component-local virtual address space
 */
struct Genode::Local::Constrained_region_map : Interface, Noncopyable
{
	struct Attr { void *ptr; size_t num_bytes; };

	using Error       = Region_map::Attach_error;
	using Attachment  = Allocation<Constrained_region_map>;
	using Result      = Attachment::Attempt;
	using Attach_attr = Region_map::Attr;

	/**
	 * Map dataspace into local address space
	 *
	 * \param ds    capability of dataspace to map
	 * \param attr  mapping attributes
	 */
	virtual Result attach(Capability<Dataspace> ds, Attach_attr const &attr) = 0;

	/**
	 * Unmap attachment from local address space
	 *
	 * \noapi
	 */
	virtual void _free(Attachment &) = 0;

	/*
	 * Emulation of 'Genode::Region_map', ror a gradual API transition
	 *
	 * \deprecated
	 * \noapi
	 */
	void detach(addr_t addr)
	{
		/* detach via '~Attachment' */
		Attachment { *this, { (void *)addr, 0 } };
	}
};

#endif /* _INCLUDE__BASE__LOCAL_H_ */
