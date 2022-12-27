/*
 * \brief  Generic type for CTF event headers
 * \author Johannes Schlatow
 * \date   2021-08-03
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CTF__EVENT_HEADER_H_
#define _CTF__EVENT_HEADER_H_

#include <base/fixed_stdint.h>
#include <trace/timestamp.h>
#include <ctf/timestamp.h>

namespace Ctf {
	using namespace Genode::Trace;

	struct Event_header_base;
}

/**
 * Base type for CTF trace events. The event type is identifed by its id.
 */
struct Ctf::Event_header_base
{
	const uint8_t    _id;
	Timestamp_base   _timestamp { (Timestamp_base)Trace::timestamp() };

	Timestamp_base    timestamp() const            { return _timestamp; }
	void              timestamp(Timestamp_base ts) { _timestamp = ts; }

	Event_header_base(uint8_t id)
	: _id(id)
	{ }

} __attribute__((packed));


#endif /* _CTF__EVENT_HEADER_H_ */
