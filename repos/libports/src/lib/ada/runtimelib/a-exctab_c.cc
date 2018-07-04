/*
 * \brief Provide dummy for custom Ada exceptions
 * \author Johannes Kliemann
 * \date 2018-06-25
 *
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 * Copyright (C) 2018 Componolit GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/log.h>

extern "C" {

	void system__exception_table__register()
	{
		Genode::warning(__func__, " not implemented");
	}

}
