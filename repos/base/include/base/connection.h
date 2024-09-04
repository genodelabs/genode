/*
 * \brief  Connection to a service
 * \author Norman Feske
 * \date   2008-08-22
 */

/*
 * Copyright (C) 2008-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__CONNECTION_H_
#define _INCLUDE__BASE__CONNECTION_H_

#include <util/retry.h>
#include <base/env.h>
#include <base/capability.h>
#include <base/log.h>

namespace Genode {

	class Connection_base;
	template <typename> class Connection;
}


class Genode::Connection_base : Noncopyable, Interface
{
	protected:

		Env &_env;

		Parent::Client _parent_client { };

		Id_space<Parent::Client>::Element const _id_space_element;

		void _block_for_session_response();

	public:

		Connection_base(Env &env)
		:
			_env(env),
			_id_space_element(_parent_client, _env.id_space())
		{ }

		void upgrade(Session::Resources resources)
		{
			String<80> const args("ram_quota=", resources.ram_quota, ", "
			                      "cap_quota=", resources.cap_quota);
			_env.upgrade(_id_space_element.id(), args.string());
		}

		void upgrade_ram(size_t bytes)
		{
			upgrade(Session::Resources { Ram_quota{bytes}, Cap_quota{0} });
		}

		void upgrade_caps(size_t caps)
		{
			upgrade(Session::Resources { Ram_quota{0}, Cap_quota{caps} });
		}

		/**
		 * Extend session quota on demand while calling an RPC function
		 *
		 * \param ram   amount of RAM to upgrade as response to 'Out_of_ram'
		 * \param caps  amount of caps to upgrade as response to 'Out_of_caps'
		 *
		 * \noapi
		 */
		auto retry_with_upgrade(Ram_quota ram, Cap_quota caps, auto const &fn) -> decltype(fn())
		{
			enum { UPGRADE_ATTEMPTS = ~0U };
			return Genode::retry<Out_of_ram>(
				[&] {
					return Genode::retry<Out_of_caps>(
						[&] { return fn(); },
						[&] { upgrade_caps(caps.value); },
						UPGRADE_ATTEMPTS);
				},
				[&] { upgrade_ram(ram.value); },
				UPGRADE_ATTEMPTS);
		}
};


/**
 * Representation of an open connection to a service
 */
template <typename SESSION_TYPE>
class Genode::Connection : public Connection_base
{
	public:

		using Args         = String<Parent::Session_args::MAX_SIZE>;
		using Session_type = SESSION_TYPE;
		using Ram_quota    = Genode::Ram_quota;

	private:

		using Client_id = Parent::Client::Id;

		static Capability<SESSION_TYPE> _request(Env                 &env,
		                                         Client_id     const &id,
		                                         Session_label const &label,
		                                         Ram_quota     const &ram_quota,
		                                         Cap_quota     const &cap_quota,
		                                         Affinity      const &affinity,
		                                         Args          const &args)
		{
			/* supplement session quotas and label as session arguments */
			Args const complete_args("label=\"",   label, "\", "
			                         "ram_quota=", ram_quota, ", "
			                         "cap_quota=", cap_quota, ", ",
			                         args);

			if (complete_args.length() == Args::capacity())
				warning("truncated arguments of ",
				        SESSION_TYPE::service_name(), " session");

			try {
				return env.session<SESSION_TYPE>(id, complete_args.string(),
				                                 affinity);
			}
			catch (...) {
				error(SESSION_TYPE::service_name(), "-session creation failed "
				      "(", complete_args, ")");
				throw;
			}
		}

		Capability<SESSION_TYPE> _cap;

	public:

		Connection(Env                 &env,
		           Session_label const &label,
		           Ram_quota     const &ram_quota,
		           Cap_quota     const &cap_quota,
		           Affinity      const &affinity,
		           Args          const &args)
		:
			Connection_base(env),
			_cap(_request(env, _id_space_element.id(),
			              label, ram_quota, cap_quota, affinity, args))
		{ }

		/**
		 * Constructor using the cap quota declared in as SESSION_TYPE::CAP_QUOTA
		 */
		Connection(Env                 &env,
		           Session_label const &label,
		           Ram_quota     const &ram_quota,
		           Affinity      const &affinity,
		           Args          const &args)
		:
			Connection(env, label, ram_quota, Cap_quota { unsigned(SESSION_TYPE::CAP_QUOTA) },
			           affinity, args)
		{ }

		/**
		 * Constructor
		 *
		 * Shortcut for the common case where the affinity is not specified.
		 */
		Connection(Env &env, Session_label const &label,
		           Ram_quota const &ram_quota, Args const &args)
		:
			Connection(env, label, ram_quota, Affinity(), args)
		{ }

		~Connection() { _env.close(_id_space_element.id()); }

		/**
		 * Return session capability
		 */
		Capability<SESSION_TYPE> cap() const { return _cap; }
};

#endif /* _INCLUDE__BASE__CONNECTION_H_ */
