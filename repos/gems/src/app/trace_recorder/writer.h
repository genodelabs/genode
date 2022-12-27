/*
 * \brief  Base class for processing traces and writing outputs
 * \author Johannes Schlatow
 * \date   2022-05-11
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _WRITER_H_
#define _WRITER_H_

/* local includes */
#include <subject_info.h>

/* Genode includes */
#include <base/registry.h>
#include <os/vfs.h>
#include <trace/trace_buffer.h>
#include <trace_recorder_policy/event.h>

namespace Trace_recorder {
	class Writer_base;

	using Directory       = Genode::Directory;
	using Writer_registry = Genode::Registry<Writer_base>;
}


class Trace_recorder::Writer_base
{
	protected:

		Writer_registry::Element _element;

	public:

		Writer_base(Writer_registry &registry)
		: _element(registry, *this)
		{ }

		virtual ~Writer_base() { }

		/***************
		 ** Interface **
		 ***************/

		virtual void start_iteration(Directory &,
		                             Directory::Path const &,
		                             ::Subject_info const &) = 0;

		virtual void process_event(Trace_recorder::Trace_event_base const &, Genode::size_t) = 0;

		virtual void end_iteration() = 0;
};


#endif /* _WRITER_H_ */
