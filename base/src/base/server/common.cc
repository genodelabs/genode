/*
 * \brief  Platform-independent part of server-side RPC framework
 * \author Norman Feske
 * \date   2006-05-12
 */

/*
 * Copyright (C) 2006-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/rpc_server.h>
#include <base/blocking.h>

using namespace Genode;


void Rpc_entrypoint::_dissolve(Rpc_object_base *obj)
{
	/* make sure nobody is able to find this object */
	remove(obj);

	/*
	 * The activation may execute a blocking operation in a dispatch function.
	 * Before resolving the corresponding object, we need to ensure that it is
	 * no longer used. Therefore, we to need cancel an eventually blocking
	 * operation and let the activation leave the context of the object.
	 */
	_leave_server_object(obj);

	/* wait until nobody is inside dispatch */
	obj->lock();

	_cap_session->free(obj->cap());

	/* now the object may be safely destructed */
}


void Rpc_entrypoint::_leave_server_object(Rpc_object_base *obj)
{
	Lock::Guard lock_guard(_curr_obj_lock);

	if (obj == _curr_obj)
		cancel_blocking();
}


void Rpc_entrypoint::_block_until_cap_valid()
{
	_cap_valid.lock();
}


Untyped_capability Rpc_entrypoint::reply_dst()
{
	return _ipc_server ? _ipc_server->dst() : Untyped_capability();
}


void Rpc_entrypoint::omit_reply()
{
	/* set current destination to an invalid capability */
	if (_ipc_server) _ipc_server->dst(Untyped_capability());
}


void Rpc_entrypoint::explicit_reply(Untyped_capability reply_cap, int return_value)
{
	if (!_ipc_server) return;

	/* backup reply capability of current request */
	Untyped_capability last_reply_cap = _ipc_server->dst();

	/* direct ipc server to the specified reply destination */
	_ipc_server->ret(return_value);
	_ipc_server->dst(reply_cap);
	*_ipc_server << IPC_REPLY;

	/* restore reply capability of the original request */
	_ipc_server->dst(last_reply_cap);
}


void Rpc_entrypoint::activate()
{
	_delay_start.unlock();
}


Rpc_entrypoint::Rpc_entrypoint(Cap_session *cap_session, size_t stack_size,
                               char const *name, bool start_on_construction)
:
	Thread_base(name, stack_size),
	_cap(Untyped_capability()),
	_curr_obj(0), _cap_valid(Lock::LOCKED), _delay_start(Lock::LOCKED),
	_cap_session(cap_session)
{
	Thread_base::start();
	_block_until_cap_valid();

	if (start_on_construction)
		activate();
}
