/*
 * \brief  Pistachio pager framework
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-14
 *
 * FIXME Isn't this file generic?
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/pager.h>

using namespace Genode;


namespace Pistachio {
#include <l4/thread.h>
}

/**********************
 ** Pager activation **
 **********************/

void Pager_activation_base::entry()
{
	Ipc_pager pager;
	_cap = pager;
	_cap_valid.unlock();

	Pager_object * obj;
	bool reply = false;

	while (1) {

		if (reply)
			pager.reply_and_wait_for_fault();
		else
			pager.wait_for_fault();

		/* lookup referenced object */
		Object_pool<Pager_object>::Guard _obj(_ep ? _ep->lookup_and_lock(pager.badge()) : 0);
		obj   = _obj;
		reply = false;

		/* handle request */
		if (obj) {
			/* if something strange occurred - leave thread in pagefault */
			reply = !obj->pager(pager);
			continue;
		} else {

			/* prevent threads outside of core to mess with our wake-up interface */
//			enum { CORE_TASK_ID = 4 };
//			if (pager.last().id.task != CORE_TASK_ID) {

#warning Check for messages from outside of core
			if (0) {
				PWRN("page fault to 0x%08lx from unknown partner %lx.",
				 Pistachio::L4_Myself().raw,
				 pager.last().raw);

			} else {

				/*
				 * We got a request from one of cores region-manager sessions
				 * to answer the pending page fault of a resolved region-manager
				 * client. Hence, we have to send the page-fault reply to the
				 * specified thread and answer the call of the region-manager
				 * session.
				 *
				 * When called from a region-manager session, we receive the
				 * core-local address of the targeted pager object via the
				 * first message word, which corresponds to the 'fault_ip'
				 * argument of normal page-fault messages.
				 */
				obj = reinterpret_cast<Pager_object *>(pager.fault_ip());

				/* send reply to the calling region-manager session */
				pager.acknowledge_wakeup();

				/* answer page fault of resolved pager object */
				pager.set_reply_dst(obj->cap());
				pager.acknowledge_wakeup();
			}
		}
	}
}


/**********************
 ** Pager entrypoint **
 **********************/

Pager_entrypoint::Pager_entrypoint(Cap_session *, Pager_activation_base *a)
: _activation(a)
{ _activation->ep(this); }


void Pager_entrypoint::dissolve(Pager_object *obj)
{
	remove_locked(obj);
}


Pager_capability Pager_entrypoint::manage(Pager_object *obj)
{
	/* return invalid capability if no activation is present */
	if (!_activation) return Pager_capability();

	Native_capability cap = Native_capability(_activation->cap().dst(), obj->badge());

	/* add server object to object pool */
	obj->cap(cap);
	insert(obj);

	/* return capability that uses the object id as badge */
	return reinterpret_cap_cast<Pager_object>(cap);
}
