/*
 * \brief  NOVA-specific support code for the server-side RPC API
 * \author Norman Feske
 * \author Sebastian Sumpf
 * \date   2010-01-13
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>
#include <base/rpc_server.h>
#include <base/cap_sel_alloc.h>

/* NOVA includes */
#include <nova/syscalls.h>


using namespace Genode;


/***********************
 ** Server entrypoint **
 ***********************/

Untyped_capability Rpc_entrypoint::_manage(Rpc_object_base *obj)
{
	using namespace Nova;

	int ec_sel = tid().ec_sel;
	int pt_sel = cap_selector_allocator()->alloc();
	int pd_sel = cap_selector_allocator()->pd_sel();

	/* create portal */
	int res = create_pt(pt_sel, pd_sel, ec_sel, Mtd(0),
	                    (mword_t)_activation_entry);

	if (res) {
		PERR("could not create server-object portal, create_pt returned %d\n",
		     res);
		return Untyped_capability();
	}

	/* create capability to portal as destination address */
	Untyped_capability ep_cap = Native_capability(pt_sel, 0);

	/* supplement capability with object ID obtained from CAP session */
	Untyped_capability new_obj_cap = _cap_session->alloc(ep_cap);

	/* add server object to object pool */
	obj->cap(new_obj_cap);
	insert(obj);

	/* return capability that uses the object id as badge */
	return new_obj_cap;
}


void Rpc_entrypoint::_dissolve(Rpc_object_base *obj)
{
	/* make sure nobody is able to find this object */
	remove(obj);

	/*
	 * The activation may execute a blocking operation
	 * in a dispatch function. Before resolving the
	 * corresponding object, we need to ensure that
	 * it is no longer used by an activation. Therefore,
	 * we to need cancel an eventually blocking operation
	 * and let the activation leave the context of the
	 * object.
	 */
	_leave_server_object(obj);

	/* wait until nobody is inside dispatch */
	obj->lock();

	/* now the object may be safely destructed */
}


void Rpc_entrypoint::_activation_entry()
{
	/* retrieve portal id from eax */
	int id_pt; asm volatile ("" : "=a" (id_pt));
	Rpc_entrypoint *ep = static_cast<Rpc_entrypoint *>(Thread_base::myself());

	Ipc_server srv(&ep->_snd_buf, &ep->_rcv_buf);

	/* destination of next reply */
	srv.dst(Native_capability(id_pt, srv.badge()));

	int opcode = 0;

	srv >> IPC_WAIT >> opcode;

	/* set default return value */
	srv.ret(ERR_INVALID_OBJECT);

	/* atomically lookup and lock referenced object */
	{
		Lock::Guard lock_guard(ep->_curr_obj_lock);

		ep->_curr_obj = ep->obj_by_id(srv.badge());
		if (!ep->_curr_obj) {
			PERR("could not look up server object, return from call");
			srv << IPC_REPLY;
		}

		ep->_curr_obj->lock();
	}

	/* dispatch request */
	try { srv.ret(ep->_curr_obj->dispatch(opcode, srv, srv)); }
	catch (Blocking_canceled) { }

	ep->_curr_obj->unlock();
	ep->_curr_obj = 0;

	ep->_rcv_buf.rcv_prepare_pt_sel_window((Nova::Utcb *)ep->utcb());
	srv << IPC_REPLY;
}


void Rpc_entrypoint::entry()
{
	/*
	 * Thread entry is not used for activations on NOVA
	 */
}


void Rpc_entrypoint::_leave_server_object(Rpc_object_base *obj) { }


void Rpc_entrypoint::_block_until_cap_valid() { }


void Rpc_entrypoint::activate()
{
	/*
	 * In contrast to a normal thread, a server activation is created at
	 * construction time. However, it executes no code because processing time
	 * is always provided by the caller of the server activation. To delay the
	 * processing of requests until the 'activate' function is called, we grab the
	 * '_curr_obj_lock' on construction and release it here.
	 */
	_curr_obj_lock.unlock();
}


Rpc_entrypoint::Rpc_entrypoint(Cap_session *cap_session, size_t stack_size,
                               const char  *name, bool start_on_construction)
:
	Thread_base(name, stack_size),
	_curr_obj(0),
	_curr_obj_lock(Lock::LOCKED),
	_cap_session(cap_session)
{
	using namespace Nova;

	/*
	 * Create EC here to ensure that 'tid()' returns a valid 'ec_sel'
	 * portal selector even before 'activate' is called.
	 */

	mword_t *sp   = (mword_t *)&_context->stack[-4];
	mword_t  utcb = (mword_t)  &_context->utcb;

	/* create local EC */
	enum { CPU_NO = 0, GLOBAL = false };
	int res = create_ec(_tid.ec_sel, Cap_selector_allocator::pd_sel(),
	                    CPU_NO, utcb, (mword_t)sp,
	                    _tid.exc_pt_sel, GLOBAL);
	if (res)
		PDBG("create_ec returned %d", res);

	_rcv_buf.rcv_prepare_pt_sel_window((Utcb *)utcb);

	if (start_on_construction)
		activate();
}
