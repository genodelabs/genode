/*
 * \brief  Skeleton for implementing servers
 * \author Norman Feske
 * \date   2013-09-07
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/component.h>
#include <os/server.h>


Genode::size_t Component::stack_size()
{
	return Server::stack_size();
}


static Genode::Env *_env;


void Component::construct(Genode::Env &env)
{
	_env = &env;
	Server::construct(env.ep());
}


void Server::wait_and_dispatch_one_signal()
{
	if (_env)
		_env->ep().wait_and_dispatch_one_signal();
}
