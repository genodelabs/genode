/*
 * \brief  Generic root component implementation
 * \author Norman Feske
 * \date   2006-05-22
 *
 * This class is there for your convenience. It performs the common actions
 * that must always be taken when creating a new session.
 */

/*
 * Copyright (C) 2006-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__ROOT__COMPONENT_H_
#define _INCLUDE__ROOT__COMPONENT_H_

#include <root/root.h>
#include <base/allocator.h>
#include <base/rpc_server.h>
#include <base/entrypoint.h>
#include <base/service.h>
#include <util/arg_string.h>
#include <base/log.h>

namespace Genode {

	class Single_client;
	class Multiple_clients;
	template <typename, typename POLICY = Multiple_clients> class Root_component;
}


/**
 * Session creation policy for a single-client service
 */
class Genode::Single_client
{
	private:

		bool _used;

	public:

		Single_client() : _used(0) { }

		void aquire(const char *)
		{
			if (_used)
				throw Service_denied();

			_used = true;
		}

		void release() { _used = false; }
};


/**
 * Session-creation policy for a multi-client service
 */
struct Genode::Multiple_clients
{
	void aquire(const char *) { }
	void release() { }
};


/**
 * Template for implementing the root interface
 *
 * \param SESSION_TYPE  session-component type to manage,
 *                      derived from 'Rpc_object'
 * \param POLICY        session-creation policy
 *
 * The 'POLICY' template parameter allows for constraining the session
 * creation to only one instance at a time (using the 'Single_session'
 * policy) or multiple instances (using the 'Multiple_sessions' policy).
 *
 * The 'POLICY' class must provide the following two methods:
 *
 * 'aquire(const char *args)' is called with the session arguments
 * at creation time of each new session. It can therefore implement
 * a session-creation policy taking session arguments into account.
 * If the policy denies the creation of a new session, it throws
 * one of the exceptions defined in the 'Root' interface.
 *
 * 'release' is called at the destruction time of a session. It enables
 * the policy to keep track of and impose restrictions on the number
 * of existing sessions.
 *
 * The default policy 'Multiple_clients' imposes no restrictions on the
 * creation of new sessions.
 */
