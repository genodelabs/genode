/*
 * \brief  NOVA-specific support code for the server-side RPC API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \author Alexander Boettcher
 * \date   2010-01-13
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/rpc_server.h>
#include <base/env.h>

/* base-internal includes */
#include <base/internal/stack.h>
#include <base/internal/ipc.h>

/* NOVA includes */
#include <nova/util.h>
#include <nova/native_thread.h>

using namespace Genode;


/***********************
 ** Server entrypoint **
 ***********************/

Untyped_capability Rpc_entrypoint::_manage(Rpc_object_base *obj)
{
	using namespace Nova;

	/* don't manage RPC object twice */
	if (obj->cap().valid()) {
		warning("attempt to manage RPC object twice");
		return obj->cap();
	}

	return with_native_thread(
		[&] (Native_thread &nt) {

			Untyped_capability const ec_cap =
				nt.ec_valid() ? Capability_space::import(nt.ec_sel) : Thread::cap();

			return _alloc_rpc_cap(_pd_session, ec_cap,
			                      addr_t(&_activation_entry)).convert<Untyped_capability>(

				[&] (Untyped_capability const obj_cap) {
					if (!obj_cap.valid())
						return Untyped_capability();

					/* add server object to object pool */
					obj->cap(obj_cap);
					insert(obj);

					/* return object capability managed by entrypoint thread */
					return obj_cap;
				},
				[&] (Alloc_error e) {
					error("unable to allocate RPC cap (", e, ")");
					return Untyped_capability();
				});
		},
		[&] { return Untyped_capability(); });
}


static void cleanup_call(Rpc_object_base *obj, Nova::Utcb * ep_utcb,
                         Native_capability &cap)
{

	/* effectively invalidate the capability used before */
	obj->cap(Untyped_capability());

	/*
	 * The activation may execute a blocking operation in a dispatch function.
	 * Before resolving the corresponding object, we need to ensure that it is
	 * no longer used by an activation. Therefore, we to need cancel an
	 * eventually blocking operation and let the activation leave the context
	 * of the object.
	 */
	using namespace Nova;

	Utcb *utcb = reinterpret_cast<Utcb *>(Thread::myself()->utcb());
	/* don't call ourself */
	if (utcb == ep_utcb)
		return;

	/* make a IPC to ensure that cap() identifier is not used anymore */
	utcb->msg()[0] = 0xdead;
	utcb->set_msg_word(1);
	if (uint8_t res = call(cap.local_name()))
		error(utcb, " - could not clean up entry point of thread ", ep_utcb, " - res ", res);
}

void Rpc_entrypoint::_dissolve(Rpc_object_base *obj)
{
	/* don't dissolve RPC object twice */
	if (!obj || !obj->cap().valid())
		return;

	/* de-announce object from cap_session */
	_free_rpc_cap(_pd_session, obj->cap());

	/* avoid any incoming IPC */
	Nova::revoke(Nova::Obj_crd(obj->cap().local_name(), 0), true);

	/* make sure nobody is able to find this object */
	remove(obj);

	cleanup_call(obj, reinterpret_cast<Nova::Utcb *>(this->utcb()), _cap);
}

static void reply(Nova::Utcb &utcb, Rpc_exception_code exc, Msgbuf_base &snd_msg)
{
	copy_msgbuf_to_utcb(utcb, snd_msg, exc.value);

	Nova::reply((void *)Thread::mystack().top);
}


