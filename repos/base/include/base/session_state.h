/*
 * \brief  Representation of a session request
 * \author Norman Feske
 * \date   2016-10-10
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SESSION_STATE_H_
#define _INCLUDE__BASE__SESSION_STATE_H_

#include <util/xml_generator.h>
#include <util/list.h>
#include <util/volatile_object.h>
#include <session/capability.h>
#include <base/id_space.h>
#include <base/env.h>
#include <base/log.h>

namespace Genode {

	class Session_state;
	class Service;
}


class Genode::Session_state : public Parent::Client, public Parent::Server,
                              Noncopyable
{
	public:

		class Factory;

		typedef String<32>  Name;
		typedef String<256> Args;

		struct Ready_callback
		{
			virtual void session_ready(Session_state &) = 0;
		};

		struct Closed_callback
		{
			virtual void session_closed(Session_state &) = 0;
		};

	private:

		Service &_service;

		/**
		 * Total of quota associated with this session
		 */
		size_t _donated_ram_quota = 0;

		Factory *_factory = nullptr;

		Volatile_object<Id_space<Parent::Client>::Element> _id_at_client;

		Args     _args;
		Affinity _affinity;

	public:

		Lazy_volatile_object<Id_space<Parent::Server>::Element> id_at_server;

		/* ID for session requests towards the parent */
		Lazy_volatile_object<Id_space<Parent::Client>::Element> id_at_parent;

		Parent::Client parent_client;

		enum Phase { CREATE_REQUESTED,
		             INVALID_ARGS,
		             AVAILABLE,
		             CAP_HANDED_OUT,
		             UPGRADE_REQUESTED,
		             CLOSE_REQUESTED,
		             CLOSED };

		/**
		 * If set, the server reponds asynchronously to the session request.
		 * The client waits for a notification that is delivered as soon as 
		 * the server delivers the session capability.
		 */
		bool async_client_notify = false;

		Phase phase = CREATE_REQUESTED;

		Ready_callback  *ready_callback  = nullptr;
		Closed_callback *closed_callback = nullptr;

		/*
		 * Pointer to session interface for sessions that are implemented
		 * locally.
		 */
		Session *local_ptr = nullptr;

		Session_capability cap;

		size_t ram_upgrade = 0;

		void print(Output &out) const;

		/**
		 * Constructor
		 *
		 * \param service          interface that was used to create the session
		 * \param client_id_space  ID space for client-side session IDs
		 * \param client_id        session ID picked by the client
		 * \param args             session arguments
		 *
		 * \throw Id_space<Parent::Client>::Conflicting_id
		 */
		Session_state(Service                  &service,
		              Id_space<Parent::Client> &client_id_space,
		              Parent::Client::Id        client_id,
		              Args               const &args,
		              Affinity           const &affinity);

		~Session_state()
		{
			if (id_at_parent.is_constructed())
				error("dangling session in parent-side ID space: ", *this);
		}

		Service &service() { return _service; }

		/**
		 * Extend amount of ram attached to the session
		 */
		void confirm_ram_upgrade()
		{
			ram_upgrade = 0;
		}

		void increase_donated_quota(size_t upgrade)
		{
			_donated_ram_quota += upgrade;
			ram_upgrade = upgrade;
		}

		Parent::Client::Id id_at_client() const
		{
			return _id_at_client->id();
		}

		void discard_id_at_client()
		{
			_id_at_client.destruct();
		}

		Args const &args() const { return _args; }

		Affinity const &affinity() const { return _affinity; }

		void generate_session_request(Xml_generator &xml) const;

		size_t donated_ram_quota() const { return _donated_ram_quota; }

		bool alive() const
		{
			switch (phase) {

			case CREATE_REQUESTED:
			case INVALID_ARGS:
			case CLOSED:
				return false;

			case AVAILABLE:
			case CAP_HANDED_OUT:
			case UPGRADE_REQUESTED:
			case CLOSE_REQUESTED:
				return true;
			}
			return false;
		}

		/**
		 * Assign owner
		 *
		 * This function is called if the session-state object is created by
		 * 'Factory'. For statically created session objects, the '_factory' is
		 * a nullptr. The owner can be defined only once.
		 */
		void owner(Factory &factory) { if (!_factory) _factory = &factory; }

		/**
		 * Destroy session-state object
		 *
		 * This function has no effect for sessions not created via a 'Factory'.
		 */
		void destroy();
};


class Genode::Session_state::Factory : Noncopyable
{
	private:

		Allocator &_md_alloc;

	public:

		/**
		 * Constructor
		 *
		 * \param md_alloc  meta-data allocator used for allocating
		 *                  'Session_state' objects
		 */
		Factory(Allocator &md_alloc) : _md_alloc(md_alloc) { }

		/**
		 * Create a new session-state object
		 *
		 * The 'args' are passed the 'Session_state' constructor.
		 *
		 * \throw Allocator::Out_of_memory
		 */
		template <typename... ARGS>
		Session_state &create(ARGS &&... args)
		{
			Session_state &session = *new (_md_alloc) Session_state(args...);
			session.owner(*this);
			return session;
		}

	private:

		/*
		 * Allow only 'Session_state::destroy' to call 'Factory::_destroy'.
		 * This way, we make sure that the 'Session_state' is destroyed with
		 * the same factory that was used for creating it.
		 */
		friend class Session_state;

		void _destroy(Session_state &session) { Genode::destroy(_md_alloc, &session); }
};


#endif /* _INCLUDE__BASE__SESSION_STATE_H_ */
