/*
 * \brief  Guarded Genode environment for session components
 * \author Martin Stein
 * \date   2020-11-30
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SESSION_ENV_H_
#define _SESSION_ENV_H_

/* Genode includes */
#include <base/ram_allocator.h>

namespace Genode { class Session_env; }


class Genode::Session_env : public Ram_allocator,
                            public Env::Local_rm
{
	private:

		Env                    &_env;
		Net::Quota             &_shared_quota;
		Ram_quota_guard         _ram_guard;
		Cap_quota_guard         _cap_guard;

		void _consume(size_t      own_ram,
		              size_t      max_shared_ram,
		              size_t      own_cap,
		              size_t      max_shared_cap,
		              auto const &fn)
		{
			size_t const max_ram_consumpt { own_ram + max_shared_ram };
			size_t const max_cap_consumpt { own_cap + max_shared_cap };
			size_t ram_consumpt { _env.pd().used_ram().value };
			size_t cap_consumpt { _env.pd().used_caps().value };

			_ram_guard.reserve(Ram_quota{max_ram_consumpt}).with_result(
				[&] (Ram_quota_guard::Reservation &reserved_ram) {
					_cap_guard.reserve(Cap_quota{max_cap_consumpt}).with_result(
						[&] (Cap_quota_guard::Reservation &reserved_caps) {
							reserved_ram.deallocate  = false;
							reserved_caps.deallocate = false;
							fn();
						},
						[&] (Cap_quota_guard::Error) { throw Out_of_caps(); });
				},
				[&] (Ram_quota_guard::Error) { throw Out_of_ram(); });

			ram_consumpt = _env.pd().used_ram().value  - ram_consumpt;
			cap_consumpt = _env.pd().used_caps().value - cap_consumpt;

			if (ram_consumpt > max_ram_consumpt) {
				error("Session_env: more RAM quota consumed than expected"); }
			if (cap_consumpt > max_cap_consumpt) {
				error("Session_env: more CAP quota consumed than expected"); }
			if (ram_consumpt < own_ram) {
				error("Session_env: less RAM quota consumed than expected"); }
			if (cap_consumpt < own_cap) {
				error("Session_env: less CAP quota consumed than expected"); }

			_shared_quota.ram += ram_consumpt - own_ram;
			_shared_quota.cap += cap_consumpt - own_cap;

			_ram_guard.replenish( Ram_quota { max_shared_ram } );
			_cap_guard.replenish( Cap_quota { max_shared_cap } );
		}

		void _replenish(size_t      accounted_ram,
		                size_t      accounted_cap,
		                auto const &functor)
		{
			size_t ram_replenish { _env.pd().used_ram().value };
			size_t cap_replenish { _env.pd().used_caps().value };
			functor();
			ram_replenish = ram_replenish - _env.pd().used_ram().value;
			cap_replenish = cap_replenish - _env.pd().used_caps().value;

			if (ram_replenish < accounted_ram) {
				error("Session_env: less RAM quota replenished than expected"); }
			if (cap_replenish < accounted_cap) {
				error("Session_env: less CAP quota replenished than expected"); }

			_shared_quota.ram -= ram_replenish - accounted_ram;
			_shared_quota.cap -= cap_replenish - accounted_cap;

			_ram_guard.replenish( Ram_quota { accounted_ram } );
			_cap_guard.replenish( Cap_quota { accounted_cap } );

		}

	public:

		Session_env(Env             &env,
		            Net::Quota      &shared_quota,
		            Ram_quota const &ram_quota,
		            Cap_quota const &cap_quota)
		:
			_env          { env },
			_shared_quota { shared_quota },
			_ram_guard    { ram_quota },
			_cap_guard    { cap_quota }
		{ }

                /*
                 * The Ram_allocator interface is Noncopyable, but this
                 * implementation is safe to copy.
                 */
                Session_env(Session_env const &session)
		:
			_env          { session._env },
			_shared_quota { session._shared_quota },
			_ram_guard    { session._ram_guard },
			_cap_guard    { session._cap_guard }
		{ }

		Entrypoint &ep() { return _env.ep(); }


		/*******************
		 ** Ram_allocator **
		 *******************/

		Alloc_result try_alloc(size_t size, Cache cache) override
		{
			enum { MAX_SHARED_CAP           = 1 };
			enum { MAX_SHARED_RAM           = 4096 };
			enum { DS_SIZE_GRANULARITY_LOG2 = 12 };

			size_t const ds_size = align_addr(size, { DS_SIZE_GRANULARITY_LOG2 });

			Alloc_result result = Alloc_error::DENIED;
			try {
				_consume(ds_size, MAX_SHARED_RAM, 1, MAX_SHARED_CAP, [&]
				{
					result = _env.ram().try_alloc(ds_size, cache);
				});
			}
			catch (Out_of_ram)  { return Alloc_error::OUT_OF_RAM;  }
			catch (Out_of_caps) { return Alloc_error::OUT_OF_CAPS; }

			return result.convert<Alloc_result>(
				[&] (Allocation &a) -> Alloc_result {
					a.deallocate = false;
					return { *this, a };
				},
				[&] (Alloc_error e) { return e; });
		}


		void _free(Allocation &ds) override
		{
			_replenish(_env.pd().ram_size(ds.cap), 1, [&] {
				_env.ram().free(ds.cap); });
		}


		/*******************
		 ** Env::Local_rm **
		 *******************/

		Env::Local_rm::Result attach(Capability<Dataspace> ds, Attach_attr const &attr) override
		{
			using Result = Env::Local_rm::Result;

			enum { MAX_SHARED_CAP = 2 };
			enum { MAX_SHARED_RAM = 4 * 4096 };

			Result result = Env::Local_rm::Error::REGION_CONFLICT;
			try {
				_consume(0, MAX_SHARED_RAM, 0, MAX_SHARED_CAP, [&] {
					result = _env.rm().attach(ds, attr);
				});
			}
			catch (Out_of_ram)  { result = Env::Local_rm::Error::OUT_OF_RAM; }
			catch (Out_of_caps) { result = Env::Local_rm::Error::OUT_OF_CAPS; }

			return result.convert<Result>(
				[&] (Attachment &a) -> Result {
					a.deallocate = false;
					return { *this , a };
				},
				[&] (Env::Local_rm::Error e) { return e; });
		}

		void _free(Attachment &a) override
		{
			_replenish(0, 0, [&] { _env.rm().detach(addr_t(a.ptr)); });
		}

		bool report_empty() const { return false; }

		void report(Genode::Generator &g) const
		{
			g.node("ram-quota", [&] () {
				g.attribute("used",  _ram_guard.used().value);
				g.attribute("limit", _ram_guard.limit().value);
				g.attribute("avail", _ram_guard.avail().value);
			});
			g.node("cap-quota", [&] () {
				g.attribute("used",  _cap_guard.used().value);
				g.attribute("limit", _cap_guard.limit().value);
				g.attribute("avail", _cap_guard.avail().value);
			});
		}


		/***************
		 ** Accessors **
		 ***************/

		Ram_quota_guard const &ram_guard() const { return _ram_guard; };
		Cap_quota_guard const &cap_guard() const { return _cap_guard; };
};

#endif /* _SESSION_ENV_H_ */
