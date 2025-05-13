/*
 * \brief  Service management framework
 * \author Norman Feske
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__SERVICE_H_
#define _INCLUDE__BASE__SERVICE_H_

#include <util/list.h>
#include <pd_session/client.h>
#include <base/env.h>
#include <base/session_state.h>
#include <base/log.h>
#include <base/registry.h>
#include <base/quota_transfer.h>

namespace Genode {

	class Service;
	template <typename> class Session_factory;
	template <typename> class Local_service;
	class Try_parent_service;
	class Parent_service;
	class Async_service;
	class Child_service;
}


class Genode::Service : public Ram_transfer::Account,
                        public Cap_transfer::Account
{
	public:

		using Name = Session_state::Name;

		using Ram_transfer_result = Ram_transfer::Account::Transfer_result;
		using Cap_transfer_result = Cap_transfer::Account::Transfer_result;

	private:

		Name const _name;

	protected:

		using Factory = Session_state::Factory;

		/**
		 * Return factory to use for creating 'Session_state' objects
		 */
		virtual Factory &_factory(Factory &client_factory) { return client_factory; }

	public:

		/**
		 * Constructor
		 *
		 * \param name  service name
		 */
		Service(Name const &name) : _name(name) { }

		virtual ~Service() { }

		/**
		 * Return service name
		 */
		Name const &name() const { return _name; }

		using Create_result = Unique_attempt<Session_state &, Alloc_error>;

		/**
		 * Create new session-state object
		 *
		 * The 'service' argument for the 'Session_state' corresponds to this
		 * session state. All subsequent 'Session_state' arguments correspond
		 * to the forwarded 'args'.
		 */
		Create_result create_session(Factory &client_factory, auto &&... args)
		{
			return _factory(client_factory).create(*this, args...);
		}

		enum class Initiate_error { OUT_OF_CAPS, OUT_OF_RAM };

		using Initiate_result = Attempt<Ok, Initiate_error>;

		/**
		 * Attempt the immediate (synchronous) creation of a session
		 *
		 * Sessions to local services and parent services are usually created
		 * immediately during the dispatching of the 'Parent::session' request.
		 * In these cases, it is not needed to wait for an asynchronous
		 * response.
		 */
		virtual Initiate_result initiate_request(Session_state &session) = 0;

		/**
		 * Wake up service to query session requests
		 */
		virtual void wakeup() { }

		virtual bool operator == (Service const &other) const { return this == &other; }
};


/**
 * Representation of a locally implemented service
 */
template <typename SESSION>
class Genode::Local_service : public Service
{
	public:

		struct Factory : Interface
		{
			using Args   = Session_state::Args;
			using Result = Unique_attempt<SESSION &, Session_error>;

			virtual Result create (Args const &, Affinity)  = 0;
			virtual void   upgrade(SESSION &, Args const &) = 0;
			virtual void   destroy(SESSION &) = 0;
		};

		/**
		 * Factory of a local service that provides a single static session
		 */
		class Single_session_factory : public Factory
		{
			private:

				using Args   = Factory::Args;
				using Result = Factory::Result;

				SESSION &_obj;

			public:

				Single_session_factory(SESSION &obj) : _obj(obj) { }

				Result create (Args const &, Affinity)  override { return { _obj }; }
				void   upgrade(SESSION &, Args const &) override { }
				void   destroy(SESSION &)               override { }
		};

		using Budget_result = Attempt<Session_state::Args, Session_error>;

