/*
 * \brief  Common exception types
 * \author Norman Feske
 * \date   2008-03-22
 */

/*
 * Copyright (C) 2008-2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__EXCEPTION_H_
#define _INCLUDE__BASE__EXCEPTION_H_

namespace Genode {

	struct Exception { };
	struct Out_of_ram  : Exception { };
	struct Out_of_caps : Exception { };
	struct Denied      : Exception { };

	struct Index_out_of_bounds      : Exception { };
	struct Access_unconstructed_obj : Exception { };
	struct Nonexistent_sub_node     : Exception { };
	struct Ipc_error                : Exception { };
}

#endif /* _INCLUDE__BASE__EXCEPTION_H_ */
