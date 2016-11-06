/*
 * \brief  Service management framework
 * \author Norman Feske
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SERVICE_H_
#define _INCLUDE__BASE__SERVICE_H_

#include <util/list.h>
#include <ram_session/client.h>
#include <base/env.h>
#include <base/session_state.h>
#include <base/log.h>
#include <base/registry.h>

namespace Genode {

	class Service;
	template <typename> class Session_factory;
	template <typename> class Local_service;
	class Parent_service;
	class Child_service;
}


class Genode::Service : Noncopyable
{
	public:

		typedef Session_state::Name Name;

	private:

		Name                   const _name;
		Ram_session_capability const _ram;

	protected:

		typedef Session_state::Factory Factory;

		/**
		 * Return factory to use for creating 'Session_state' objects
		 */
		virtual Factory &_factory(Factory &client_factory) { return client_factory; }

	public:

		/*********************
		 ** Exception types **
		 *********************/

		class Invalid_args   { };
		class Unavailable    { };
		class Quota_exceeded { };

		/**
		 * Constructor
		 *
		 * \param name  service name
		 * \param ram   RAM session to receive/withdraw session quota
		 */
		Service(Name const &name, Ram_session_capability ram)
		: _name(name), _ram(ram) { }

		virtual ~Service() { }

		/**
		 * Return service name
		 */
		Name const &name() const { return _name; }

		/**
		 * Create new session-state object
		 *
		 * The 'service' argument for the 'Session_state' corresponds to this
		 * session state. All subsequent 'Session_state' arguments correspond
		 * to the forwarded 'args'.
		 */
		template <typename... ARGS>
		Session_state &create_session(Factory &client_factory, ARGS &&... args)
		{
			return _factory(client_factory).create(*this, args...);
		}

		/**
		 * Attempt the immediate (synchronous) creation of a session
		 *
		 * Sessions to local services and parent services are usually created
		 * immediately during the dispatching of the 'Parent::session' request.
		 * In these cases, it is not needed to wait for an asynchronous
		 * response.
		 */
		virtual void initiate_request(Session_state &session) = 0;

		/**
		 * Wake up service to query session requests
		 */
		virtual void wakeup() { }

		/**
		 * Return the RAM session to be used for trading resources
		 */
		Ram_session_capability ram() const
		{
			return _ram;
		}
};


/**
 * Representation of a locally implemented service
 */
template <typename SESSION>
class Genode::Local_service : public Service
{
	public:

		struct Factory
		{
			typedef Session_state::Args Args;

			class Denied : Exception { };

			/**
			 * \throw Denied
			 */
			virtual SESSION &create(Args const &, Affinity)  = 0;

			virtual void upgrade(SESSION &, Args const &) = 0;
			virtual void destroy(SESSION &)               = 0;
		};

		/**
		 * Factory of a local service that provides a single static session
		 */
		class Single_session_factory : public Factory
		{
			private:

				typedef Session_state::Args Args;

				SESSION &_s;

			public:

				Single_session_factory(SESSION &session) : _s(session) { }

				SESSION &create  (Args const &, Affinity)  override { return _s; }
				void     upgrade (SESSION &, Args const &) override { }
				void     destroy (SESSION &)               override { }
		};

	private:

		Factory &_factory;

		template <typename FUNC>
		void _apply_to_rpc_obj(Session_state &session, FUNC const &fn)
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
		:
			Service(SESSION::service_name(), Ram_session_capability()),
			_factory(factory)
		{ }

