/*
 * \brief   Kernel backend for protection domains
 * \author  Martin Stein
 * \author  Stefan Kalkowski
 * \date    2012-11-30
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* core includes */
#include <kernel/pd.h>

/* Genode includes */
#include <unmanaged_singleton.h>

using namespace Kernel;

Mode_transition_control::Mode_transition_control()
: _slab(&_allocator), _master(&_table)
{
	assert(Genode::aligned(this, ALIGN_LOG2));
	assert(sizeof(_master) <= _master_context_size());
	assert(_size() <= SIZE);
	map(&_table, &_slab);
	Genode::memcpy(&_mt_master_context_begin, &_master, sizeof(_master));
}


Mode_transition_control * Kernel::mtc()
{
	typedef Mode_transition_control Control;
	return unmanaged_singleton<Control, Control::ALIGN>();
}
