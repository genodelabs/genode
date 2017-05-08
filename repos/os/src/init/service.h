/*
 * \brief  Services as targeted by session routes
 * \author Norman Feske
 * \date   2017-03-03
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__INIT__SERVICE_H_
#define _SRC__INIT__SERVICE_H_

/* Genode includes */
#include <base/service.h>
#include <base/child.h>

namespace Init {
	class Abandonable;
	class Parent_service;
	class Routed_service;
	class Forwarded_service;
}


class Init::Abandonable
{
	private:

		bool _abandoned = false;

	public:

		void abandon() { _abandoned = true; }

		bool abandoned() const { return _abandoned; }
};


class Init::Parent_service : public Genode::Parent_service, public Abandonable
{
	private:

		Registry<Parent_service>::Element _reg_elem;

	public:

		Parent_service(Registry<Parent_service> &registry, Env &env,
		               Service::Name const &name)
		:
			Genode::Parent_service(env, name), _reg_elem(registry, *this)
		{ }
};


/**
 * Init-specific representation of a child service
 */
class Init::Routed_service : public Async_service, public Abandonable
{
	public:

		typedef Child_policy::Name Child_name;

		struct Ram_accessor
		{
			virtual Ram_session           &ram()           = 0;
			virtual Ram_session_capability ram_cap() const = 0;
		};

		struct Pd_accessor
		{
			virtual Pd_session           &pd()           = 0;
			virtual Pd_session_capability pd_cap() const = 0;
		};

	private:

		Child_name _child_name;

		Ram_accessor &_ram_accessor;
		Pd_accessor  &_pd_accessor;

		Session_state::Factory &_factory;

		Registry<Routed_service>::Element _registry_element;

	public:

		/**
		 * Constructor
		 *
		 * \param services    registry of all services provides by children
		 * \param child_name  child name of server, used for session routing
		 *
		 * The other arguments correspond to the arguments of 'Async_service'.
		 */
		Routed_service(Registry<Routed_service> &services,
		               Child_name         const &child_name,
		               Ram_accessor             &ram_accessor,
		               Pd_accessor              &pd_accessor,
		               Id_space<Parent::Server> &server_id_space,
		               Session_state::Factory   &factory,
		               Service::Name      const &name,
		               Wakeup                   &wakeup)
		:
			Async_service(name, server_id_space, factory, wakeup),
			_child_name(child_name),
			_ram_accessor(ram_accessor), _pd_accessor(pd_accessor),
			_factory(factory), _registry_element(services, *this)
		{ }

		Child_name const &child_name() const { return _child_name; }

		Session_state::Factory &factory() { return _factory; }

		/**
		 * Ram_transfer::Account interface
		 */
		void transfer(Ram_session_capability to, Ram_quota amount) override
		{
			if (to.valid()) _ram_accessor.ram().transfer_quota(to, amount);
		}

		/**
		 * Ram_transfer::Account interface
		 */
		Ram_session_capability cap(Ram_quota) const override
		{
			return _ram_accessor.ram_cap();
		}

		/**
		 * Cap_transfer::Account interface
		 */
		void transfer(Pd_session_capability to, Cap_quota amount) override
		{
			if (to.valid()) _pd_accessor.pd().transfer_quota(to, amount);
		}

		/**
		 * Cap_transfer::Account interface
		 */
		Pd_session_capability cap(Cap_quota) const override
		{
			return _pd_accessor.pd_cap();
		}
};

#endif /* _SRC__INIT__SERVICE_H_ */
