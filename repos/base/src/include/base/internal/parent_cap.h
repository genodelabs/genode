/*
 * \brief  Interface to obtain the parent capability for the component
 * \author Norman Feske
 * \date   2013-09-25
 *
 * This implementation is used on platforms that rely on global IDs (thread
 * IDs, global unique object IDs) as capability representation.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_
#define _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_

/* Genode includes */
#include <parent/capability.h>
#include <util/string.h>

/* base-internal includes */
#include <base/internal/crt0.h>


namespace Genode {

	static inline Parent_capability parent_cap()
	{
		Parent_capability cap;
		memcpy(&cap, (void *)&_parent_cap, sizeof(cap));
		return Parent_capability(cap);
	}
}

#endif /* _INCLUDE__BASE__INTERNAL__PARENT_CAP_H_ */
