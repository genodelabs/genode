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
#include <base/printf.h>
#include <dataspace/client.h>
#include <ram_session/client.h>

namespace Genode {

	class Ram_session_client_guard : public Ram_session_client
	{
		private:

			size_t _amount;        /* total amount */
			size_t _consumed;      /* already consumed bytes */
			Lock   _consumed_lock;

		public:

			Ram_session_client_guard(Ram_session_capability session, size_t amount)
			: Ram_session_client(session), _amount(amount), _consumed(0) { }

			Ram_dataspace_capability alloc(size_t size, Cache_attribute cached)
			{
				Lock::Guard _consumed_lock_guard(_consumed_lock);

				if ((_amount - _consumed) < size) {
					PWRN("Quota exceeded! amount=%lu, size=%lu, consumed=%lu",
					     _amount, size, _consumed);
					return Ram_dataspace_capability();
				}

				Ram_dataspace_capability cap =
					Ram_session_client::alloc(size, cached);

				_consumed += size;

				return cap;
			}

			void free(Ram_dataspace_capability ds)
			{
				Lock::Guard _consumed_lock_guard(_consumed_lock);

				_consumed -= Dataspace_client(ds).size();

				Ram_session_client::free(ds);
			}

			int transfer_quota(Ram_session_capability ram_session, size_t amount)
			{
				Lock::Guard _consumed_lock_guard(_consumed_lock);

				if ((_amount - _consumed) < amount) {
					PWRN("Quota exceeded! amount=%lu, size=%lu, consumed=%lu",
					     _amount, amount, _consumed);
					return -1;
				}

				int result = Ram_session_client::transfer_quota(ram_session, amount);

				if (result == 0)
					_consumed += amount;

				return result;
			}

			size_t quota()
			{
				return _amount;
			}

			size_t used()
			{
				Lock::Guard _consumed_lock_guard(_consumed_lock);
				return _consumed;
			}
	};
}

#endif /* _RAM_SESSION_CLIENT_GUARD_H_ */
