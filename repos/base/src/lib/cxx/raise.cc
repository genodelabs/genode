/*
 * \brief  Reflect error conditions as C++ exceptions
 * \author Norman Feske
 * \date   2025-03-05
 */

/*
 * Copyright (C) 2025 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/exception.h>
#include <base/error.h>


void Genode::raise(Alloc_error error)
{
	switch (error) {
	case Alloc_error::OUT_OF_RAM:  throw Out_of_ram();
	case Alloc_error::OUT_OF_CAPS: throw Out_of_caps();
	case Alloc_error::DENIED:      break;
	}
	throw Denied();
}
