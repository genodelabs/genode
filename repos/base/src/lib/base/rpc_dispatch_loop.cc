/*
 * \brief  Default version of platform-specific part of RPC framework
 * \author Norman Feske
 * \date   2006-05-12
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <util/retry.h>
#include <base/rpc_server.h>

/* base-internal includes */
#include <base/internal/ipc_server.h>

using namespace Genode;


/***********************
 ** Server entrypoint **
 ***********************/

Untyped_capability Rpc_entrypoint::_manage(Rpc_object_base *obj)
{
	/* don't manage RPC object twice */
	if (obj->cap().valid()) {
		warning("attempt to manage RPC object twice");
		return obj->cap();
	}

	return _alloc_rpc_cap(_pd_session, _cap).convert<Untyped_capability>(
		[&] (Untyped_capability cap) {

			/* add server object to object pool */
			obj->cap(cap);
			insert(obj);

			/* return capability that uses the object id as badge */
			return cap;
		},
		[&] (Alloc_error e) {
			error("unable to allocate RPC cap (", e, ")");
			return Untyped_capability();
		});
}


void Rpc_entrypoint::entry()
{
	Ipc_server srv;
	_cap = srv;
	_cap_valid.wakeup();

	Rpc_exception_code exc = Rpc_exception_code(Rpc_exception_code::INVALID_OBJECT);

	while (!_exit_handler.exit) {

		Rpc_request const request = ipc_reply_wait(_caller, exc, _snd_buf, _rcv_buf);
		_caller = request.caller;

		Ipc_unmarshaller unmarshaller(_rcv_buf);
		Rpc_opcode opcode(0);
		unmarshaller.extract(opcode);

		/* set default return value */
		exc = Rpc_exception_code(Rpc_exception_code::INVALID_OBJECT);
		_snd_buf.reset();

		apply(request.badge, [&] (Rpc_object_base *obj)
		{
			if (obj)
				exc = obj->dispatch(opcode, unmarshaller, _snd_buf);
		});
	}

	/* answer exit call, thereby wake up '~Rpc_entrypoint' */
	Msgbuf<16> snd_buf;
	ipc_reply(_caller, Rpc_exception_code(Rpc_exception_code::SUCCESS), snd_buf);

	/* defer the destruction of 'Ipc_server' until '~Rpc_entrypoint' is ready */
	_delay_exit.block();
}
