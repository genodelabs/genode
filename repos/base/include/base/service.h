/*
 * \brief  Service management framework
 * \author Norman Feske
 * \date   2006-07-12
 */

/*
 * Copyright (C) 2006-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__BASE__SERVICE_H_
#define _INCLUDE__BASE__SERVICE_H_

#include <root/client.h>
#include <base/log.h>
#include <util/list.h>
#include <ram_session/client.h>
#include <base/env.h>

namespace Genode {

	class Client;
	class Server;
	class Service;
	class Local_service;
	class Parent_service;
	class Child_service;
	class Service_registry;
}


/**
 * Client role
 *
 * A client is someone who applies for a service. If the service is not
 * available yet, we enqueue the client into a wait queue and wake him up
 * as soon as the requested service gets available.
 */
class Genode::Client : public List<Client>::Element
{
	private:

		Cancelable_lock _service_apply_lock;
		const char     *_apply_for;

	public:

		/**
		 * Constructor
		 */
		Client(): _service_apply_lock(Lock::LOCKED), _apply_for(0) { }

		virtual ~Client() { }

		/**
		 * Set/Request service name that we are currently applying for
		 */
		void apply_for(const char *apply_for) { _apply_for = apply_for; }
		const char *apply_for() { return _apply_for; }

		/**
		 * Service wait queue support
		 */
		void sleep()  { _service_apply_lock.lock(); }
		void wakeup() { _service_apply_lock.unlock(); }
};


/**
 * Server role
 *
 * A server is a process that provides one or multiple services. For the
 * most part, this class is used as an opaque key to represent the server
 * role.
 */
class Genode::Server
{
	private:

		Ram_session_capability _ram;

	public:

		/**
		 * Constructor
		 *
		 * \param ram  RAM session capability of the server process used,
		 *             for quota transfers from/to the server
		 */
		Server(Ram_session_capability ram): _ram(ram) { }

		/**
		 * Return RAM session capability of the server process
		 */
		Ram_session_capability ram_session_cap() const { return _ram; }
};


class Genode::Service : public List<Service>::Element
{
	public:

		enum { MAX_NAME_LEN = 32 };

	private:

		char _name[MAX_NAME_LEN];

	public:

		/*********************
		 ** Exception types **
		 *********************/

		class Invalid_args   { };
		class Unavailable    { };
		class Quota_exceeded { };

		/**
		 * Constructor
		 *
		 * \param name  service name
		 */
		Service(const char *name) { strncpy(_name, name, sizeof(_name)); }

		virtual ~Service() { }

		/**
		 * Return service name
		 */
		const char *name() const { return _name; }

		/**
		 * Create session
		 *
		 * \param args      session-construction arguments
		 * \param affinity  preferred CPU affinity of session
		 *
		 * \throw Invalid_args
		 * \throw Unavailable
		 * \throw Quota_exceeded
		 */
		virtual Session_capability session(char const *args,
		                                   Affinity const &affinity) = 0;

		/**
		 * Extend resource donation to an existing session
		 */
		virtual void upgrade(Session_capability session, const char *args) = 0;

		/**
		 * Close session
		 */
		virtual void close(Session_capability /*session*/) { }

		/**
		 * Return server providing the service
		 */
		virtual Server *server() const { return 0; }

		/**
		 * Return the RAM session to be used for trading resources
		 */
		Ram_session_capability ram_session_cap()
		{
			if (server())
				return server()->ram_session_cap();
			return Ram_session_capability();
		}
};


/**
 * Representation of a locally implemented service
 */
class Genode::Local_service : public Service
{
	private:

		Root *_root;

	public:

		Local_service(const char *name, Root *root)
		: Service(name), _root(root) { }

		Session_capability session(const char *args, Affinity const &affinity) override
		{
			try { return _root->session(args, affinity); }
			catch (Root::Invalid_args)   { throw Invalid_args(); }
			catch (Root::Unavailable)    { throw Unavailable(); }
			catch (Root::Quota_exceeded) { throw Quota_exceeded(); }
			catch (Genode::Ipc_error)    { throw Unavailable();  }
		}

		void upgrade(Session_capability session, const char *args) override
		{
			try { _root->upgrade(session, args); }
			catch (Genode::Ipc_error)      { throw Unavailable();    }
		}

		void close(Session_capability session) override
		{
			try { _root->close(session); }
			catch (Genode::Ipc_error)    { throw Blocking_canceled(); }
		}
};


/**
 * Representation of a service provided by our parent
 */
class Genode::Parent_service : public Service
{
	public:

		Parent_service(const char *name) : Service(name) { }

		Session_capability session(const char *args, Affinity const &affinity) override
		{
			try { return env()->parent()->session(name(), args, affinity); }
			catch (Parent::Unavailable) {
				warning("parent has no service \"", name(), "\"");
				throw Unavailable();
			}
			catch (Parent::Quota_exceeded) { throw Quota_exceeded(); }
			catch (Genode::Ipc_error)      { throw Unavailable();    }
		}

		void upgrade(Session_capability session, const char *args) override
		{
			try { env()->parent()->upgrade(session, args); }
			catch (Genode::Ipc_error)    { throw Unavailable();    }
		}

