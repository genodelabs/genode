/*
 * \brief  Extract 'Native_pd_args' from '<start>' node of the init config
 * \author Norman Feske
 * \date   2012-11.21
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* init includes */
#include <init/child.h>


/**
 * Read chroot path from XML node
 */
struct Root
{
	char path[Genode::Native_pd_args::ROOT_PATH_MAX_LEN];

	Root(Genode::Xml_node node)
	{
		path[0] = 0;
		try { node.attribute("root").value(path, sizeof(path)); }
		catch (Genode::Xml_node::Nonexistent_attribute) { }
	}
};


/**
 * Read unsigned ID from XML node
 */
static unsigned id_value(char const *attr, Genode::Xml_node node)
{
	unsigned value = 0;
	try { node.attribute(attr).value(&value); }
	catch (Genode::Xml_node::Nonexistent_attribute) { }
	return value;
}


Init::Child::Pd_args::Pd_args(Genode::Xml_node start_node)
:
	Genode::Native_pd_args(Root(start_node).path,
	                       id_value("uid", start_node),
	                       id_value("gid", start_node))
{ }


void Init::Child_policy_pd_args::filter_session_args(char const    *session,
                                                     char          *args,
                                                     Genode::size_t args_len)
{
	/*
	 * Specify 'Genode' namespace to remove possible ambiguity of
	 * 'strcmp' when including the header along with libc headers.
	 */
	if (Genode::strcmp(session, "PD") != 0)
		return;

	/*
	 * Apply platform-specific PD-session arguments only if specified.
	 */
	if (!_pd_args)
		return;

	using namespace Genode;

	/*
	 * Prepend the '_root' to the 'root' session argument of PD sessions
	 * initiated through the child (not the child's PD session).
	 */
	if (_pd_args->root() && _pd_args->root()[0]) {

		char path[Parent::Session_args::MAX_SIZE];
		Arg_string::find_arg(args, "root").string(path, sizeof(path), "");

		char value[Parent::Session_args::MAX_SIZE];
		Genode::snprintf(value, sizeof(value),
		                 "\"%s%s\"",
		                 _pd_args->root(), path);

		Arg_string::set_arg(args, args_len, "root", value);
	}

	/*
	 * Add user ID and group ID to session arguments
	 */
	if (_pd_args->uid())
		Arg_string::set_arg(args, args_len, "uid", _pd_args->uid());

	if (_pd_args->gid())
		Arg_string::set_arg(args, args_len, "gid", _pd_args->gid());
}