void Rpc_entrypoint::_activation_entry()
{
	/* retrieve portal id from eax/rdi */
#ifdef __x86_64__
	addr_t id_pt; asm volatile ("" : "=D" (id_pt));
#else
	addr_t id_pt; asm volatile ("" : "=a" (id_pt));
#endif

	Rpc_entrypoint &ep   = *static_cast<Rpc_entrypoint *>(Thread::myself());
	Nova::Utcb     &utcb = *(Nova::Utcb *)Thread::myself()->utcb();

	ep.with_native_thread([&] (Native_thread &nt) {
		Receive_window &rcv_window = nt.server_rcv_window;
		rcv_window.post_ipc(utcb);

		/* handle ill-formed message */
		if (utcb.msg_words() < 2) {
			ep._rcv_buf.word(0) = ~0UL; /* invalid opcode */
		} else {
			copy_utcb_to_msgbuf(utcb, rcv_window, ep._rcv_buf);
		}

		Ipc_unmarshaller unmarshaller(ep._rcv_buf);

		Rpc_opcode opcode(0);
		unmarshaller.extract(opcode);

		/* default return value */
		Rpc_exception_code exc = Rpc_exception_code(Rpc_exception_code::INVALID_OBJECT);

		/* in case of a portal cleanup call we are done here - just reply */
		if (ep._cap.local_name() == (long)id_pt) {
			if (!rcv_window.prepare_rcv_window(utcb))
				warning("out of capability selectors for handling server requests");

			ep._rcv_buf.reset();
			reply(utcb, exc, ep._snd_buf);
		}

		/* atomically lookup and lock referenced object */
		auto lambda = [&] (Rpc_object_base *obj)
		{
			if (!obj) {
				error("could not look up server object, return from call id_pt=", id_pt);
				return;
			}

			/* dispatch request */
			ep._snd_buf.reset();
			exc = obj->dispatch(opcode, unmarshaller, ep._snd_buf);
		};
		ep.apply(id_pt, lambda);

		if (!rcv_window.prepare_rcv_window(*(Nova::Utcb *)ep.utcb()))
			warning("out of capability selectors for handling server requests");

		ep._rcv_buf.reset();
		reply(utcb, exc, ep._snd_buf);
	});
}


void Rpc_entrypoint::entry()
{
	/*
	 * Thread entry is not used for activations on NOVA
	 */
}


void Rpc_entrypoint::_block_until_cap_valid() { }


bool Rpc_entrypoint::is_myself() const
{
	return (Thread::myself() == this);
}


Rpc_entrypoint::Rpc_entrypoint(Pd_session *pd_session, size_t stack_size,
                               const char  *name, Affinity::Location location)
:
	Thread(Cpu_session::Weight::DEFAULT_WEIGHT, name, stack_size, location),
	_pd_session(*pd_session)
{
	/* set magic value evaluated by thread_nova.cc to start a local thread */
	with_native_thread([&] (Native_thread &nt) {
		if (nt.ec_valid())
			return;

		nt.ec_sel     = Native_thread::INVALID_INDEX - 1;
		nt.initial_ip = (addr_t)&_activation_entry;
	});

	/* required to create a 'local' EC */
	Thread::start();

	_stack.with_result(
		[&] (Stack &stack) {

			/* create cleanup portal */
			_cap = _alloc_rpc_cap(_pd_session,
			                      Capability_space::import(stack.native_thread().ec_sel),
			                      addr_t(_activation_entry)).convert<Untyped_capability>(
				[&] (Untyped_capability cap) { return cap; },
				[&] (Alloc_error e) {
					error("failed to allocate RPC cap for new entrypoint (", e, ")");
					return Untyped_capability();
				});

			Receive_window &rcv_window = stack.native_thread().server_rcv_window;

			/* prepare portal receive window of new thread */
			if (!rcv_window.prepare_rcv_window((Nova::Utcb &)stack.utcb()))
				error("failed to prepare receive window for RPC entrypoint");
		},
		[&] (Stack_error) { }
	);
}


Rpc_entrypoint::~Rpc_entrypoint()
{
	using Pool = Object_pool<Rpc_object_base>;

	Pool::remove_all([&] (Rpc_object_base *obj) {
		warning("object pool not empty in ", __func__);

		/* don't dissolve RPC object twice */
		if (!obj || !obj->cap().valid())
			return;

		/* de-announce object from cap_session */
		_free_rpc_cap(_pd_session, obj->cap());

		/* avoid any incoming IPC */
		Nova::revoke(Nova::Obj_crd(obj->cap().local_name(), 0), true);

		cleanup_call(obj, reinterpret_cast<Nova::Utcb *>(this->utcb()), _cap);
	});

	if (!_cap.valid())
		return;

	_free_rpc_cap(_pd_session, _cap);
}
