/*
 * \brief  Error classes specific to base-hw kernel/core
 * \author Stefan Kalkowski
 * \date   2025-06-16
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__LIB__HW__ERROR_H_
#define _SRC__LIB__HW__ERROR_H_

namespace Hw {
	enum class Page_table_error { DOUBLE_INSERT, INVALID_RANGE, ALLOC_ERROR };
}

#endif /* _SRC__LIB__HW__ERROR_H_ */
