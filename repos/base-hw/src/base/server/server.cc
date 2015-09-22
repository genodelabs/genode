/*
 * \brief  base-hw specific part of RPC framework
 * \author Stefan Kalkowski
 * \date   2015-03-05
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/rpc_server.h>
#include <base/sleep.h>
#include <base/env.h>
#include <util/retry.h>
#include <cap_session/client.h>

using namespace Genode;


/***********************
 ** Server entrypoint **
 ***********************/

Untyped_capability Rpc_entrypoint::_manage(Rpc_object_base *obj)
{
	Untyped_capability new_obj_cap =
		retry<Genode::Cap_session::Out_of_metadata>(
			[&] () { return _cap_session->alloc(_cap); },
			[&] () {
				Cap_session_client *client =
				dynamic_cast<Cap_session_client*>(_cap_session);
				if (client)
					env()->parent()->upgrade(*client, "ram_quota=16K");
			});

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
		apply(srv.badge(), [&] (Rpc_object_base *curr_obj) {
			if (!curr_obj) return;

			/* dispatch request */
			try { srv.ret(curr_obj->dispatch(opcode, srv, srv)); }
			catch (Blocking_canceled) { }
		});
	}

	/* answer exit call, thereby wake up '~Rpc_entrypoint' */
	srv << IPC_REPLY;

	/* defer the destruction of 'Ipc_server' until '~Rpc_entrypoint' is ready */
	_delay_exit.lock();
}
