/*
 * \brief  Signal-source server interface
 * \author Norman Feske
 * \date   2010-02-03
 *
 * This file is only included by 'signal_session/server.h' and relies on the
 * headers included there. No include guards are needed. It is a separate
 * header file to make it easily replaceable by a platform-specific
 * implementation.
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__SIGNAL_SESSION__SOURCE_SERVER_H_
#define _INCLUDE__SIGNAL_SESSION__SOURCE_SERVER_H_

#include <base/rpc_server.h>
#include <signal_session/nova_source.h>

namespace Genode { struct Signal_source_rpc_object; }

struct Genode::Signal_source_rpc_object : Rpc_object<Nova_signal_source,
                                                     Signal_source_rpc_object>
{
	private:

		Native_capability _blocking_semaphore;
		bool              _missed_wakeup;

	protected:

		void _wakeup_client()
		{
			if (!_blocking_semaphore.valid()) {
				_missed_wakeup = true;
				return;
			}

			if (_missed_wakeup)
				_missed_wakeup = false;

			/* wake up client */
			uint8_t res = Nova::sm_ctrl(_blocking_semaphore.local_name(),
			                            Nova::SEMAPHORE_UP);
			if (res != Nova::NOVA_OK) {
				PWRN("%s - signal delivery failed - error %x",
				     __func__, res);
				_missed_wakeup = true;
			}
		}

	public:

		void _register_semaphore(Native_capability const &cap)
		{
			if (_blocking_semaphore.valid())
				PWRN("overwritting blocking signal semaphore !!!");

			_blocking_semaphore = cap;

			if (_missed_wakeup)
				_wakeup_client();
		}

		Signal_source_rpc_object() : _missed_wakeup(false) {}
};

#endif /* _INCLUDE__SIGNAL_SESSION__SOURCE_SERVER_H_ */
