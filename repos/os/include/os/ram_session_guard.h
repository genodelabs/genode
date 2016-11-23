/*
 * \brief  RAM session guard
 * \author Stefan Kalkowski
 * \date   2016-06-03
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
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

		/* XXX should be either a exception or a enum in rm_session */
		enum { RM_SESSION_INSUFFICIENT_QUOTA = -3 };

	public:

		Ram_session_guard(Ram_session &session, Ram_session_capability cap,
		                  size_t quota)
		: _session(session), _session_cap(cap), _quota(quota) { }

		/**
		 * Convenient transfer_quota method throwing a exception iif the
		 * quota is insufficient.
		 */
		template <typename T>
		int transfer_quota(Ram_session_capability ram_session, size_t amount)
		{
			int const error = transfer_quota(ram_session, amount);

			if (error == RM_SESSION_INSUFFICIENT_QUOTA)
				throw T();

			return error;
		}

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

			int error = ram_session.transfer_quota(_session_cap, amount);
			if (!error)
				_used -= amount;

			return error;
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

		int ref_account(Ram_session_capability ram_session) override {
			return _session.ref_account(ram_session); }

		int transfer_quota(Ram_session_capability ram_session,
		                   size_t amount) override
		{
			if (_used + amount <= _used || _used + amount > _quota)
				return RM_SESSION_INSUFFICIENT_QUOTA;

			int const error = _session.transfer_quota(ram_session, amount);

			if (!error)
				_used += amount;

			return error;
		}

		size_t quota() override { return _quota; }

		size_t used() override { return _used; }
};

#endif /* _INCLUDE__OS__RAM_SESSION_GUARD_H_ */
