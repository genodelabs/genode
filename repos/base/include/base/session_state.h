/*
 * \brief  Representation of a session request
 * \author Norman Feske
 * \date   2016-10-10
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__BASE__SESSION_STATE_H_
#define _INCLUDE__BASE__SESSION_STATE_H_

#include <util/xml_generator.h>
#include <util/list.h>
#include <util/reconstructible.h>
#include <session/capability.h>
#include <base/slab.h>
#include <base/id_space.h>
#include <base/env.h>
#include <base/log.h>
#include <base/session_label.h>

namespace Genode {

	class Session_state;
	class Service;
}


class Genode::Session_state : public Parent::Client, public Parent::Server
{
	public:

		class Factory;

		typedef String<32>  Name;
		typedef String<256> Args;

		struct Ready_callback : Interface
		{
			virtual void session_ready(Session_state &) = 0;
		};

		struct Closed_callback : Interface
		{
			virtual void session_closed(Session_state &) = 0;
		};

	private:

		Service &_service;

		/**
		 * Total of quota associated with this session
		 */
		Ram_quota _donated_ram_quota { 0 };
		Cap_quota _donated_cap_quota { 0 };

		Factory *_factory = nullptr;

		Reconstructible<Id_space<Parent::Client>::Element> _id_at_client;

		Session::Label const _label;
		Args                 _args;
		Affinity             _affinity;

		/*
		 * Noncopyable
		 */
		Session_state(Session_state const &);
		Session_state &operator = (Session_state const &);

	public:

		Constructible<Id_space<Parent::Server>::Element> id_at_server { };

		/* ID for session requests towards the parent */
		Constructible<Id_space<Parent::Client>::Element> id_at_parent { };

		Parent::Client parent_client { };

		enum Phase { CREATE_REQUESTED,
		             SERVICE_DENIED,
		             INSUFFICIENT_RAM_QUOTA,
		             INSUFFICIENT_CAP_QUOTA,
		             AVAILABLE,
		             CAP_HANDED_OUT,
		             UPGRADE_REQUESTED,
		             CLOSE_REQUESTED,
		             CLOSED };

		/**
		 * If set, the server responds asynchronously to the session request.
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

		Session_capability cap { };

		Ram_quota ram_upgrade { 0 };
		Cap_quota cap_upgrade { 0 };

		void print(Output &out) const;

		/**
		 * Constructor
		 *
		 * \param service          interface that was used to create the session
		 * \param label            session label to be presented to the server
		 * \param client_id_space  ID space for client-side session IDs
		 * \param client_id        session ID picked by the client
		 * \param args             session arguments
		 *
		 * \throw Id_space<Parent::Client>::Conflicting_id
		 *
		 * The client-provided (and child-name-prefixed) session label is
		 * contained in 'args'. In contrast, the 'label' argument is the label
		 * presented to the server along with the session request, which
		 * depends on the policy of 'Child_policy::resolve_session_request'.
		 */
		Session_state(Service                  &service,
		              Id_space<Parent::Client> &client_id_space,
		              Parent::Client::Id        client_id,
		              Session::Label     const &label,
		              Args               const &args,
		              Affinity           const &affinity);

		~Session_state()
		{
			if (id_at_parent.is_constructed())
				error("dangling session in parent-side ID space: ", *this);
		}

		Service       &service()       { return _service; }
		Service const &service() const { return _service; }

		/**
		 * Extend amount of ram attached to the session
		 */
		void confirm_ram_upgrade()
		{
			ram_upgrade = Ram_quota { 0 };
		}

		void increase_donated_quota(Ram_quota added_ram_quota,
		                            Cap_quota added_cap_quota)
		{
			_donated_ram_quota.value += added_ram_quota.value;
			_donated_cap_quota.value += added_cap_quota.value;
			ram_upgrade = added_ram_quota;
			cap_upgrade = added_cap_quota;
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

		void generate_session_request(Xml_generator &) const;

		struct Detail { enum Args { NO_ARGS, ARGS } args; };

		void generate_client_side_info(Xml_generator &, Detail detail) const;
		void generate_server_side_info(Xml_generator &, Detail detail) const;

		Ram_quota donated_ram_quota() const { return _donated_ram_quota; }
		Cap_quota donated_cap_quota() const { return _donated_cap_quota; }

		bool alive() const
		{
			switch (phase) {

			case CREATE_REQUESTED:
			case SERVICE_DENIED:
			case INSUFFICIENT_RAM_QUOTA:
			case INSUFFICIENT_CAP_QUOTA:
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

		bool closed() const
		{
			switch (phase) {

			case SERVICE_DENIED:
			case INSUFFICIENT_RAM_QUOTA:
			case INSUFFICIENT_CAP_QUOTA:
			case CLOSED:
				return true;

			case CREATE_REQUESTED:
			case AVAILABLE:
			case CAP_HANDED_OUT:
			case UPGRADE_REQUESTED:
			case CLOSE_REQUESTED:
				return false;
			}
			return false;
		}

		/**
		 * Return client-side label of the session request
		 */
		Session::Label client_label() const { return label_from_args(_args.string()); }

		bool client_exists() const { return _id_at_client.constructed(); }

		/**
		 * Return label presented to the server along with the session request
		 */
		Session::Label label() const { return _label; }

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

		/**
		 * Utility to override the client-provided label by the label assigned
		 * by 'Child_policy::resolve_session_request'.
		 */
		struct Server_args
		{
			char _buf[Args::capacity()];

			Server_args(Session_state const &session)
			{
				Genode::strncpy(_buf, session._args.string(), sizeof(_buf));
				Arg_string::set_arg_string(_buf, sizeof(_buf),
				                           "label", session._label.string());
			}

			char const *string() const { return _buf; }
		};
};


class Genode::Session_state::Factory : Noncopyable
{
	private:

		size_t const _batch_size;

		Slab _slab;

	public:

		struct Batch_size { size_t value; };

		/**
		 * Constructor
		 *
		 * \param md_alloc  meta-data allocator used for allocating
		 *                  'Session_state' objects
		 *
		 * \param batch     granularity of allocating blocks at 'md_alloc',
		 *                  must be greater than 0
		 */
		Factory(Allocator &md_alloc, Batch_size batch)
		:
			_batch_size(batch.value),
			/*
			 * The calculation of 'block_size' is just an approximation as
			 * a slab block contains a few bytes of meta data in addition
			 * to the actual slab entries.
			 */
			_slab(sizeof(Session_state), sizeof(Session_state)*_batch_size,
			      nullptr, &md_alloc)
		{ }

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
			Session_state &session = *new (_slab) Session_state(args...);
			session.owner(*this);
			return session;
		}

		/**
		 * Return number of bytes consumed per session
		 */
		size_t session_costs() const { return _slab.overhead(sizeof(Session_state)); }

	private:

		/*
		 * Allow only 'Session_state::destroy' to call 'Factory::_destroy'.
		 * This way, we make sure that the 'Session_state' is destroyed with
		 * the same factory that was used for creating it.
		 */
		friend class Session_state;

		void _destroy(Session_state &session) { Genode::destroy(_slab, &session); }
};


#endif /* _INCLUDE__BASE__SESSION_STATE_H_ */
