/*
 * \brief  Child carrying application-specific context information
 * \author Norman Feske
 * \date   2014-10-02
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CHILD_H_
#define _CHILD_H_

/* public CLI-monitor includes */
#include <cli_monitor/child.h>

/* local includes */
#include <line_editor.h>

namespace Cli_monitor { struct Child; }


struct Cli_monitor::Child : Child_base, private List<Child>::Element
{
	friend class List<Child>;

	using List<Child>::Element::next;

	Argument argument;

	Child(Genode::Env                      &env,
	      Ram                              &ram,
	      Genode::Allocator                &alloc,
	      Name                       const &label,
	      Binary_name                const &binary,
	      Genode::Pd_session               &ref_pd,
	      Genode::Pd_session_capability     ref_pd_cap,
	      Genode::Region_map               &local_rm,
	      Cap_quota                         cap_quota,
	      Genode::size_t                    ram_quota,
	      Genode::size_t                    ram_limit,
	      Genode::Signal_context_capability yield_response_sig_cap,
	      Genode::Signal_context_capability exit_sig_cap)
	:
		Child_base(env,
		           ram,
		           alloc,
		           label,
		           binary,
		           ref_pd,
		           ref_pd_cap,
		           local_rm,
		           cap_quota,
		           ram_quota,
		           ram_limit,
		           yield_response_sig_cap,
		           exit_sig_cap),
		argument(label.string(), "subsystem")
	{ }
};

#endif /* _CHILD_H_ */
