/*
 * \brief  RAM session guard
 * \author Stefan Kalkowski
 * \date   2016-06-03
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__RAM_SESSION_GUARD_H_
#define _INCLUDE__OS__RAM_SESSION_GUARD_H_

#include <dataspace/client.h>
#include <ram_session/ram_session.h>
#include <ram_session/capability.h>

namespace Genode { struct Ram_session_guard; }


class Genode::Ram_session_guard : public Genode::Ram_session
{
	private:

		Ram_session            &_session;
		Ram_session_capability  _session_cap;

		size_t       _quota;
		size_t       _used = 0;
		size_t       _withdraw = 0;

	public:

		Ram_session_guard(Ram_session &session, Ram_session_capability cap,
		                  size_t quota)
		: _session(session), _session_cap(cap), _quota(quota) { }

		/**
		 * Extend allocation limit
		 */
		void upgrade(size_t additional_amount) {
			_quota += additional_amount; }

		/**
		 * Consume bytes without actually allocating them
		 */
		bool withdraw(size_t size)
		{
			if ((_quota - _used) < size)
				return false;

			_used     += size;
			_withdraw += size;
			return true;
		}

		/**
		 * Revert withdraw
		 */
		bool revert_withdraw(size_t size)
		{
			if (size > _withdraw)
				return false;

			_used     -= size;
			_withdraw -= size;

			return true;
		}

		/**
		 * Revert transfer quota
		 */
		int revert_transfer_quota(Ram_session &ram_session,
		                          size_t amount)
		{
			if (amount > _used)
				return -4;

			try {
				ram_session.transfer_quota(_session_cap, Ram_quota{amount});
				_used -= amount;
				return 0;
			} catch (...) { return -1; }
		}

		/***************************
		 ** Ram_session interface **
		 ***************************/


		Ram_dataspace_capability alloc(size_t size,
		                               Cache_attribute cached = CACHED) override
		{
			if (_used + size <= _used || _used + size > _quota)
				throw Quota_exceeded();

			Ram_dataspace_capability cap = _session.alloc(size, cached);

			if (cap.valid())
				_used += size;

			return cap;
		}

		void free(Ram_dataspace_capability ds) override
		{
			size_t size = Dataspace_client(ds).size();
			_session.free(ds);
			_used -= size;
		}

		size_t dataspace_size(Ram_dataspace_capability ds) const override
		{
			return _session.dataspace_size(ds);
		}

		void ref_account(Ram_session_capability ram_session) override {
			_session.ref_account(ram_session); }

		void transfer_quota(Ram_session_capability ram_session,
		                    Ram_quota amount) override
		{
			if (_used + amount.value <= _used || _used + amount.value > _quota)
				throw Out_of_ram();

			_session.transfer_quota(ram_session, amount);
			_used += amount.value;
		}

		Ram_quota ram_quota() const override { return Ram_quota{_quota}; }
		Ram_quota used_ram()  const override { return Ram_quota{_used}; }
};

#endif /* _INCLUDE__OS__RAM_SESSION_GUARD_H_ */
