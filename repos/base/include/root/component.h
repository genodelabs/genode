/*
 * \brief  Generic root component implementation
 * \author Norman Feske
 * \date   2006-05-22
 *
 * This class is there for your convenience. It performs the common actions
 * that must always be taken when creating a new session.
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__ROOT__COMPONENT_H_
#define _INCLUDE__ROOT__COMPONENT_H_

#include <root/root.h>
#include <base/rpc_server.h>
#include <base/heap.h>
#include <ram_session/ram_session.h>
#include <util/arg_string.h>
#include <base/printf.h>

namespace Genode {

	/**
	 * Session creation policy for a single-client service
	 */
	class Single_client
	{
		private:

			bool _used;

		public:

			Single_client() : _used(0) { }

			void aquire(const char *)
			{
				if (_used)
					throw Root::Unavailable();

				_used = true;
			}

			void release() { _used = false; }
	};


	/**
	 * Session-creation policy for a multi-client service
	 */
	struct Multiple_clients
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
	 * The 'POLICY' class must provide the following two functions:
	 *
	 * :'aquire(const char *args)': is called with the session arguments
	 *   at creation time of each new session. It can therefore implement
	 *   a session-creation policy taking session arguments into account.
	 *   If the policy denies the creation of a new session, it throws
	 *   one of the exceptions defined in the 'Root' interface.
	 *
	 * :'release': is called at the destruction time of a session. It enables
	 *   the policy to keep track of and impose restrictions on the number
	 *   of existing sessions.
	 *
	 * The default policy 'Multiple_clients' imposes no restrictions on the
	 * creation of new sessions.
	 */
	template <typename SESSION_TYPE, typename POLICY = Multiple_clients>
	class Root_component : public Rpc_object<Typed_root<SESSION_TYPE> >, private POLICY
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

		protected:

			/**
			 * Create new session (to be implemented by a derived class)
			 *
			 * Only a derived class knows the constructor arguments of
			 * a specific session. Therefore, we cannot unify the call
			 * of its 'new' operator and must implement the session
			 * creation at a place, where the required knowledge exist.
			 *
			 * In the implementation of this function, the heap, provided
			 * by 'Root_component' must be used for allocating the session
			 * object.
			 *
			 * If the server implementation does not evaluate the session
			 * affinity, it suffices to override the overload without the
			 * affinity argument.
			 *
			 * \throw Allocator::Out_of_memory  typically caused by the
			 *                                  meta-data allocator
			 * \throw Root::Invalid_args        typically caused by the
			 *                                  session-component constructor
			 */
			virtual SESSION_TYPE *_create_session(const char *args,
			                                      Affinity const &)
			{
				return _create_session(args);
			}

			virtual SESSION_TYPE *_create_session(const char *args)
			{
				throw Root::Invalid_args();
			}

			/**
			 * Inform session about a quota upgrade
			 *
			 * Once a session is created, its client can successively extend
			 * its quota donation via the 'Parent::transfer_quota' function.
			 * This will result in the invokation of 'Root::upgrade' at the
			 * root interface the session was created with. The root interface,
			 * in turn, informs the session about the new resources via the
			 * '_upgrade_session' function. The default implementation is
			 * suited for sessions that use a static amount of resources
			 * accounted for at session-creation time. For such sessions, an
			 * upgrade is not useful. However, sessions that dynamically
			 * allocate resources on behalf of its client, should respond to
			 * quota upgrades by implementing this function.
			 *
			 * \param session  session to upgrade
			 * \param args     description of additional resources in the
			 *                 same format as used at session creation
			 */
			virtual void _upgrade_session(SESSION_TYPE *, const char *) { }

			virtual void _destroy_session(SESSION_TYPE *session) {
				destroy(_md_alloc, session); }

			/**
			 * Return allocator to allocate server object in '_create_session()'
			 */
			Allocator      *md_alloc() { return _md_alloc; }
			Rpc_entrypoint *ep()       { return _ep; }

		public:

			/**
			 * Constructor
			 *
			 * \param ep           entry point that manages the sessions of this
			 *                     root interface.
			 * \param ram_session  provider of dataspaces for the backing store
			 *                     of session objects and session data
			 */
			Root_component(Rpc_entrypoint *ep, Allocator *metadata_alloc)
			: _ep(ep), _md_alloc(metadata_alloc) { }


			/********************
			 ** Root interface **
			 ********************/

			Session_capability session(Root::Session_args const &args,
			                           Affinity           const &affinity)
			{
				if (!args.is_valid_string()) throw Root::Invalid_args();

				POLICY::aquire(args.string());

				/*
				 * We need to decrease 'ram_quota' by
				 * the size of the session object.
				 */
				size_t ram_quota = Arg_string::find_arg(args.string(), "ram_quota").long_value(0);
				size_t needed = sizeof(SESSION_TYPE) + md_alloc()->overhead(sizeof(SESSION_TYPE));

				if (needed > ram_quota) {
					PERR("Insufficient ram quota, provided=%zu, required=%zu",
					     ram_quota, needed);
					throw Root::Quota_exceeded();
				}

				size_t const remaining_ram_quota = ram_quota - needed;

				/*
				 * Deduce ram quota needed for allocating the session object from the
				 * donated ram quota.
				 *
				 * XXX  the size of the 'adjusted_args' buffer should dependent
				 *      on the message-buffer size and stack size.
				 */
				enum { MAX_ARGS_LEN = 256 };
				char adjusted_args[MAX_ARGS_LEN];
				strncpy(adjusted_args, args.string(), sizeof(adjusted_args));
				char ram_quota_buf[64];
				snprintf(ram_quota_buf, sizeof(ram_quota_buf), "%zu",
				         remaining_ram_quota);
				Arg_string::set_arg(adjusted_args, sizeof(adjusted_args),
				                    "ram_quota", ram_quota_buf);

				SESSION_TYPE *s = 0;
				try { s = _create_session(adjusted_args, affinity); }
				catch (Allocator::Out_of_memory) { throw Root::Quota_exceeded(); }

				return _ep->manage(s);
			}

			void upgrade(Session_capability session, Root::Upgrade_args const &args)
			{
				if (!args.is_valid_string()) throw Root::Invalid_args();

				typedef typename Object_pool<SESSION_TYPE>::Guard Object_guard;
				Object_guard s(_ep->lookup_and_lock(session));
				if (!s) return;

				_upgrade_session(s, args.string());
			}

			void close(Session_capability session)
			{
				SESSION_TYPE * s =
					dynamic_cast<SESSION_TYPE *>(_ep->lookup_and_lock(session));
				if (!s) return;

				/* let the entry point forget the session object */
				_ep->dissolve(s);

				_destroy_session(s);

				POLICY::release();
				return;
			}
	};
}

#endif /* _INCLUDE__ROOT__COMPONENT_H_ */
