/*
 * \brief  Slab allocator for signal contexts
 * \author Norman Feske
 * \date   2017-05-10
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SIGNAL_CONTEXT_SLAB_H_
#define _CORE__INCLUDE__SIGNAL_CONTEXT_SLAB_H_

/* Genode includes */
#include <base/slab.h>

/* core-local includes */
#include <signal_source_component.h>

namespace Genode { struct Signal_context_slab; }


/**
 * Slab allocator for signal contexts
 *
 * We define an initial slab block to prevent a dynamic slab-block allocation
 * via '_md_alloc' at the slab's construction time. This would be a problem
 * because the '_md_alloc' supplied by the 'Pd_session_component' constructor
 * uses the PD session itself as backing store (which would be in the middle of
 * construction).
 */
struct Genode::Signal_context_slab : Slab
{
	static constexpr size_t SBS = 960*sizeof(long);
	uint8_t _initial_sb[SBS];

	Signal_context_slab(Allocator &md_alloc)
	: Slab(sizeof(Signal_context_component), SBS, _initial_sb, &md_alloc) { }

	Signal_context_component *any_signal_context()
	{
		return (Signal_context_component *)Slab::any_used_elem();
	}
};

#endif /* _CORE__INCLUDE__SIGNAL_CONTEXT_SLAB_H_ */
