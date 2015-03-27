/*
 * \brief  NOVA-specific support code for the server-side RPC API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-13
 */

/*
 * Copyright (C) 2010-2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/rpc_server.h>
#include <base/env.h>

/* NOVA includes */
#include <nova/syscalls.h>
#include <nova/util.h>

using namespace Genode;


static Untyped_capability create_portal(Cap_session * cap_session,
                                        Untyped_capability ec_cap,
                                        addr_t entry)
{
	Untyped_capability obj_cap;

	obj_cap = cap_session->alloc(ec_cap, entry);

	if (!obj_cap.valid())
		return obj_cap;

	using namespace Nova;

	/* set local badge */
	if (pt_ctrl(obj_cap.local_name(), obj_cap.local_name()) != NOVA_OK) {
		cap_session->free(obj_cap);
		return Untyped_capability();
	}

	/* disable PT_CTRL permission - feature for security reasons now */
	revoke(Obj_crd(obj_cap.local_name(), 0, Obj_crd::RIGHT_PT_CTRL));

	return obj_cap;
}


/***********************
 ** Server entrypoint **
 ***********************/

Untyped_capability Rpc_entrypoint::_manage(Rpc_object_base *obj)
{
	using namespace Nova;

	Untyped_capability ec_cap, obj_cap;

	/* _ec_sel is invalid until thread gets started */
	if (tid().ec_sel != Native_thread::INVALID_INDEX)
		ec_cap = Native_capability(tid().ec_sel);
	else
		ec_cap = _thread_cap;

	obj_cap = create_portal(_cap_session, ec_cap, (addr_t)_activation_entry);
	if (!obj_cap.valid())
		return obj_cap;

	/* add server object to object pool */
	obj->cap(obj_cap);
	insert(obj);

	/* return object capability managed by entrypoint thread */
	return obj_cap;
}


void Rpc_entrypoint::_dissolve(Rpc_object_base *obj)
{
	/* de-announce object from cap_session */
	_cap_session->free(obj->cap());

	/* avoid any incoming IPC */
	Nova::revoke(Nova::Obj_crd(obj->cap().local_name(), 0), true);

	/* make sure nobody is able to find this object */
	remove_locked(obj);

	/*
	 * The activation may execute a blocking operation in a dispatch function.
	 * Before resolving the corresponding object, we need to ensure that it is
	 * no longer used by an activation. Therefore, we to need cancel an
	 * eventually blocking operation and let the activation leave the context
	 * of the object.
	 */
	_leave_server_object(obj);

	/* wait until nobody is inside dispatch */
	obj->acquire();
}

void Rpc_entrypoint::_activation_entry()
{
	/* retrieve portal id from eax/rdi */
#ifdef __x86_64__
	addr_t id_pt; asm volatile ("" : "=D" (id_pt));
#else
	addr_t id_pt; asm volatile ("" : "=a" (id_pt));
#endif

	Rpc_entrypoint *ep = static_cast<Rpc_entrypoint *>(Thread_base::myself());

	/* delay start if requested so */
	if (ep->_curr_obj) {
		ep->_delay_start.lock();
		ep->_delay_start.unlock();
	}

	/* required to decrease ref count of capability used during last reply */
	ep->_snd_buf.snd_reset();

	/* prepare ipc server object (copying utcb content to message buffer */
	int opcode = 0;

	Ipc_server srv(&ep->_snd_buf, &ep->_rcv_buf);
	srv >> IPC_WAIT >> opcode;

	/* set default return value */
	srv.ret(Ipc_client::ERR_INVALID_OBJECT);

	/* atomically lookup and lock referenced object */
	ep->_curr_obj = ep->lookup_and_lock(id_pt);
	if (!ep->_curr_obj) {

		/*
		 * Badge is used to suppress error message solely.
		 * It's non zero during cleanup call of an
		 * rpc_object_base object, see _leave_server_object.
		 */
		if (!srv.badge())
			PERR("could not look up server object, "
			     " return from call id_pt=%lx",
			     id_pt);

	} else {

		/* dispatch request */
		try { srv.ret(ep->_curr_obj->dispatch(opcode, srv, srv)); }
		catch (Blocking_canceled) { }

		Rpc_object_base * tmp = ep->_curr_obj;
		ep->_curr_obj = 0;

		tmp->release();
	}

	/* if we can't setup receive window, die in order to recognize the issue */
	if (!ep->_rcv_buf.prepare_rcv_window((Nova::Utcb *)ep->utcb()))
		/* printf doesn't work here since for IPC also prepare_rcv_window is used */
		nova_die();

	srv << IPC_REPLY;
}


