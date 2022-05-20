/*
 * \brief  CTF event base type for the trace recorder
 * \author Johannes Schlatow
 * \date   2022-05-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRACE_RECORDER_POLICY__CTF_H_
#define _TRACE_RECORDER_POLICY__CTF_H_

#include <ctf/event_header.h>
#include <trace_recorder_policy/event.h>

namespace Trace_recorder { struct Ctf_event; }

struct Trace_recorder::Ctf_event : Trace_event<Event_type::CTF>,
                                   Ctf::Event_header_base
{
	static Event_type Type() { return Event_type::CTF; }

	Ctf_event(Genode::uint8_t id) : Ctf::Event_header_base(id) { }
} __attribute__((packed));

#endif /* _TRACE_RECORDER_POLICY__CTF_H_ */
