/*
 * \brief  Generic implementation of pager entrypoint
 * \author Norman Feske
 * \author Stefan Kalkowski
 * \date   2009-03-31
 */

/*
 * Copyright (C) 2009-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* core includes */
#include <pager.h>

using namespace Core;


void Pager_entrypoint::entry()
{
	using Pool = Object_pool<Pager_object>;

	bool reply_pending = false;

	while (1) {

		if (reply_pending)
			_pager.reply_and_wait_for_fault();
		else
			_pager.wait_for_fault();

		reply_pending = false;

		Pool::apply(_pager.badge(), [&] (Pager_object *obj) {
			if (obj) {
				if (_pager.exception()) {
					obj->submit_exception_signal();
				} else {
					/* send reply if page-fault handling succeeded */
					using Result = Pager_object::Pager_result;
					reply_pending = (obj->pager(_pager) == Result::CONTINUE);
				}
			} else {

				/*
				 * Prevent threads outside of core to mess with our wake-up
				 * interface. This condition can trigger if a process gets
				 * destroyed which triggered a page fault shortly before getting
				 * killed. In this case, 'wait_for_fault()' returns (because of
				 * the page fault delivery) but the pager-object lookup will fail
				 * (because core removed the process already).
				 */
				if (_pager.request_from_core()) {

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
					obj = reinterpret_cast<Pager_object *>(_pager.fault_ip());

					/* send reply to the calling region-manager session */
					_pager.acknowledge_wakeup();

					/* answer page fault of resolved pager object */
					_pager.set_reply_dst(obj->cap());
					_pager.acknowledge_wakeup();
				}
			}
		});
	}
}


void Pager_entrypoint::dissolve(Pager_object &obj)
{
	using Pool = Object_pool<Pager_object>;

	Pool::remove(&obj);
}


Pager_capability Pager_entrypoint::manage(Pager_object &obj)
{
	Native_capability cap = _pager_object_cap(obj.badge());

	/* add server object to object pool */
	obj.cap(cap);
	insert(&obj);

	/* return capability that uses the object id as badge */
	return reinterpret_cap_cast<Pager_object>(cap);
}
