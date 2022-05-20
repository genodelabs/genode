/*
 * \brief  Helper for storing static parts of Trace::Subject_info
 * \author Johannes Schlatow
 * \date   2021-08-06
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SUBJECT_INFO_H_
#define _SUBJECT_INFO_H_

#include <base/trace/types.h>

struct Subject_info
{
	Genode::Session_label        _session_label;
	Genode::Trace::Thread_name   _thread_name;
	Genode::Affinity::Location   _affinity;
	unsigned                     _priority;

	Subject_info(Genode::Trace::Subject_info const &info)
	: _session_label(info.session_label()),
	  _thread_name(info.thread_name()),
	  _affinity(info.affinity()),
	  _priority(info.execution_time().priority)
	{ }

	Genode::Session_label      const &session_label() const { return _session_label; }
	Genode::Trace::Thread_name const &thread_name()   const { return _thread_name; }
	Genode::Affinity::Location const &affinity()      const { return _affinity; }
	unsigned                          priority()      const { return _priority; }
};

#endif /* _SUBJECT_INFO_H_ */
