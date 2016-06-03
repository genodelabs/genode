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

#ifndef _RAM_SESSION_GUARD_H_
#define _RAM_SESSION_GUARD_H_

#include <dataspace/client.h>
#include <ram_session/ram_session.h>
#include <ram_session/capability.h>

namespace Genode { struct Ram_session_guard; }


class Genode::Ram_session_guard : public Genode::Ram_session
{
	private:

		Ram_session &_session;
		size_t       _quota;
		size_t       _used;

	public:

		Ram_session_guard(Ram_session &session, size_t quota)
		: _session(session) { }

		Ram_dataspace_capability alloc(size_t size,
		                               Cache_attribute cached = CACHED) override
		{
			if (_used + size > _used) throw Quota_exceeded();
			_used += size;
			return _session.alloc(size, cached);
		}

		void free(Ram_dataspace_capability ds) override
		{
			size_t size = Dataspace_client(ds).size();
			_used -= size;
			_session.free(ds);
		}

		int ref_account(Ram_session_capability ram_session) override {
			return -1; }

		int transfer_quota(Ram_session_capability ram_session, size_t amount) override {
			return -1; }

		size_t quota() override { return _quota; }

		size_t used() override { return _used; }
};

#endif /* _RAM_SESSION_GUARD_H_ */
