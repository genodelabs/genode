/*
 * \brief  Dummy implementation of Native_capability
 * \author Stefan Kalkowski
 * \date   2015-05-20
 */

/*
 * Copyright (C) 2015-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/internal/capability_space.h>

Genode::Native_capability::Native_capability() { }


void Genode::Native_capability::_inc() { }


void Genode::Native_capability::_dec() { }


long Genode::Native_capability::local_name() const {
	return (long)_data; }


bool Genode::Native_capability::valid() const {
	return (Genode::addr_t)_data != Kernel::cap_id_invalid(); }


Genode::Native_capability::Raw Genode::Native_capability::raw() const {
	return { 0, 0, 0, 0 }; }
