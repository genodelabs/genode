/*
 * \brief  Wrapper type for trace events with sub-types
 * \author Johannes Schlatow
 * \date   2022-05-20
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _TRACE_RECORDER_POLICY__EVENT_H_
#define _TRACE_RECORDER_POLICY__EVENT_H_

#include <base/exception.h>

namespace Trace_recorder {
	enum Event_type { PCAPNG = 1, CTF };

	struct Trace_event_base;

	template <Event_type>
	struct Trace_event;
}


struct Trace_recorder::Trace_event_base
{
	struct Cast_failed : Genode::Exception { };

	const Event_type _type;

	Trace_event_base(Event_type type)
	: _type(type)
	{ }

	Event_type type() const { return _type; }

	template <typename T>
	T const &event() const
	{
		if (_type != T::Type())
			throw Cast_failed();

		return *reinterpret_cast<const T*>(this);
	}

	/* define placement new since construct_at in policy inflates policy size */
	void *operator new(__SIZE_TYPE__, void *p) { return p; }
} __attribute__((packed));


template <Trace_recorder::Event_type TYPE>
struct Trace_recorder::Trace_event : Trace_event_base
{
	Trace_event()
	: Trace_event_base(TYPE)
	{ }

} __attribute__((packed));

#endif /* _TRACE_RECORDER_POLICY__EVENT_H_ */