template <typename SESSION_TYPE, typename POLICY>
class Genode::Root_component : public Rpc_object<Typed_root<SESSION_TYPE> >,
                               public Local_service<SESSION_TYPE>::Factory,
                               private POLICY
{
	private:

		/*
		 * Entry point that manages the session objects created by this root
		 * interface
		 */
		Rpc_entrypoint &_ep;

		/*
		 * Allocator for allocating session objects. This allocator must be
		 * used by the derived class when calling the 'new' operator for
		 * creating a new session.
		 */
		Allocator &_md_alloc;

		using Local_service = Genode::Local_service<SESSION_TYPE>;
		using Create_error  = Service::Create_error;
		using Create_result = Local_service::Factory::Result;

		/*
		 * Used by both the legacy 'Root::session' and the new 'Factory::create'
		 */
		Create_result _create(Session_state::Args const &args, Affinity affinity)
		{
			POLICY::aquire(args.string());

			/*
			 * Guard to ensure that 'release' is called whenever the scope
			 * is left with an exception.
			 */
			struct Guard
			{
				bool ack = false;
				Root_component &root;
				Guard(Root_component &root) : root(root) { }
				~Guard() { if (!ack) root.release(); }
			} aquire_guard { *this };

			try {
				SESSION_TYPE &s =
					*_create_session(args.string(), affinity);

				/*
				 * Consider that the session-object constructor may already
				 * have called 'manage'.
				 */
				if (!s.cap().valid())
					_ep.manage(&s);

				aquire_guard.ack = true;
				return s;
			}
			catch (Out_of_ram)             { return Create_error::INSUFFICIENT_RAM; }
			catch (Out_of_caps)            { return Create_error::INSUFFICIENT_CAPS; }
			catch (Service_denied)         { return Create_error::DENIED; }
			catch (Insufficient_cap_quota) { return Create_error::INSUFFICIENT_CAPS; }
			catch (Insufficient_ram_quota) { return Create_error::INSUFFICIENT_RAM; }
			catch (...) {
				warning("unexpected exception during ",
				        SESSION_TYPE::service_name(), "-session creation");
			}
			return Create_error::DENIED;
		}

		/*
		 * Noncopyable
		 */
		Root_component(Root_component const &);
		Root_component &operator = (Root_component const &);

	protected:

		/**
		 * Create new session (to be implemented by a derived class)
		 *
		 * Only a derived class knows the constructor arguments of
		 * a specific session. Therefore, we cannot unify the call
		 * of its 'new' operator and must implement the session
		 * creation at a place, where the required knowledge exist.
		 *
		 * In the implementation of this method, the heap, provided
		 * by 'Root_component' must be used for allocating the session
		 * object.
		 *
		 * If the server implementation does not evaluate the session
		 * affinity, it suffices to override the overload without the
		 * affinity argument.
		 *
		 * \throw Out_of_ram 
		 * \throw Out_of_caps
		 * \throw Service_denied
		 * \throw Insufficient_cap_quota
		 * \throw Insufficient_ram_quota
		 */
		virtual SESSION_TYPE *_create_session(const char *args,
		                                      Affinity const &)
		{
			return _create_session(args);
		}

		virtual SESSION_TYPE *_create_session(const char *)
		{
			throw Service_denied();
		}

		/**
		 * Inform session about a quota upgrade
		 *
		 * Once a session is created, its client can successively extend
		 * its quota donation via the 'Parent::transfer_quota' operation.
		 * This will result in the invokation of 'Root::upgrade' at the
		 * root interface the session was created with. The root interface,
		 * in turn, informs the session about the new resources via the
		 * '_upgrade_session' method. The default implementation is
		 * suited for sessions that use a static amount of resources
		 * accounted for at session-creation time. For such sessions, an
		 * upgrade is not useful. However, sessions that dynamically
		 * allocate resources on behalf of its client, should respond to
		 * quota upgrades by implementing this method.
		 *
		 * \param session  session to upgrade
		 * \param args     description of additional resources in the
		 *                 same format as used at session creation
		 */
		virtual void _upgrade_session(SESSION_TYPE *, const char *) { }

		virtual void _destroy_session(SESSION_TYPE *session) {
			Genode::destroy(_md_alloc, session); }

		/**
		 * Return allocator to allocate server object in '_create_session()'
		 */
		Allocator *md_alloc() { return &_md_alloc; }

		/**
		 * Return entrypoint that serves the root component
		 */
		Rpc_entrypoint *ep() { return &_ep; }

	public:

		/**
		 * Constructor
		 *
		 * \param ep        entry point that manages the sessions of this
		 *                  root interface
		 * \param md_alloc  meta-data allocator providing the backing store
		 *                  for session objects
		 */
		Root_component(Entrypoint &ep, Allocator &md_alloc)
		:
			_ep(ep.rpc_ep()), _md_alloc(md_alloc)
		{ }

		/**
		 * Constructor
		 *
		 * \deprecated  use the constructor with the 'Entrypoint &'
		 *              argument instead
		 */
		Root_component(Rpc_entrypoint *ep, Allocator *md_alloc)
		:
			_ep(*ep), _md_alloc(*md_alloc)
		{ }


		/**************************************
		 ** Local_service::Factory interface **
		 **************************************/

		Create_result create(Session_state::Args const &args, Affinity affinity) override
		{
			return Local_service::budget_adjusted_args(args, *md_alloc()).template convert<Create_result>(
				[&] (Session_state::Args const &adjusted_args) -> Create_result {
					return _create(adjusted_args, affinity); },
				[&] (Create_error e) { return e; });
		}

		void upgrade(SESSION_TYPE &session, Session_state::Args const &args) override
		{
			_upgrade_session(&session, args.string());
		}

		void destroy(SESSION_TYPE &session) override
		{
			close(session.cap());
		}


		/********************
		 ** Root interface **
		 ********************/

		Root::Result session(Root::Session_args const &args,
		                     Affinity           const &affinity) override
		{
			if (!args.valid_string()) return Create_error::DENIED;

			return Local_service::budget_adjusted_args(args.string(), *md_alloc())
				.template convert<Root::Result>(
					[&] (Session_state::Args const &adjusted_args) -> Root::Result {
						return _create(adjusted_args.string(), affinity)
							.template convert<Root::Result>(
								[&] (SESSION_TYPE &obj) { return obj.cap(); },
								[&] (Create_error e)    { return e; });
					},
					[&] (Create_error e) { return e; });
		}

		void upgrade(Session_capability cap, Root::Upgrade_args const &args) override
		{
			_ep.apply(cap, [&] (SESSION_TYPE *s) {
				if (s && args.valid_string())
					_upgrade_session(s, args.string()); });
		}

		void close(Session_capability session_cap) override
		{
			SESSION_TYPE * session;

			_ep.apply(session_cap, [&] (SESSION_TYPE *s) {
				session = s;

				/* let the entry point forget the session object */
				if (session) _ep.dissolve(session);
			});

			if (!session) return;

			_destroy_session(session);

			POLICY::release();
		}
};

#endif /* _INCLUDE__ROOT__COMPONENT_H_ */
