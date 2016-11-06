/*
 * \brief  Core-specific parent client implementation
 * \author Norman Feske
 * \author Christian Helmuth
 * \date   2006-07-20
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _CORE__INCLUDE__CORE_PARENT_H_
#define _CORE__INCLUDE__CORE_PARENT_H_

#include <parent/parent.h>
#include <base/service.h>
#include <base/allocator.h>

namespace Genode {

	template <typename> struct Core_service;
	struct Core_parent;
}


template <typename SESSION>
struct Genode::Core_service : Local_service<SESSION>, Registry<Service>::Element
{
	Core_service(Registry<Service>                        &registry,
	             typename Local_service<SESSION>::Factory &factory)
	:
		Local_service<SESSION>(factory),
		Registry<Service>::Element(registry, *this)
	{ }
};


/**
 * Core has no parent. But most of Genode's library code could work seamlessly
 * inside core if it had one. Core_parent fills this gap.
 */
class Genode::Core_parent : public Parent
{
	private:

		Id_space<Client>   _id_space;
		Allocator         &_alloc;
		Registry<Service> &_services;

	public:

		/**
		 * Constructor
		 *
		 * \alloc  allocator to be used for allocating core-local
		 *         'Session_state' objects
		 */
		Core_parent(Allocator &alloc, Registry<Service> &services)
		: _alloc(alloc), _services(services) { }

		void exit(int) override;

		void announce(Service_name const &) override { }

		void session_sigh(Signal_context_capability) override { }

		Session_capability session(Client::Id, Service_name const &, Session_args const &,
		                           Affinity const &) override;

		Session_capability session_cap(Client::Id) override { return Session_capability(); }

		Upgrade_result upgrade(Client::Id, Upgrade_args const &) override {
			throw Quota_exceeded(); }

		Close_result close(Client::Id) override { return CLOSE_DONE; }

		void session_response(Server::Id, Session_response) override { }

		void deliver_session_cap(Server::Id,
		                         Session_capability) override { }

		Thread_capability main_thread_cap() const override { return Thread_capability(); }

		void resource_avail_sigh(Signal_context_capability) override { }

		void resource_request(Resource_args const &) override { }

		void yield_sigh(Signal_context_capability) override { }

		Resource_args yield_request() override { return Resource_args(); }

		void yield_response() override { }
};

#endif /* _CORE__INCLUDE__CORE_PARENT_H_ */