		void close(Session_capability session) override
		{
			try { env()->parent()->close(session); }
			catch (Genode::Ipc_error)    { throw Blocking_canceled(); }
		}
};


/**
 * Representation of a service that is implemented in a child
 */
class Genode::Child_service : public Service
{
	private:

		Root_capability _root_cap;
		Root_client     _root;
		Server         *_server;

	public:

		/**
		 * Constructor
		 *
		 * \param name    name of service
		 * \param root    capability to root interface
		 * \param server  server process providing the service
		 */
		Child_service(const char     *name,
		              Root_capability root,
		              Server         *server)
		: Service(name), _root_cap(root), _root(root), _server(server) { }

		Server *server() const override { return _server; }

		Session_capability session(const char *args, Affinity const &affinity) override
		{
			if (!_root_cap.valid())
				throw Unavailable();

			try { return _root.session(args, affinity); }
			catch (Root::Invalid_args)   { throw Invalid_args();   }
			catch (Root::Unavailable)    { throw Unavailable();    }
			catch (Root::Quota_exceeded) { throw Quota_exceeded(); }
			catch (Genode::Ipc_error)    { throw Unavailable();    }
		}

		void upgrade(Session_capability sc, const char *args) override
		{
			if (!_root_cap.valid())
				throw Unavailable();

			try { _root.upgrade(sc, args); }
			catch (Root::Invalid_args)   { throw Invalid_args();   }
			catch (Root::Unavailable)    { throw Unavailable();    }
			catch (Root::Quota_exceeded) { throw Quota_exceeded(); }
			catch (Genode::Ipc_error)    { throw Unavailable();    }
		}

		void close(Session_capability sc) override
		{
			try { _root.close(sc); }
			catch (Genode::Ipc_error)    { throw Blocking_canceled(); }
		}
};


/**
 * Container for holding service representations
 */
class Genode::Service_registry
{
	protected:

		Lock          _service_wait_queue_lock;
		List<Client>  _service_wait_queue;
		List<Service> _services;

	public:

		/**
		 * Probe for service with specified name
		 *
		 * \param name    service name
		 * \param server  server providing the service,
		 *                default (0) for any server
		 */
		Service *find(const char *name, Server *server = 0)
		{
			if (!name) return 0;

			Lock::Guard lock_guard(_service_wait_queue_lock);

			for (Service *s = _services.first(); s; s = s->next())
				if (strcmp(s->name(), name) == 0
				 && (!server || s->server() == server)) return s;

			return 0;
		}

		/**
		 * Check if service name is ambiguous
		 *
		 * \return true  if the same service is provided multiple
		 *               times
		 */
		bool is_ambiguous(const char *name)
		{
			Lock::Guard lock_guard(_service_wait_queue_lock);

			/* count number of services with the specified name */
			unsigned cnt = 0;
			for (Service *s = _services.first(); s; s = s->next())
				cnt += (strcmp(s->name(), name) == 0);
			return cnt > 1;
		}

		/**
		 * Return first service provided by specified server
		 */
		Service *find_by_server(Server *server)
		{
			Lock::Guard lock_guard(_service_wait_queue_lock);

			for (Service *s = _services.first(); s; s = s->next())
				if (s->server() == server)
					return s;

			return 0;
		}

		/**
		 * Wait for service
		 *
		 * This method is called by the clients's thread when requesting a
		 * session creation. It blocks if the requested service is not
		 * available.
		 *
		 * \return  service structure that matches the request or
		 *          0 if the waiting was canceled.
		 */
		Service *wait_for_service(const char *name, Client *client, const char *client_name)
		{
			Service *service;

			client->apply_for(name);

			_service_wait_queue_lock.lock();
			_service_wait_queue.insert(client);
			_service_wait_queue_lock.unlock();

			do {
				service = find(name);

				/*
				 * The service that we are seeking is not available today.
				 * Lets sleep a night over it.
				 */
				if (!service) {
					log(client_name, ": service ", name, " not yet available - sleeping");

					try {
						client->sleep();
						log(client_name, ": service ", name, " got available");
					} catch (Blocking_canceled) {
						log(client_name, ": cancel waiting for service");
						break;
					}
				}

			} while (!service);

			/* we got what we needed, stop applying */
			_service_wait_queue_lock.lock();
			_service_wait_queue.remove(client);
			_service_wait_queue_lock.unlock();

			client->apply_for(0);

			return service;
		}

		/**
		 * Register service
		 *
		 * This method is called by the server's thread.
		 */
		void insert(Service *service)
		{
			/* make new service known */
			_services.insert(service);

			/* wake up applicants waiting for the service */
			Lock::Guard lock_guard(_service_wait_queue_lock);
			for (Client *c = _service_wait_queue.first(); c; c = c->next())
				if (strcmp(service->name(), c->apply_for()) == 0)
					c->wakeup();
		}

		/**
		 * Unregister service
		 */
		void remove(Service *service) { _services.remove(service); }

		/**
		 * Unregister all services
		 */
		void remove_all()
		{
			while (_services.first())
				remove(_services.first());
		}
};

#endif /* _INCLUDE__BASE__SERVICE_H_ */
