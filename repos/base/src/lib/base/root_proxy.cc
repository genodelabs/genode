/*
 * \brief  Mechanism for dispatching session requests to root interfaces
 * \author Norman Feske
 * \date   2016-10-07
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include <base/log.h>
#include <base/attached_rom_dataspace.h>
#include <base/session_state.h>
#include <base/heap.h>
#include <base/tslab.h>
#include <root/client.h>

using namespace Genode;


namespace {

	struct Service
	{
		typedef Session_state::Name Name;

		Name             name;
		Capability<Root> root;

		struct Session : Parent::Server
		{
			Id_space<Parent::Server>::Element  id;
			Session_capability                 cap;
			Service                           &service;

			Session(Id_space<Parent::Server> &id_space, Parent::Server::Id id,
			        Service &service, Session_capability cap)
			:
				id(*this, id_space, id), cap(cap), service(service)
			{ }
		};
	};


	class Service_registry : Noncopyable
	{
		private:

			enum { MAX = 32 };

			Lock mutable _lock;
			Service      _services[MAX];
			unsigned     _cnt = 0;

		public:

			void insert(Service const &service)
			{
				Lock::Guard guard(_lock);

				if (_cnt == MAX) {
					error("maximum number of services announced");
					return;
				}

				_services[_cnt++] = service;
			}

			/**
			 * Call functor 'fn' with root capability for a given service name
			 */
			template <typename FUNC>
			void apply(Service::Name const &name, FUNC const &fn)
			{
				/*
				 * Protect '_services' but execute 'fn' with the lock released.
				 *
				 * If we called 'fn' with the lock held, the following scenario
				 * may result in a deadlock:
				 *
				 * A component provides two services, e.g., "Framebuffer" and
				 * "Input" (fb_sdl or nit_fb). In-between the two 'announce'
				 * calls (within the 'Component::construct' function), a
				 * service request for the already announced service comes in.
				 * The root proxy calls 'apply' with the service name, which
				 * results in an RPC call to the root interface that is served
				 * by the same entrypoint as 'Component::construct'. The RPC
				 * call blocks until 'Component::construct' returns. However,
				 * before returning, the function announces the second service,
				 * eventually arriving at 'Service_registry::insert', which
				 * tries to acquire the same lock as the blocking 'apply' call.
				 */
				_lock.lock();

				for (unsigned i = 0; i < _cnt; i++) {
					if (name != _services[i].name)
						continue;

					_lock.unlock();
					fn(_services[i]);
					return;
				}
				_lock.unlock();
			}
	};


	class Root_proxy
	{
		private:

			Env &_env;

			Id_space<Parent::Server> _id_space;

			Entrypoint _ep { _env, 2*1024*sizeof(long), "root" };

			Attached_rom_dataspace _session_requests { _env, "session_requests" };

			Signal_handler<Root_proxy> _session_request_handler {
				_ep, *this, &Root_proxy::_handle_session_requests };

			Sliced_heap _sliced_heap { _env.ram(), _env.rm() };

			Tslab<Service::Session, 4000> _session_slab { &_sliced_heap };

			void _handle_session_request(Xml_node);
			void _handle_session_requests();

			Service_registry _services;

		public:

			Root_proxy(Env &env) : _env(env)
			{
				_session_requests.sigh(_session_request_handler);
			}

			void announce(Service const &service)
			{
				_services.insert(service);

				/* trigger re-interpretation of the "session_requests" ROM */
				Signal_transmitter(_session_request_handler).submit();

				/* notify parent */
				_env.parent().announce(service.name.string());
			}
	};
}


void Root_proxy::_handle_session_request(Xml_node request)
{
	if (!request.has_attribute("id"))
		return;

	Parent::Server::Id const id { request.attribute_value("id", 0UL) };

	typedef Service::Session Session;

	if (request.has_type("create")) {

		if (!request.has_sub_node("args"))
			return;

		typedef Session_state::Args Args;
		Args const args = request.sub_node("args").decoded_content<Args>();

		try {
			Service::Name const name = request.attribute_value("service", Service::Name());

			_services.apply(name, [&] (Service &service) {

				// XXX affinity
				Session_capability cap =
					Root_client(service.root).session(args.string(), Affinity());

				new (_session_slab) Session(_id_space, id, service, cap);
				_env.parent().deliver_session_cap(id, cap);
			});
		}
		catch (Root::Invalid_args) {
			_env.parent().session_response(id, Parent::INVALID_ARGS); }
		catch (Root::Quota_exceeded) {
			_env.parent().session_response(id, Parent::INVALID_ARGS); }
		catch (Root::Unavailable) {
			_env.parent().session_response(id, Parent::INVALID_ARGS); }
	}

	if (request.has_type("upgrade")) {

		_id_space.apply<Session>(id, [&] (Session &session) {

			size_t ram_quota = request.attribute_value("ram_quota", 0UL);

			char buf[64];
			snprintf(buf, sizeof(buf), "ram_quota=%ld", ram_quota);

			// XXX handle Root::Invalid_args
			Root_client(session.service.root).upgrade(session.cap, buf);

			_env.parent().session_response(id, Parent::SESSION_OK);
		});
	}

	if (request.has_type("close")) {

		_id_space.apply<Session>(id, [&] (Session &session) {

			Root_client(session.service.root).close(session.cap);

			destroy(_session_slab, &session);

			_env.parent().session_response(id, Parent::SESSION_CLOSED);
		});
	}
}


void Root_proxy::_handle_session_requests()
{
	_session_requests.update();

	Xml_node const requests = _session_requests.xml();

	requests.for_each_sub_node([&] (Xml_node request) {
		_handle_session_request(request); });
}


static Env *env_ptr = nullptr;


namespace Genode { void init_root_proxy(Env &env) { env_ptr = &env; } }


void Parent::announce(Service_name const &name, Root_capability root)
{
	if (!env_ptr) {
		error("announce called prior init_root_proxy");
		return;
	}

	static Root_proxy root_proxy(*env_ptr);

	root_proxy.announce({ name.string(), root });
}
