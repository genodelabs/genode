/*
 * \brief  Obtain parent capability
 * \author Norman Feske
 * \date   2010-01-26
 *
 * This implementation is used on platforms that rely on global IDs (thread
 * IDs, global unique object IDs) as capability representation.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIB__STARTUP___MAIN_PARENT_CAP_H_
#define _LIB__STARTUP___MAIN_PARENT_CAP_H_

namespace Genode {

	/**
	 * Return constructed parent capability
	 */
	Parent_capability parent_cap()
	{
		Parent_capability cap;
		memcpy(&cap, (void *)&_parent_cap, sizeof(cap));
		return Parent_capability(cap);
	}
}

#endif /* _LIB__STARTUP___MAIN_PARENT_CAP_H_ */