		static Budget_result budget_adjusted_args(Session_state::Args const &args,
		                                          Allocator &alloc)
		{
			/*
			 * We need to decrease 'ram_quota' by
			 * the size of the session object.
			 */
			Ram_quota const ram_quota = ram_quota_from_args(args.string());

			size_t needed = sizeof(SESSION) + alloc.overhead(sizeof(SESSION));

			if (needed > ram_quota.value)
				return Session_error::INSUFFICIENT_RAM;

			Ram_quota const remaining_ram_quota { ram_quota.value - needed };

			/*
			 * Validate that the client provided the amount of caps as mandated
			 * for the session interface.
			 */
			Cap_quota const cap_quota = cap_quota_from_args(args.string());

			if (cap_quota.value < SESSION::CAP_QUOTA)
				return Session_error::INSUFFICIENT_CAPS;

			/*
			 * Account for the dataspace capability needed for allocating the
			 * session object from the sliced heap.
			 */
			if (cap_quota.value < 1)
				return Session_error::INSUFFICIENT_CAPS;

			Cap_quota const remaining_cap_quota { cap_quota.value - 1 };

			/*
			 * Deduce ram quota needed for allocating the session object from the
			 * donated ram quota.
			 */
			enum { MAX_ARGS_LEN = 256 };
			char adjusted_args[MAX_ARGS_LEN];
			copy_cstring(adjusted_args, args.string(), sizeof(adjusted_args));

			Arg_string::set_arg(adjusted_args, sizeof(adjusted_args),
			                    "ram_quota", String<64>(remaining_ram_quota).string());

			Arg_string::set_arg(adjusted_args, sizeof(adjusted_args),
			                    "cap_quota", String<64>(remaining_cap_quota).string());

			return { adjusted_args };
		}

	private:

		Factory &_factory;

		void _apply_to_rpc_obj(Session_state &session, auto const &fn)
		{
			SESSION *rpc_obj = dynamic_cast<SESSION *>(session.local_ptr);

			if (rpc_obj)
				fn(*rpc_obj);
			else
				warning("local ", SESSION::service_name(), " session "
				        "(", session.args(), ") has no valid RPC object");
		}

	public:

		/**
		 * Constructor
		 */
		Local_service(Factory &factory)
		: Service(SESSION::service_name()), _factory(factory) { }

		Initiate_result initiate_request(Session_state &session) override
		{
			switch (session.phase) {

			case Session_state::CREATE_REQUESTED:

				_factory.create(Session_state::Server_args(session).string(),
				                session.affinity()).with_result(
					[&] (SESSION &obj) {
						session.local_ptr = &obj;
						session.cap       =  obj.cap();
						session.phase     =  Session_state::AVAILABLE;
					},
					[&] (Session_error e) {
						switch (e) {
						case Session_error::DENIED:
							session.phase = Session_state::SERVICE_DENIED; break;
						case Session_error::INSUFFICIENT_RAM:
						case Session_error::OUT_OF_RAM:
							session.phase = Session_state::INSUFFICIENT_RAM_QUOTA; break;
						case Session_error::INSUFFICIENT_CAPS:
						case Session_error::OUT_OF_CAPS:
							session.phase = Session_state::INSUFFICIENT_CAP_QUOTA; break;
						}
					});
				break;

			case Session_state::UPGRADE_REQUESTED:
				{
					String<100> const args("ram_quota=", session.ram_upgrade, ", "
					                       "cap_quota=", session.cap_upgrade);

					_apply_to_rpc_obj(session, [&] (SESSION &rpc_obj) {
						_factory.upgrade(rpc_obj, args.string()); });

					session.phase = Session_state::CAP_HANDED_OUT;
					session.confirm_ram_upgrade();
				}
				break;

			case Session_state::CLOSE_REQUESTED:
				{
					_apply_to_rpc_obj(session, [&] (SESSION &rpc_obj) {
						_factory.destroy(rpc_obj); });

					session.phase = Session_state::CLOSED;
				}
				break;

			case Session_state::SERVICE_DENIED:
			case Session_state::INSUFFICIENT_RAM_QUOTA:
			case Session_state::INSUFFICIENT_CAP_QUOTA:
			case Session_state::AVAILABLE:
			case Session_state::CAP_HANDED_OUT:
			case Session_state::CLOSED:
				break;
			}
			return Ok();
		}
};


