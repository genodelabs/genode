/*
 * \brief  Platform CPU memory area preparation
 * \author Stefan Kalkowski
 * \date   2024-04-25
 */

/*
 * Copyright (C) 2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <platform.h>


unsigned Bootstrap::Platform::_prepare_cpu_memory_area()
{
	using namespace Genode;

	for (size_t id = 0; id < ::Board::NR_OF_CPUS; id++)
		_prepare_cpu_memory_area(id);
	return ::Board::NR_OF_CPUS;
}
