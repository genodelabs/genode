/*
 * \brief  Default version of platform-specific part of server framework
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2006-05-12
 *
 * This version is suitable for platforms similar to L4. Each platform
 * for which this implementation is not suited contains a platform-
 * specific version in its respective 'base-<platform>' repository.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <internal/capability_space_sel4.h>
#include <base/rpc_server.h>

using namespace Genode;


/***********************
 ** Server entrypoint **
 ***********************/

Untyped_capability Rpc_entrypoint::_manage(Rpc_object_base *obj)
{
	Untyped_capability new_obj_cap = _cap_session->alloc(_cap);

	/* add server object to object pool */
	obj->cap(new_obj_cap);
	insert(obj);

	/* return capability that uses the object id as badge */
	return new_obj_cap;
}


void Rpc_entrypoint::entry()
{
	Ipc_server srv(&_snd_buf, &_rcv_buf);
	_ipc_server = &srv;
	_cap = srv;
	_cap_valid.unlock();

	/*
	 * Now, the capability of the server activation is initialized
	 * an can be passed around. However, the processing of capability
	 * invocations should not happen until activation-using server
	 * is completely initialized. Thus, we wait until the activation
	 * gets explicitly unblocked by calling 'Rpc_entrypoint::activate()'.
	 */
	_delay_start.lock();

	while (!_exit_handler.exit) {

		int opcode = 0;

		srv >> IPC_REPLY_WAIT >> opcode;

		/* set default return value */
		srv.ret(Ipc_client::ERR_INVALID_OBJECT);

		/* atomically lookup and lock referenced object */
		auto lambda = [&] (Rpc_object_base *obj) {
			if (!obj) return;

			/* dispatch request */
			try { srv.ret(obj->dispatch(opcode, srv, srv)); }
			catch (Blocking_canceled) { }
		};
		apply(srv.badge(), lambda);
	}

	/* answer exit call, thereby wake up '~Rpc_entrypoint' */
	srv << IPC_REPLY;

	/* defer the destruction of 'Ipc_server' until '~Rpc_entrypoint' is ready */
	_delay_exit.lock();
}
