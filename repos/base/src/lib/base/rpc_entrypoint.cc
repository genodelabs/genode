/*
 * \brief  Platform-independent part of server-side RPC framework
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
#include <base/rpc_server.h>
#include <base/rpc_client.h>
#include <base/blocking.h>
#include <base/env.h>

/* base-internal includes */
#include <base/internal/ipc_server.h>
#include <signal_source/signal_source.h>

using namespace Genode;


void Rpc_entrypoint::_dissolve(Rpc_object_base *obj)
{
	/* make sure nobody is able to find this object */
	remove(obj);

	_free_rpc_cap(_pd_session, obj->cap());

	/* effectively invalidate the capability used before */
	obj->cap(Untyped_capability());

	/* now the object may be safely destructed */
}


void Rpc_entrypoint::_block_until_cap_valid()
{
	_cap_valid.lock();
}


void Rpc_entrypoint::reply_signal_info(Untyped_capability reply_cap,
                                       unsigned long imprint, unsigned long cnt)
{
	Msgbuf<sizeof(Signal_source::Signal)> snd_buf;
	snd_buf.insert(Signal_source::Signal(imprint, cnt));
	ipc_reply(reply_cap, Rpc_exception_code(Rpc_exception_code::SUCCESS), snd_buf);
}


void Rpc_entrypoint::activate()
{
	_delay_start.unlock();
}


bool Rpc_entrypoint::is_myself() const
{
	return (Thread::myself() == this);
}


Rpc_entrypoint::Rpc_entrypoint(Pd_session *pd_session, size_t stack_size,
                               char const *name, bool start_on_construction,
                               Affinity::Location location)
:
	Thread(Cpu_session::Weight::DEFAULT_WEIGHT, name, stack_size, location),
	_cap(Untyped_capability()),
	_cap_valid(Lock::LOCKED), _delay_start(Lock::LOCKED),
	_delay_exit(Lock::LOCKED),
	_pd_session(*pd_session)
{
	Thread::start();
	_block_until_cap_valid();

	if (start_on_construction)
		activate();

	_exit_cap = manage(&_exit_handler);
}


Rpc_entrypoint::~Rpc_entrypoint()
{
	/*
	 * We have to make sure the server loop is running which is only the case
	 * if the Rpc_entrypoint was actived before we execute the RPC call.
	 */
	_delay_start.unlock();

	/* leave server loop */
	_exit_cap.call<Exit::Rpc_exit>();

	dissolve(&_exit_handler);

	if (!empty())
		warning("object pool not empty in ", __func__);

	/*
	 * Now that we finished the 'dissolve' steps above (which need a working
	 * 'Ipc_server' in the context of the entrypoint thread), we can allow the
	 * entrypoint thread to leave the scope. Thereby, the 'Ipc_server' object
	 * will get destructed.
	 */
	_delay_exit.unlock();

	join();
}
