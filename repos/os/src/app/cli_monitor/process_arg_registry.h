/*
 * \brief  Registry of process names used as arguments
 * \author Norman Feske
 * \date   2013-03-18
 */

/*
 * Copyright (C) 2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _PROCESS_ARG_REGISTRY_H_
#define _PROCESS_ARG_REGISTRY_H_

/**
 * Registry of arguments referring to the currently running processes
 */
struct Process_arg_registry
{
	Genode::List<Argument> list;
};

#endif /* _PROCESS_ARG_REGISTRY_H_ */
