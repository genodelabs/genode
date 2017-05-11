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
		 * Entry point that manages the session objects
		 * created by this root interface
		 */
		Rpc_entrypoint *_ep;

		/*
		 * Allocator for allocating session objects.
		 * This allocator must be used by the derived
		 * class when calling the 'new' operator for
		 * creating a new session.
		 */
		Allocator *_md_alloc;

		/*
		 * Used by both the legacy 'Root::session' and the new 'Factory::create'
		 */
		SESSION_TYPE &_create(Session_state::Args const &args, Affinity affinity)
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

			/*
			 * We need to decrease 'ram_quota' by
			 * the size of the session object.
			 */
			Ram_quota const ram_quota = ram_quota_from_args(args.string());

			size_t needed = sizeof(SESSION_TYPE) + md_alloc()->overhead(sizeof(SESSION_TYPE));

			if (needed > ram_quota.value) {
				warning("insufficient ram quota "
				        "for ", SESSION_TYPE::service_name(), " session, "
				        "provided=", ram_quota, ", required=", needed);
				throw Insufficient_ram_quota();
			}

			Ram_quota const remaining_ram_quota { ram_quota.value - needed };

			/*
			 * Validate that the client provided the amount of caps as mandated
			 * for the session interface.
			 */
			Cap_quota const cap_quota = cap_quota_from_args(args.string());

			if (cap_quota.value < SESSION_TYPE::CAP_QUOTA)
				throw Insufficient_cap_quota();

			/*
			 * Account for the dataspace capability needed for allocating the
			 * session object from the sliced heap.
			 */
			if (cap_quota.value < 1)
				throw Insufficient_cap_quota();

			Cap_quota const remaining_cap_quota { cap_quota.value - 1 };

			/*
			 * Deduce ram quota needed for allocating the session object from the
			 * donated ram quota.
			 */
			enum { MAX_ARGS_LEN = 256 };
			char adjusted_args[MAX_ARGS_LEN];
			strncpy(adjusted_args, args.string(), sizeof(adjusted_args));

			Arg_string::set_arg(adjusted_args, sizeof(adjusted_args),
			                    "ram_quota", String<64>(remaining_ram_quota).string());

			Arg_string::set_arg(adjusted_args, sizeof(adjusted_args),
			                    "cap_quota", String<64>(remaining_cap_quota).string());

			SESSION_TYPE *s = 0;
			try { s = _create_session(adjusted_args, affinity); }
			catch (Out_of_ram)             { throw Insufficient_ram_quota(); }
			catch (Out_of_caps)            { throw Insufficient_cap_quota(); }
			catch (Service_denied)         { throw; }
			catch (Insufficient_cap_quota) { throw; }
			catch (Insufficient_ram_quota) { throw; }
			catch (...) {
				warning("unexpected exception during ",
				        SESSION_TYPE::service_name(), "-session creation"); }

			/*
			 * Consider that the session-object constructor may already have
			 * called 'manage'.
			 */
			if (!s->cap().valid())
				_ep->manage(s);

			aquire_guard.ack = true;
			return *s;
		}

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

		virtual SESSION_TYPE *_create_session(const char *args)
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
		Allocator *md_alloc() { return _md_alloc; }

		/**
		 * Return entrypoint that serves the root component
		 */
		Rpc_entrypoint *ep() { return _ep; }

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
			_ep(&ep.rpc_ep()), _md_alloc(&md_alloc)
		{ }

		/**
		 * Constructor
		 *
		 * \deprecated  use the constructor with the 'Entrypoint &'
		 *              argument instead
		 */
		Root_component(Rpc_entrypoint *ep, Allocator *md_alloc)
		:
			_ep(ep), _md_alloc(md_alloc)
		{ }


		/**************************************
		 ** Local_service::Factory interface **
		 **************************************/

		SESSION_TYPE &create(Session_state::Args const &args,
		                     Affinity affinity) override
		{
			try { return _create(args, affinity); }
			catch (Insufficient_ram_quota) { throw; }
			catch (Insufficient_cap_quota) { throw; }
			catch (...) { throw Service_denied(); }
		}

		void upgrade(SESSION_TYPE &session,
		             Session_state::Args const &args) override
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

		Session_capability session(Root::Session_args const &args,
		                           Affinity           const &affinity) override
		{
			if (!args.valid_string()) throw Service_denied();
			SESSION_TYPE &session = _create(args.string(), affinity);
			return session.cap();
		}

		void upgrade(Session_capability session, Root::Upgrade_args const &args) override
		{
			if (!args.valid_string()) throw Service_denied();

			_ep->apply(session, [&] (SESSION_TYPE *s) {
				if (!s) return;

				_upgrade_session(s, args.string());
			});
		}

		void close(Session_capability session_cap) override
		{
			SESSION_TYPE * session;

			_ep->apply(session_cap, [&] (SESSION_TYPE *s) {
				session = s;

				/* let the entry point forget the session object */
				if (session) _ep->dissolve(session);
			});

			if (!session) return;

			_destroy_session(session);

			POLICY::release();
		}
};

#endif /* _INCLUDE__ROOT__COMPONENT_H_ */