/**
 * Representation of a strictly accounted service provided by our parent
 *
 * The 'Try_parent_service' reflects the local depletion of RAM or cap quotas
 * during 'initiate_request' via 'Out_of_ram' or 'Out_of_caps' exceptions.
 * This is appropriate in situations that demand strict accounting of resource
 * use per child, e.g., child components hosted by the init component.
 */
class Genode::Try_parent_service : public Service
{
	private:

		Env &_env;

		using Error = Session_error;

		static Session_state::Phase session_phase_from_error(Error e)
		{
			switch (e) {
			case Error::OUT_OF_RAM:        break;
			case Error::OUT_OF_CAPS:       break;
			case Error::DENIED:            return Session_state::SERVICE_DENIED;
			case Error::INSUFFICIENT_RAM:  return Session_state::INSUFFICIENT_RAM_QUOTA;
			case Error::INSUFFICIENT_CAPS: return Session_state::INSUFFICIENT_CAP_QUOTA;
			}
			return Session_state::CLOSED;
		};

		static Initiate_result result_from_error(Error e)
		{
			switch (e) {
			case Error::OUT_OF_RAM:        return Initiate_error::OUT_OF_RAM;
			case Error::OUT_OF_CAPS:       return Initiate_error::OUT_OF_CAPS;
			case Error::DENIED:            break;
			case Error::INSUFFICIENT_RAM:  break;
			case Error::INSUFFICIENT_CAPS: break;
			}
			return Ok();
		};

	public:

		Try_parent_service(Env &env, Service::Name const &name)
		: Service(name), _env(env) { }

		Initiate_result initiate_request(Session_state &session) override
		{
			Initiate_result result = Ok();

			switch (session.phase) {

			case Session_state::CREATE_REQUESTED:

				session.id_at_parent.construct(session.parent_client,
				                               _env.id_space());

				_env.try_session(name().string(),
				                 session.id_at_parent->id(),
				                 Session_state::Server_args(session).string(),
				                 session.affinity()).with_result(

					[&] (Session_capability cap) {
						session.cap = cap;
						session.phase = Session_state::AVAILABLE;
					},
					[&] (Session_error e) {
						session.id_at_parent.destruct();
						session.phase = session_phase_from_error(e);
						result = result_from_error(e);
					}
				);
				break;

			case Session_state::UPGRADE_REQUESTED:
				{
					String<100> const args("ram_quota=", session.ram_upgrade, ", "
					                       "cap_quota=", session.cap_upgrade);

					if (!session.id_at_parent.constructed())
						error("invalid parent-session state: ", session);

					_env.upgrade(session.id_at_parent->id(), args.string());

					session.confirm_ram_upgrade();
					session.phase = Session_state::CAP_HANDED_OUT;
				}
				break;

			case Session_state::CLOSE_REQUESTED:

				if (session.id_at_parent.constructed())
					_env.close(session.id_at_parent->id());

				session.id_at_parent.destruct();
				session.phase = Session_state::CLOSED;
				break;

			case Session_state::SERVICE_DENIED:
			case Session_state::INSUFFICIENT_RAM_QUOTA:
			case Session_state::INSUFFICIENT_CAP_QUOTA:
			case Session_state::AVAILABLE:
			case Session_state::CAP_HANDED_OUT:
			case Session_state::CLOSED:
				break;
			}
			return result;
		}
};


/**
 * Representation of a service provided by our parent
 *
 * In contrast to 'Try_parent_service', the 'Parent_service' handles the
 * exhaution of the local RAM or cap quotas by issuing resource requests.
 * This is useful in situations where the parent is unconditionally willing
 * to satisfy the resource needs of its children.
 */
class Genode::Parent_service : public Try_parent_service
{
	private:

		Env &_env;

	public:

		Parent_service(Env &env, Service::Name const &name)
		: Try_parent_service(env, name), _env(env) { }

