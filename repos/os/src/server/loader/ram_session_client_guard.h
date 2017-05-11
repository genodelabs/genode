/*
 * \brief  A guard for RAM session clients to limit memory exhaustion
 * \author Christian Prochaska
 * \date   2012-04-25
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _RAM_SESSION_CLIENT_GUARD_H_
#define _RAM_SESSION_CLIENT_GUARD_H_

#include <base/lock.h>
#include <base/log.h>
#include <base/ram_allocator.h>
#include <pd_session/client.h>
#include <dataspace/client.h>

namespace Genode {

	class Ram_session_client_guard : public Ram_allocator
	{
		private:

			Pd_session_client _pd;
			size_t      const _amount;        /* total amount */
			size_t            _consumed;      /* already consumed bytes */
			Lock      mutable _consumed_lock;

		public:

			Ram_session_client_guard(Pd_session_capability session, Ram_quota amount)
			: _pd(session), _amount(amount.value), _consumed(0) { }

			Ram_dataspace_capability alloc(size_t size, Cache_attribute cached) override
			{
				Lock::Guard _consumed_lock_guard(_consumed_lock);

				if ((_amount - _consumed) < size) {
					warning("quota exceeded! amount=", _amount, ", "
					        "size=", size, ", consumed=", _consumed);
					return Ram_dataspace_capability();
				}

				Ram_dataspace_capability cap = _pd.alloc(size, cached);

				_consumed += size;

				return cap;
			}

			void free(Ram_dataspace_capability ds) override
			{
				Lock::Guard _consumed_lock_guard(_consumed_lock);

				_consumed -= Dataspace_client(ds).size();

				_pd.free(ds);
			}

			size_t dataspace_size(Ram_dataspace_capability ds) const override
			{
				Lock::Guard _consumed_lock_guard(_consumed_lock);

				return _pd.dataspace_size(ds);
			}
	};
}

#endif /* _RAM_SESSION_CLIENT_GUARD_H_ */
