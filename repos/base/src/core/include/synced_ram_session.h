/*
 * \brief  Synchronized wrapper for the 'Ram_session' interface
 * \author Norman Feske
 * \date   2017-05-11
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _CORE__INCLUDE__SYNCED_RAM_SESSION_H_
#define _CORE__INCLUDE__SYNCED_RAM_SESSION_H_

/* Genode includes */
#include <ram_session/ram_session.h>
#include <base/lock.h>

namespace Genode { class Synced_ram_session; }


class Genode::Synced_ram_session : public Ram_session
{
	private:

		Lock mutable _lock;
		Ram_session &_ram_session;

	public:

		Synced_ram_session(Ram_session &ram_session) : _ram_session(ram_session) { }

		Ram_dataspace_capability alloc(size_t size, Cache_attribute cached) override
		{
			Lock::Guard lock_guard(_lock);
			return _ram_session.alloc(size, cached);
		}

		void free(Ram_dataspace_capability ds) override
		{
			Lock::Guard lock_guard(_lock);
			_ram_session.free(ds);
		}

		size_t dataspace_size(Ram_dataspace_capability ds) const override
		{
			Lock::Guard lock_guard(_lock);
			return _ram_session.dataspace_size(ds);
		}

		void ref_account(Ram_session_capability session) override
		{
			Lock::Guard lock_guard(_lock);
			_ram_session.ref_account(session);
		}

		void transfer_quota(Ram_session_capability session, Ram_quota amount) override
		{
			Lock::Guard lock_guard(_lock);
			_ram_session.transfer_quota(session, amount);
		}

		Ram_quota ram_quota() const override
		{
			Lock::Guard lock_guard(_lock);
			return _ram_session.ram_quota();
		}

		Ram_quota used_ram() const override
		{
			Lock::Guard lock_guard(_lock);
			return _ram_session.used_ram();
		}
};

#endif /* _CORE__INCLUDE__SYNCED_RAM_SESSION_H_ */
