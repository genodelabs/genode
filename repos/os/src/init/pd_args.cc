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


Init::Child::Pd_args::Pd_args(Genode::Xml_node start_node) { }


void Init::Child_policy_pd_args::filter_session_args(char const *,
                                                     char *, Genode::size_t)
{ }
