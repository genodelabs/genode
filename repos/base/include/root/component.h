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

		using Result = Attempt<Ok, Session_error>;

		Result aquire(const char *)
		{
			if (_used)
				return Session_error::DENIED;

			_used = true;
			return Ok();
		}

		void release() { _used = false; }
};


/**
 * Session-creation policy for a multi-client service
 */
struct Genode::Multiple_clients
{
	using Result = Attempt<Ok, Session_error>;

	Result aquire(const char *) { return Ok(); }
	void release() { }
};


/**
 * Template for implementing the root interface
 *
 * \param SESSION  session-component type to manage,
 *                 derived from 'Rpc_object'
 * \param POLICY   session-creation policy
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
 *
 * 'release' is called at the destruction time of a session. It enables
 * the policy to keep track of and impose restrictions on the number
 * of existing sessions.
 *
 * The default policy 'Multiple_clients' imposes no restrictions on the
 * creation of new sessions.
 */
template <typename SESSION, typename POLICY>
class Genode::Root_component : public Rpc_object<Typed_root<SESSION> >,
                               public Local_service<SESSION>::Factory,
                               private POLICY
{
	public:

		using Local_service = Genode::Local_service<SESSION>;
		using Create_error  = Session_error;
		using Create_result = Local_service::Factory::Result;

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

		Memory::Constrained_obj_allocator<SESSION> _obj_alloc { *md_alloc() };

		/*
		 * Used by both the legacy 'Root::session' and the new 'Factory::create'
		 */
		Create_result _create(Session_state::Args const &args, Affinity affinity)
		{
			{
				typename POLICY::Result const result = POLICY::aquire(args.string());
				if (result.failed())
					return result.template convert<Create_error>(
						[] (auto &) /* never */ { return Create_error { }; },
						[] (Create_error e)     { return e; });
			}

			/*
			 * Guard to ensure that 'release' is called if '_create_session'
			 * throws an exception.
			 */
			struct Guard
			{
				bool ok = false;
				Root_component &root;
				Guard(Root_component &root) : root(root) { }
				~Guard() { if (!ok) root.release(); }
			} aquire_guard { *this };

			return _create_session(args.string(), affinity)
				.template convert<Create_result>([&] (SESSION &s) -> SESSION & {

					/*
					 * Consider that the session-object constructor may
					 * already have called 'manage'.
					 */
					if (!s.cap().valid())
						_ep.manage(&s);

					aquire_guard.ok = true;
					return s;
				},
				[&] (Create_error e) { return e; });
		}

		/*
		 * Noncopyable
		 */
		Root_component(Root_component const &);
		Root_component &operator = (Root_component const &);

	protected:

		/**
		 * Construct session object allocated from 'md_alloc'
		 */
		Create_result _alloc_obj(auto &&... args)
		{
			return _obj_alloc.create(args...).template convert<Create_result>(
				[&] (auto &a) -> SESSION & {
					a.deallocate = false;
					return a.obj;
				},
				[&] (Alloc_error e) {
					switch (e) {
					case Alloc_error::OUT_OF_RAM:  return Create_error::INSUFFICIENT_RAM;
					case Alloc_error::OUT_OF_CAPS: return Create_error::INSUFFICIENT_CAPS;
					case Alloc_error::DENIED:      break;
					}
					return Create_error::DENIED;
				});
		}

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
		 * This method supports the propagation of errors either as
		 * 'Create_error' result values or by exceptions.
		 *
		 * \throw Out_of_ram
		 * \throw Out_of_caps
		 * \throw Service_denied
		 * \throw Insufficient_cap_quota
		 * \throw Insufficient_ram_quota
		 */
		virtual Create_result _create_session(const char *args, Affinity const &)
		{
			return _create_session(args);
		}

		virtual Create_result _create_session(const char *)
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
		virtual void _upgrade_session(SESSION &, const char *) { }

		virtual void _destroy_session(SESSION &session) {
			Genode::destroy(_md_alloc, &session); }

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

		void upgrade(SESSION &session, Session_state::Args const &args) override
		{
			_upgrade_session(session, args.string());
		}

		void destroy(SESSION &session) override
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

			try {
				return Local_service::budget_adjusted_args(args.string(), *md_alloc())
					.template convert<Root::Result>(
						[&] (Session_state::Args const &adjusted_args) -> Root::Result {
							return _create(adjusted_args.string(), affinity)
								.template convert<Root::Result>(
									[&] (SESSION &obj) { return obj.cap(); },
									[&] (Create_error e)    { return e; });
						},
						[&] (Create_error e) { return e; });
			}
			catch (Out_of_ram)             { return Create_error::INSUFFICIENT_RAM; }
			catch (Out_of_caps)            { return Create_error::INSUFFICIENT_CAPS; }
			catch (Service_denied)         { return Create_error::DENIED; }
			catch (Insufficient_cap_quota) { return Create_error::INSUFFICIENT_CAPS; }
			catch (Insufficient_ram_quota) { return Create_error::INSUFFICIENT_RAM; }
			catch (...) {
				warning("unexpected exception during ",
				        SESSION::service_name(), "-session creation");
			}
			return Create_error::DENIED;
		}

		void upgrade(Session_capability cap, Root::Upgrade_args const &args) override
		{
			_ep.apply(cap, [&] (SESSION *s) {
				if (s && args.valid_string())
					_upgrade_session(*s, args.string()); });
		}

		void close(Session_capability session_cap) override
		{
			SESSION *session = nullptr;

			_ep.apply(session_cap, [&] (SESSION *s) {
				session = s;

				/* let the entry point forget the session object */
				if (session) _ep.dissolve(session);
			});

			if (!session) return;

			_destroy_session(*session);

			POLICY::release();
		}
};

#endif /* _INCLUDE__ROOT__COMPONENT_H_ */