		void initiate_request(Session_state &session) override
		{
			switch (session.phase) {

			case Session_state::CREATE_REQUESTED:

				try {
					SESSION &rpc_obj = _factory.create(session.args(),
					                                   session.affinity());
					session.local_ptr = &rpc_obj;
					session.cap       = rpc_obj.cap();
					session.phase     = Session_state::AVAILABLE;
				}
				catch (typename Factory::Denied) {
					session.phase = Session_state::INVALID_ARGS; }

				break;

			case Session_state::UPGRADE_REQUESTED:
				{
					String<64> const args("ram_quota=", session.ram_upgrade);

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

			case Session_state::INVALID_ARGS:
			case Session_state::AVAILABLE:
			case Session_state::CAP_HANDED_OUT:
			case Session_state::CLOSED:
				break;
			}
		}
};


/**
 * Representation of a service provided by our parent
 */
class Genode::Parent_service : public Service
{
	private:

		/*
		 * \deprecated
		 */
		Env &_env_deprecated();
		Env &_env;

	public:

		/**
		 * Constructor
		 */
		Parent_service(Env &env, Service::Name const &name)
		: Service(name, Ram_session_capability()), _env(env) { }

		/**
		 * Constructor
		 *
		 * \deprecated
		 */
		Parent_service(Service::Name const &name)
		: Service(name, Ram_session_capability()), _env(_env_deprecated()) { }

		void initiate_request(Session_state &session) override
		{
			switch (session.phase) {

			case Session_state::CREATE_REQUESTED:

				session.id_at_parent.construct(session.parent_client,
				                               _env.id_space());
				try {
					session.cap = _env.session(name().string(),
					                           session.id_at_parent->id(),
					                           session.args().string(),
					                           session.affinity());

					session.phase = Session_state::AVAILABLE;
				}
				catch (Parent::Quota_exceeded) {
					session.id_at_parent.destruct();
					session.phase = Session_state::INVALID_ARGS; }

				catch (Parent::Service_denied) {
					session.id_at_parent.destruct();
					session.phase = Session_state::INVALID_ARGS; }

				break;

			case Session_state::UPGRADE_REQUESTED:
				{
					String<64> const args("ram_quota=", session.ram_upgrade);

					if (!session.id_at_parent.constructed())
						error("invalid parent-session state: ", session);

					try {
						_env.upgrade(session.id_at_parent->id(), args.string()); }
					catch (Parent::Quota_exceeded) {
						warning("quota exceeded while upgrading parent session"); }

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

			case Session_state::INVALID_ARGS:
			case Session_state::AVAILABLE:
			case Session_state::CAP_HANDED_OUT:
			case Session_state::CLOSED:
				break;
			}
		}
};


/**
 * Representation of a service that is implemented in a child
 */
class Genode::Child_service : public Service
{
	public:

		struct Wakeup { virtual void wakeup_child_service() = 0; };

	private:

		Id_space<Parent::Server> &_server_id_space;

		Session_state::Factory &_server_factory;

		Wakeup &_wakeup;

	protected:

		/*
		 * In contrast to local services and parent services, session-state
		 * objects for child services are owned by the server. This enables
		 * the server to asynchronouly respond to close requests when the
		 * client is already gone.
		 */
		Factory &_factory(Factory &) override { return _server_factory; }

	public:


		/**
		 * Constructor
		 *
		 * \param factory  server-side session-state factory
		 * \param name     name of service
		 * \param ram      recipient of session quota
		 * \param wakeup   callback to be notified on the arrival of new
		 *                 session requests
		 *
		 */
		Child_service(Id_space<Parent::Server> &server_id_space,
		              Session_state::Factory   &factory,
		              Service::Name      const &name,
		              Ram_session_capability    ram,
		              Wakeup                   &wakeup)
		:
			Service(name, ram),
			_server_id_space(server_id_space),
			_server_factory(factory), _wakeup(wakeup)
		{ }

		void initiate_request(Session_state &session) override
		{
			if (!session.id_at_server.constructed())
				session.id_at_server.construct(session, _server_id_space);

			session.async_client_notify = true;
		}

		bool has_id_space(Id_space<Parent::Server> const &id_space) const
		{
			return &_server_id_space == &id_space;
		}

		void wakeup() override { _wakeup.wakeup_child_service(); }
};

#endif /* _INCLUDE__BASE__SERVICE_H_ */
