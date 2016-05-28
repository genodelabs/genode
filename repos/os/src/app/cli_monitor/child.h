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
	      char                       const *label,
	      char                       const *binary,
	      Genode::Cap_session              &cap_session,
	      Genode::size_t                    ram_quota,
	      Genode::size_t                    ram_limit,
	      Genode::Signal_context_capability yield_response_sig_cap,
	      Genode::Signal_context_capability exit_sig_cap,
	      Genode::Dataspace_capability      ldso_ds)
	:
		Child_base(ram,
		           label,
		           binary,
		           cap_session,
		           ram_quota,
		           ram_limit,
		           yield_response_sig_cap,
		           exit_sig_cap,
		           ldso_ds),
		argument(label, "subsystem")
	{ }
};

#endif /* _CHILD_H_ */