		Initiate_result initiate_request(Session_state &session) override
		{
			Initiate_result result = Ok();

			Session_state::Phase original_phase = session.phase;

			for (unsigned i = 0; i < 10; i++) {

				result = Try_parent_service::initiate_request(session);
				if (result.ok())
					return result;

				if (result == Initiate_error::OUT_OF_RAM) {
					Ram_quota ram_quota { ram_quota_from_args(session.args().string()) };
					Parent::Resource_args args(String<64>("ram_quota=", ram_quota));
					_env.parent().resource_request(args);
					session.phase = original_phase;
				}
				if (result == Initiate_error::OUT_OF_CAPS) {
					Cap_quota cap_quota { cap_quota_from_args(session.args().string()) };
					Parent::Resource_args args(String<64>("cap_quota=", cap_quota));
					_env.parent().resource_request(args);
					session.phase = original_phase;
				}
			}
			error("parent-session request repeatedly failed");
			return result;
		}
};


/**
 * Representation of a service that asynchronously responds to session request
 */
class Genode::Async_service : public Service
{
	public:

		struct Wakeup : Interface { virtual void wakeup_async_service() = 0; };

	private:

		Id_space<Parent::Server> &_server_id_space;

		Session_state::Factory &_server_factory;

		Wakeup &_wakeup;

	protected:

		/*
		 * In contrast to local services and parent services, session-state
		 * objects for child services are owned by the server. This enables
		 * the server to asynchronously respond to close requests when the
		 * client is already gone.
		 */
		Factory &_factory(Factory &) override { return _server_factory; }

	public:

		/**
		 * Constructor
		 *
		 * \param factory  server-side session-state factory
		 * \param name     name of service
		 * \param wakeup   callback to be notified on the arrival of new
		 *                 session requests
		 */
		Async_service(Service::Name      const &name,
		              Id_space<Parent::Server> &server_id_space,
		              Session_state::Factory   &factory,
		              Wakeup                   &wakeup)
		:
			Service(name),
			_server_id_space(server_id_space),
			_server_factory(factory), _wakeup(wakeup)
		{ }

		Initiate_result initiate_request(Session_state &session) override
		{
			if (!session.id_at_server.constructed())
				session.id_at_server.construct(session, _server_id_space);

			session.async_client_notify = true;
			return Ok();
		}

		bool has_id_space(Id_space<Parent::Server> const &id_space) const
		{
			return &_server_id_space == &id_space;
		}

		void wakeup() override { _wakeup.wakeup_async_service(); }
};


/**
 * Representation of a service that is implemented in a child
 */
class Genode::Child_service : public Async_service
{
	private:

		Pd_session_client _pd;

	public:

		/**
		 * Constructor
		 */
		Child_service(Service::Name      const &name,
		              Id_space<Parent::Server> &server_id_space,
		              Session_state::Factory   &factory,
		              Wakeup                   &wakeup,
		              Pd_session_capability,
		              Pd_session_capability     pd)
		:
			Async_service(name, server_id_space, factory, wakeup), _pd(pd)
		{ }

		/**
		 * Ram_transfer::Account interface
		 */
		Ram_transfer_result transfer(Capability<Pd_account> to, Ram_quota amount) override
		{
			return to.valid() ? _pd.transfer_quota(to, amount)
			                  : Ram_transfer_result::OK;
		}

		/**
		 * Ram_transfer::Account interface
		 */
		Capability<Pd_account> cap(Ram_quota) const override { return _pd.rpc_cap(); }

		/**
		 * Cap_transfer::Account interface
		 */
		Cap_transfer_result transfer(Capability<Pd_account> to, Cap_quota amount) override
		{
			return to.valid() ? _pd.transfer_quota(to, amount)
			                  : Cap_transfer_result::OK;
		}

		/**
		 * Cap_transfer::Account interface
		 */
		Capability<Pd_account> cap(Cap_quota) const override { return _pd.rpc_cap(); }
};

#endif /* _INCLUDE__BASE__SERVICE_H_ */
