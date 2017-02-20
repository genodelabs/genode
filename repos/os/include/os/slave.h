/*
 * \brief  Convenience helper for running a service as child process
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2012-01-25
 */

/*
 * Copyright (C) 2012-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__OS__SLAVE_H_
#define _INCLUDE__OS__SLAVE_H_

/* Genode includes */
#include <base/rpc_server.h>
#include <base/local_connection.h>
#include <base/child.h>
#include <init/child_policy.h>
#include <os/child_policy_dynamic_rom.h>
#include <os/session_requester.h>

namespace Genode { struct Slave; }


struct Genode::Slave
{
	typedef Session_state::Args Args;

	class Policy;

	template <typename>
	class Connection_base;

	template <typename>
	struct Connection;
};


class Genode::Slave::Policy : public Child_policy
{
	public:

		typedef Child_policy::Name Name;
		typedef Session_label      Label;

		typedef Registered<Genode::Parent_service> Parent_service;
		typedef Registry<Parent_service>           Parent_services;

	private:

		Label                    const _label;
		Binary_name              const _binary_name;
		Ram_session_client             _ram;
		Genode::Parent_service         _binary_service;
		size_t                         _ram_quota;
		Parent_services               &_parent_services;
		Rpc_entrypoint                &_ep;
		Child_policy_dynamic_rom_file  _config_policy;
		Session_requester              _session_requester;

	public:

		class Connection;

		/**
		 * Slave-policy constructor
		 *
		 * \param ep        entrypoint used to provide local services
		 *                  such as the config ROM service
		 * \param local_rm  local address space, needed to populate dataspaces
		 *                  provided to the child (config, session_requests)
		 *
		 * \throw Ram_session::Alloc_failed by 'Child_policy_dynamic_rom_file'
		 * \throw Rm_session::Attach_failed by 'Child_policy_dynamic_rom_file'
		 */
		Policy(Label            const &label,
		       Name             const &binary_name,
		       Parent_services        &parent_services,
		       Rpc_entrypoint         &ep,
		       Region_map             &rm,
		       Ram_session_capability  ram_cap,
		       size_t                  ram_quota)
		:
			_label(label), _binary_name(binary_name), _ram(ram_cap),
			_binary_service(Rom_session::service_name()),
			_ram_quota(ram_quota), _parent_services(parent_services), _ep(ep),
			_config_policy(rm, "config", _ep, &_ram),
			_session_requester(ep, _ram, rm)
		{
			configure("<config/>");
		}

		/**
		 * Assign new configuration to slave
		 *
		 * \param config  new configuration as null-terminated string
		 */
		void configure(char const *config)
		{
			_config_policy.load(config, strlen(config) + 1);
		}

		void configure(char const *config, size_t len)
		{
			_config_policy.load(config, len);
		}

		void trigger_session_requests()
		{
			_session_requester.trigger_update();
		}


		/****************************
		 ** Child_policy interface **
		 ****************************/

		Name name() const override { return _label; }

		Binary_name binary_name() const override { return _binary_name; }

		Ram_session           &ref_ram()           override { return _ram; }
		Ram_session_capability ref_ram_cap() const override { return _ram; }

		void init(Ram_session &session, Ram_session_capability cap) override
		{
			session.ref_account(_ram);
			_ram.transfer_quota(cap, _ram_quota);
		}

		Service &resolve_session_request(Service::Name       const &service_name,
		                                 Session_state::Args const &args)
		{
			/* check for config file request */
			if (Service *s = _config_policy.resolve_session_request(service_name.string(), args.string()))
				return *s;

			if (service_name == "ROM") {
				Session_label const rom_name(label_from_args(args.string()).last_element());
				if (rom_name == _binary_name)       return _binary_service;
				if (rom_name == "session_requests") return _session_requester.service();
			}

			/* fill parent service registry on demand */
			Parent_service *service = nullptr;
			_parent_services.for_each([&] (Parent_service &s) {
				if (!service && s.name() == service_name)
					service = &s; });

			if (!service) {
				error(name(), ": illegal session request of "
				      "service \"", service_name, "\" (", args, ")");
				throw Parent::Service_denied();
			}

			return *service;
		}

		Id_space<Parent::Server> &server_id_space() override {
			return _session_requester.id_space(); }
};


template <typename CONNECTION>
class Genode::Slave::Connection_base
{
	protected:

		/* each connection appears as a separate client */
		Id_space<Parent::Client> _id_space;

		Policy &_policy;

		struct Service : Genode::Service, Session_state::Ready_callback,
		                                  Session_state::Closed_callback
		{
			Id_space<Parent::Server> &_id_space;

			Lock _lock { Lock::LOCKED };
			bool _alive = false;

			Service(Policy &policy)
			:
				Genode::Service(CONNECTION::service_name(), policy.ref_ram_cap()),
				_id_space(policy.server_id_space())
			{ }

			void initiate_request(Session_state &session) override
			{
				switch (session.phase) {

				case Session_state::CREATE_REQUESTED:

					if (!session.id_at_server.constructed())
						session.id_at_server.construct(session, _id_space);

					session.ready_callback = this;
					session.async_client_notify = true;
					break;

				case Session_state::UPGRADE_REQUESTED:
					warning("upgrading slaves is not implemented");
					session.phase = Session_state::CAP_HANDED_OUT;
					break;

				case Session_state::CLOSE_REQUESTED:
					warning("closing slave connections is not implemented");
					session.phase = Session_state::CLOSED;
					break;

				case Session_state::INVALID_ARGS:
				case Session_state::QUOTA_EXCEEDED:
				case Session_state::AVAILABLE:
				case Session_state::CAP_HANDED_OUT:
				case Session_state::CLOSED:
					break;
				}
			}

			void wakeup() override { }

			/**
			 * Session_state::Ready_callback
			 */
			void session_ready(Session_state &session) override
			{
				_alive = session.alive();
				_lock.unlock();
			}

			/**
			 * Session_state::Closed_callback
			 */
			void session_closed(Session_state &s) override { _lock.unlock(); }

		} _service;

		Local_connection<CONNECTION> _connection;

		Connection_base(Policy &policy, Args const &args, Affinity const &affinity)
		:
			_policy(policy), _service(_policy),
			_connection(_service, _id_space, { 1 }, args, affinity)
		{
			_policy.trigger_session_requests();
			_service._lock.lock();
			if (!_service._alive)
				throw Parent::Service_denied();
		}

		~Connection_base()
		{
			_policy.trigger_session_requests();
			_service._lock.lock();
		}

		typedef typename CONNECTION::Session_type SESSION;

		Capability<SESSION> _cap() const { return _connection.cap(); }
};


template <typename CONNECTION>
struct Genode::Slave::Connection : private Connection_base<CONNECTION>,
                                   public  CONNECTION::Client
{
	/**
	 * Constructor
	 *
	 * \throw Parent::Service_denied   parent denies session request
	 * \throw Parent::Quota_exceeded   our own quota does not suffice for
	 *                                 the creation of the new session
	 */
	Connection(Slave::Policy &policy, Args const &args,
	           Affinity const &affinity = Affinity())
	:
		Connection_base<CONNECTION>(policy, args, affinity),
		CONNECTION::Client(Connection_base<CONNECTION>::_cap())
	{ }

	Connection(Region_map &rm, Slave::Policy &policy, Args const &args,
	           Affinity const &affinity = Affinity())
	:
		Connection_base<CONNECTION>(policy, args, affinity),
		CONNECTION::Client(rm, Connection_base<CONNECTION>::_cap())
	{ }
};

#endif /* _INCLUDE__OS__SLAVE_H_ */
