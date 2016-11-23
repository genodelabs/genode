/*
 * \brief  Child carrying application-specific context information
 * \author Norman Feske
 * \date   2014-10-02
 */

/*
 * Copyright (C) 2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CHILD_H_
#define _CHILD_H_

/* public CLI-monitor includes */
#include <cli_monitor/child.h>

/* local includes */
#include <line_editor.h>

struct Child : Child_base, List<Child>::Element
{
	Argument argument;

	Child(Ram                              &ram,
	      Name                       const &label,
	      Binary_name                const &binary,
	      Genode::Pd_session               &pd_session,
	      Genode::Ram_session              &ref_ram,
	      Genode::Ram_session_capability    ref_ram_cap,
	      Genode::Region_map               &local_rm,
	      Genode::size_t                    ram_quota,
	      Genode::size_t                    ram_limit,
	      Genode::Signal_context_capability yield_response_sig_cap,
	      Genode::Signal_context_capability exit_sig_cap)
	:
		Child_base(ram,
		           label,
		           binary,
		           pd_session,
		           ref_ram,
		           ref_ram_cap,
		           local_rm,
		           ram_quota,
		           ram_limit,
		           yield_response_sig_cap,
		           exit_sig_cap),
		argument(label.string(), "subsystem")
	{ }
};

#endif /* _CHILD_H_ */
