/*
 * \brief  Implementation of platform-specific capabilities for core
 * \author Stefan Kalkowski
 * \date   2015-05-20
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */
#include <base/capability.h>

void Genode::Native_capability::_inc() const { }

void Genode::Native_capability::_dec() const { }
