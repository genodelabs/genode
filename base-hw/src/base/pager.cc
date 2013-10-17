/*
 * \brief  Pager implementations that are specific for the HW-core
 * \author Martin Stein
 * \date   2012-03-29
 */

/*
 * Copyright (C) 2012-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/pager.h>
#include <base/printf.h>

using namespace Genode;


/***************************
 ** Pager_activation_base **
 ***************************/

void Pager_activation_base::entry()
{
	/* acknowledge that we're ready to work */
	Ipc_pager pager;
	_cap = pager;
	_cap_valid.unlock();

	/* receive and handle faults */
	bool mapping_pending = 0;
	pager.wait_for_first_fault();
	while (1)
	{
		/* protect bottom half of loop against pager-object guard */
		{
			/* lookup pager object for current faulter */
			Object_pool<Pager_object>::Guard
				o(_ep ? _ep->lookup_and_lock(pager.badge()) : 0);

			if (!o) {
				PERR("invalid pager object");
				mapping_pending = 0;
			} else {
				/* try to find an appropriate mapping */
				mapping_pending = !o->pager(pager);
			}
		}
		if (mapping_pending) {
			/* apply mapping and await next fault */
			if (pager.resolve_and_wait_for_fault()) {
				PERR("failed to resolve page fault");
				pager.wait_for_fault();
			}
		} else { pager.wait_for_fault(); }
	}
}


/**********************
 ** Pager_entrypoint **
 **********************/

Pager_entrypoint::
Pager_entrypoint(Cap_session *, Pager_activation_base * const a)
: _activation(a) { _activation->ep(this); }


void Pager_entrypoint::dissolve(Pager_object * const o) { remove_locked(o); }


Pager_capability Pager_entrypoint::manage(Pager_object * const o)
{
	/* do we have an activation */
	if (!_activation) return Pager_capability();

	/* create cap with the object badge as local name */
	Native_capability c;
	c = Native_capability(_activation->cap().dst(), o->badge());

	/* let activation provide the pager object */
	o->cap(c);
	insert(o);

	/* return pager-object cap */
	return reinterpret_cap_cast<Pager_object>(c);
}


/***************
 ** Ipc_pager **
 ***************/

void Ipc_pager::wait_for_first_fault()
{
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	while (1) {
		Kernel::wait_for_request();
		if (utcb->msg.type == Msg_type::IPC) { break; }
		PERR("failed to receive fault");
	}
	_wait_for_fault(utcb->ipc_msg.size);
}


void Ipc_pager::wait_for_fault()
{
	Native_utcb * const utcb = Thread_base::myself()->utcb();
	utcb->ipc_msg.size = 0;
	Kernel::reply(1);
	while (utcb->msg.type != Msg_type::IPC) {
		PERR("failed to receive fault");
		Kernel::wait_for_request();
	}
	_wait_for_fault(utcb->ipc_msg.size);
}


void Ipc_pager::_wait_for_fault(size_t s)
{
	while (1)
	{
		/*
		 * FIXME: the message size is a weak indicator for the message type
		 */
		switch (s)
		{
		case sizeof(Pagefault): {

			/* message is a pagefault */
			Native_utcb * const utcb = Thread_base::myself()->utcb();
			Pagefault * const pf = (Pagefault *)utcb->ipc_msg.data;
			if (pf->valid())
			{
				/* give our caller the chance to handle the fault */
				_pagefault = *pf;
				return;
			}
			/* pagefault is invalid so get the next message */
			else {
				PERR("invalid pagefault");
				continue;
			}
			continue; }

		case sizeof(Pagefault_resolved): {

			/* message is a release request from a RM session */
			Native_utcb * const utcb = Thread_base::myself()->utcb();
			void * const msg_base = utcb->ipc_msg.data;
			Pagefault_resolved * const msg = (Pagefault_resolved *)msg_base;

			/* resume faulter, send ack to RM and get the next message */
			Kernel::resume_thread(msg->pager_object->badge());
			utcb->ipc_msg.size = 0;
			Kernel::reply(1);
			while (utcb->msg.type != Msg_type::IPC) {
				PERR("failed to receive fault");
				Kernel::wait_for_request();
			}
			s = utcb->ipc_msg.size;
			continue; }

		default: {

			PERR("invalid message format");
			continue; }
		}
	}
}