void Rpc_entrypoint::entry()
{
	/*
	 * Thread entry is not used for activations on NOVA
	 */
}


void Rpc_entrypoint::_leave_server_object(Rpc_object_base *)
{
	using namespace Nova;

	Utcb *utcb = reinterpret_cast<Utcb *>(Thread_base::myself()->utcb());
	/* don't call ourself */
	if (utcb == reinterpret_cast<Utcb *>(this->utcb()))
		return;

	/*
	 * Required outside of core. E.g. launchpad needs it to forcefully kill
	 * a client which blocks on a session opening request where the service
	 * is not up yet.
	 */
	cancel_blocking();

	utcb->msg[0] = 0xdead;
	utcb->set_msg_word(1);
	if (uint8_t res = call(_cap.local_name()))
		PERR("%8p - could not clean up entry point of thread 0x%p - res %u",
		     utcb, this->utcb(), res);
}


void Rpc_entrypoint::_block_until_cap_valid() { }


void Rpc_entrypoint::activate()
{
	/*
	 * In contrast to a normal thread, a server activation is created at
	 * construction time. However, it executes no code because processing
	 * time is always provided by the caller of the server activation. To
	 * delay the processing of requests until the 'activate' function is
	 * called, we grab the '_delay_start' lock on construction and release it
	 * here.
	 */
	_delay_start.unlock();
}


Rpc_entrypoint::Rpc_entrypoint(Cap_session *cap_session, size_t stack_size,
                               const char  *name, bool start_on_construction,
                               Affinity::Location location)
:
	Thread_base(Cpu_session::DEFAULT_WEIGHT, name, stack_size),
	_curr_obj(start_on_construction ? 0 : (Rpc_object_base *)~0UL),
	_delay_start(Lock::LOCKED),
	_cap_session(cap_session)
{
	/* when not running in core set the affinity via cpu session */
	if (_tid.ec_sel == Native_thread::INVALID_INDEX) {

		/* place new thread on the specified CPU */
		if (location.valid())
			_cpu_session->affinity(_thread_cap, location);

		/* magic value evaluated by thread_nova.cc to start a local thread */
		_tid.ec_sel = Native_thread::INVALID_INDEX - 1;
	} else {
		/* tell affinity CPU in 'core' via stack */
		reinterpret_cast<Affinity::Location *>(stack_base())[0] = location;
	}

	/* required to create a 'local' EC */
	Thread_base::start();

	/* create cleanup portal */
	_cap = create_portal(cap_session, Native_capability(_tid.ec_sel),
	                     (addr_t)_activation_entry);
	if (!_cap.valid())
		throw Cpu_session::Thread_creation_failed();

	/* prepare portal receive window of new thread */
	if (!_rcv_buf.prepare_rcv_window((Nova::Utcb *)&_context->utcb))
		throw Cpu_session::Thread_creation_failed();

	if (start_on_construction)
		activate();
}


Rpc_entrypoint::~Rpc_entrypoint()
{
	typedef Object_pool<Rpc_object_base> Pool;

	if (Pool::first()) {
		PWRN("Object pool not empty in %s", __func__);

		/* dissolve all objects - objects are not destroyed! */
		while (Rpc_object_base *obj = Pool::first())
			_dissolve(obj);
	}

	if (!_cap.valid())
		return;

	/* de-announce object from cap_session */
	_cap_session->free(_cap);
}
