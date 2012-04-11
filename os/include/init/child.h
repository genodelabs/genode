/*
 * \brief  Representation used for children of the init process
 * \author Norman Feske
 * \date   2010-05-04
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__INIT__CHILD_H_
#define _INCLUDE__INIT__CHILD_H_

/* Genode includes */
#include <ram_session/connection.h>
#include <cpu_session/connection.h>
#include <cap_session/connection.h>
#include <base/printf.h>
#include <base/child.h>

/* init includes */
#include <init/child_config.h>
#include <init/child_policy.h>

namespace Init {

	extern bool config_verbose;


	/***************
	 ** Utilities **
	 ***************/

	inline long read_priority(Genode::Xml_node start_node)
	{
		long priority = Genode::Cpu_session::DEFAULT_PRIORITY;
		try { start_node.attribute("priority").value(&priority); }
		catch (...) { }

		/*
		 * All priority declarations in the config file are
		 * negative because child priorities can never be higher
		 * than parent priorities. To simplify priority
		 * calculations, we use inverted values. Lower values
		 * correspond to higher priorities.
		 */
		return -priority;
	}


	inline Genode::size_t read_ram_quota(Genode::Xml_node start_node)
	{
		Genode::Number_of_bytes ram_quota = 0;
		try {
			Genode::Xml_node rsc = start_node.sub_node("resource");
			for (;; rsc = rsc.next("resource")) {

				try {
					if (rsc.attribute("name").has_value("RAM")) {
						rsc.attribute("quantum").value(&ram_quota);
					}
				} catch (...) { }
			}
		} catch (...) { }

		/*
		 * If the configured quota exceeds our own quota, we donate
		 * all remaining quota to the child but we need to count in
		 * our allocation of the child meta data from the heap.
		 * Hence, we preserve some of our own quota.
		 */
		if (ram_quota > Genode::env()->ram_session()->avail() - 128*1024) {
			ram_quota = Genode::env()->ram_session()->avail() - 128*1024;
			if (config_verbose)
				Genode::printf("Warning: Specified quota exceeds available quota.\n"
				               "         Proceeding with a quota of %zd bytes.\n",
				               (Genode::size_t)ram_quota);
		}
		return ram_quota;
	}


	/**
	 * Return true if service XML node matches the specified service name
	 */
	inline bool service_node_matches(Genode::Xml_node service_node, const char *service_name)
	{
		if (service_node.has_type("any-service"))
			return true;

		return service_node.has_type("service")
		    && service_node.attribute("name").has_value(service_name);
	}


	/**
	 * Check if arguments satisfy the condition specified for the route
	 */
	inline bool service_node_args_condition_satisfied(Genode::Xml_node service_node,
	                                                  const char *args)
	{
		try {
			Genode::Xml_node if_arg = service_node.sub_node("if-arg");
			enum { KEY_MAX_LEN = 64, VALUE_MAX_LEN = 64 };
			char key[KEY_MAX_LEN];
			char value[VALUE_MAX_LEN];
			if_arg.attribute("key").value(key, sizeof(key));
			if_arg.attribute("value").value(value, sizeof(value));

			char arg_value[VALUE_MAX_LEN];
			Genode::Arg_string::find_arg(args, key).string(arg_value, sizeof(arg_value), "");
			return Genode::strcmp(value, arg_value) == 0;
		} catch (...) { }

		/* if no if-arg node exists, the condition is met */
		return true;
	}


	/**
	 * Init-specific representation of a child service
	 *
	 * For init, we introduce this 'Service' variant that distinguishes two
	 * phases, declared and announced. A 'Routed_service' object is created
	 * when a '<provides>' declaration is found in init's configuration.
	 * At that time, however, no children including the server do yet exist.
	 * If, at this stage, a client tries to open a session to this service,
	 * the client get enqueued in a list of applicants and blocked. When
	 * the server officially announces its service and passes over the root
	 * capability, the 'Routed_service' enters the announced stage and any
	 * applicants get unblocked.
	 */
	class Routed_service : public Genode::Service
	{
		private:

			Genode::Root_capability _root;
			bool                    _announced;
			Genode::Server         *_server;

			struct Applicant : public Genode::Cancelable_lock,
			                   public Genode::List<Applicant>::Element
			{
				Applicant() : Cancelable_lock(Genode::Lock::LOCKED) { }
			};

			Genode::Lock            _applicants_lock;
			Genode::List<Applicant> _applicants;

		public:

			/**
			 * Constructor
			 *
			 * \param name    name of service
			 * \param server  server providing the service
			 */
			Routed_service(const char     *name,
			               Genode::Server *server)
			: Service(name), _announced(false), _server(server) { }

			Genode::Server *server() const { return _server; }

			void announce(Genode::Root_capability root)
			{
				Genode::Lock::Guard guard(_applicants_lock);

				_root = root;
				_announced = true;

				/* wake up aspiring clients */
				for (Applicant *a; (a = _applicants.first()); ) {
					_applicants.remove(a);
					a->unlock();
				}
			}

			Genode::Session_capability session(const char *args)
			{
				/*
				 * This function is called from the context of the client's
				 * activation thread. If the service is not yet announced,
				 * we let the client block.
				 */
				_applicants_lock.lock();
				if (!_announced) {
					Applicant myself;
					_applicants.insert(&myself);
					_applicants_lock.unlock();
					myself.lock();
				} else
					_applicants_lock.unlock();

				Genode::Session_capability cap;
				try { cap = Genode::Root_client(_root).session(args); }
				catch (Genode::Root::Invalid_args)   { throw Invalid_args();   }
				catch (Genode::Root::Unavailable)    { throw Unavailable();    }
				catch (Genode::Root::Quota_exceeded) { throw Quota_exceeded(); }

				if (!cap.valid())
					throw Unavailable();

				return cap;
			}

			void upgrade(Genode::Session_capability sc, const char *args)
			{
				Genode::Root_client(_root).upgrade(sc, args);
			}

			void close(Genode::Session_capability sc)
			{
				Genode::Root_client(_root).close(sc);
			}
	};


	/**
	 * Interface for name database
	 */
	struct Name_registry
	{
		virtual ~Name_registry() { }

		/**
		 * Check if specified name is unique
		 *
		 * \return false if name already exists
		 */
		virtual bool is_unique(const char *name) const = 0;

		/**
		 * Find server with specified name
		 */
		virtual Genode::Server *lookup_server(const char *name) const = 0;
	};


	class Child_registry;


	class Child : Genode::Child_policy
	{
		private:

			friend class Child_registry;

			Genode::List_element<Child> _list_element;

			Genode::Xml_node _start_node;

			Genode::Xml_node _default_route_node;

			Name_registry *_name_registry;

			/**
			 * Unique child name and file name of ELF binary
			 */
			struct Name
			{
				enum { MAX_NAME_LEN = 64 };
				char file[MAX_NAME_LEN];
				char unique[MAX_NAME_LEN];

				/**
				 * Constructor
				 *
				 * Obtains file name and unique process name from XML node
				 *
				 * \param start_node XML start node
				 * \param registry   registry tracking unique names
				 */
				Name(Genode::Xml_node start_node, Name_registry const *registry) {
					try {
						start_node.attribute("name").value(unique, sizeof(unique)); }
					catch (Genode::Xml_node::Nonexistent_attribute) {
						PWRN("Missing 'name' attribute in '<start>' entry.\n");
						throw; }

					/* check for a name confict with the other children */
					if (!registry->is_unique(unique)) {
						PERR("Child name \"%s\" is not unique", unique);
						class Child_name_is_not_unique { };
						throw Child_name_is_not_unique();
					}

					/* use name as default file name if not declared otherwise */
					Genode::strncpy(file, unique, sizeof(file));

					/* check for a binary declaration */
					try {
						Genode::Xml_node binary = start_node.sub_node("binary");
						binary.attribute("name").value(file, sizeof(file));
					} catch (...) { }
				}
			} _name;

			/**
			 * Resources assigned to the child
			 */
			struct Resources
			{
				long                   prio_levels_log2;
				long                   priority;
				Genode::size_t         ram_quota;
				Genode::Ram_connection ram;
				Genode::Cpu_connection cpu;
				Genode::Rm_connection  rm;

				Resources(Genode::Xml_node start_node, const char *label,
				          long prio_levels_log2)
				:
					prio_levels_log2(prio_levels_log2),
					priority(read_priority(start_node)),
					ram_quota(read_ram_quota(start_node)),
					ram(label),
					cpu(label, priority*(Genode::Cpu_session::PRIORITY_LIMIT >> prio_levels_log2))
				{
					/* deduce session costs from usable ram quota */
					Genode::size_t session_donations = Genode::Rm_connection::RAM_QUOTA +
					                                   Genode::Cpu_connection::RAM_QUOTA +
					                                   Genode::Ram_connection::RAM_QUOTA;

					if (ram_quota > session_donations)
						ram_quota -= session_donations;
					else ram_quota = 0;

					ram.ref_account(Genode::env()->ram_session_cap());
					Genode::env()->ram_session()->transfer_quota(ram.cap(), ram_quota);
				}
			} _resources;

			/*
			 * Entry point used for serving the parent interface and the
			 * locally provided ROM sessions for the 'config' and 'binary'
			 * files.
			 */
			enum { ENTRYPOINT_STACK_SIZE = 12*1024 };
			Genode::Rpc_entrypoint _entrypoint;

			/**
			 * ELF binary
			 */
			Genode::Rom_connection _binary_rom;

			/**
			 * Private child configuration
			 */
			Init::Child_config _config;

			/**
			 * Each child of init can act as a server
			 */
			Genode::Server _server;

			Genode::Child _child;

			Genode::Service_registry *_parent_services;
			Genode::Service_registry *_child_services;

			/**
			 * Policy helpers
			 */
			Init::Child_policy_enforce_labeling      _labeling_policy;
			Init::Child_policy_handle_cpu_priorities _priority_policy;
			Init::Child_policy_provide_rom_file      _config_policy;
			Init::Child_policy_provide_rom_file      _binary_policy;
			Init::Child_policy_redirect_rom_file     _configfile_policy;

		public:

			Child(Genode::Xml_node          start_node,
			      Genode::Xml_node          default_route_node,
			      Name_registry            *name_registry,
			      long                      prio_levels_log2,
			      Genode::Service_registry *parent_services,
			      Genode::Service_registry *child_services,
			      Genode::Cap_session      *cap_session)
			:
				_list_element(this),
				_start_node(start_node),
				_default_route_node(default_route_node),
				_name_registry(name_registry),
				_name(start_node, name_registry),
				_resources(start_node, _name.unique, prio_levels_log2),
				_entrypoint(cap_session, ENTRYPOINT_STACK_SIZE, _name.unique, false),
				_binary_rom(_name.file, _name.unique),
				_config(_resources.ram.cap(), start_node),
				_server(_resources.ram.cap()),
				_child(_binary_rom.dataspace(), _resources.ram.cap(),
				       _resources.cpu.cap(), _resources.rm.cap(), &_entrypoint, this),
				_parent_services(parent_services),
				_child_services(child_services),
				_labeling_policy(_name.unique),
				_priority_policy(_resources.prio_levels_log2, _resources.priority),
				_config_policy("config", _config.dataspace(), &_entrypoint),
				_binary_policy("binary", _binary_rom.dataspace(), &_entrypoint),
				_configfile_policy("config", _config.filename())
			{
				using namespace Genode;

				if (_resources.ram_quota == 0)
					PWRN("no valid RAM resource for child \"%s\"", _name.unique);

				if (config_verbose) {
					Genode::printf("child \"%s\"\n", _name.unique);
					Genode::printf("  RAM quota:  %zd\n", _resources.ram_quota);
					Genode::printf("  ELF binary: %s\n", _name.file);
					Genode::printf("  priority:   %ld\n", _resources.priority);
				}

				/*
				 * Determine services provided by the child
				 */
				try {
					Xml_node service_node = start_node.sub_node("provides").sub_node("service");

					for (; ; service_node = service_node.next("service")) {

						char name[Genode::Service::MAX_NAME_LEN];
						service_node.attribute("name").value(name, sizeof(name));

						if (config_verbose)
							Genode::printf("  provides service %s\n", name);

						child_services->insert(new (_child.heap())
							Routed_service(name, &_server));

					}
				} catch (Xml_node::Nonexistent_sub_node) { }
			}

			virtual ~Child() { }

			/**
			 * Return true if the child has the specified name
			 */
			bool has_name(const char *n) const { return !Genode::strcmp(name(), n); }

			Genode::Server *server() { return &_server; }

			/**
			 * Start execution of child
			 */
			void start() { _entrypoint.activate(); }


			/****************************
			 ** Child-policy interface **
			 ****************************/

			const char *name() const { return _name.unique; }

			Genode::Service *resolve_session_request(const char *service_name,
			                                         const char *args)
			{
				Genode::Service *service = 0;

				/* check for config file request */
				if ((service = _config_policy.resolve_session_request(service_name, args)))
					return service;

				/* check for binary file request */
				if ((service = _binary_policy.resolve_session_request(service_name, args)))
					return service;

				try {
					Genode::Xml_node route_node = _default_route_node;
					try {
						route_node = _start_node.sub_node("route"); }
					catch (...) { }
					Genode::Xml_node service_node = route_node.sub_node();

					for (; ; service_node = service_node.next()) {

						bool service_wildcard = service_node.has_type("any-service");

						if (!service_node_matches(service_node, service_name))
							continue;

						if (!service_node_args_condition_satisfied(service_node, args))
							continue;

						Genode::Xml_node target = service_node.sub_node();
						for (; ; target = target.next()) {

							if (target.has_type("parent")) {
								service = _parent_services->find(service_name);
								if (service)
									return service;

								if (!service_wildcard) {
									PWRN("%s: service lookup for \"%s\" at parent failed", name(), service_name);
									return 0;
								}
							}

							if (target.has_type("child")) {
								char server_name[Name::MAX_NAME_LEN];
								server_name[0] = 0;
								target.attribute("name").value(server_name, sizeof(server_name));

								Genode::Server *server = _name_registry->lookup_server(server_name);
								if (!server)
									PWRN("%s: invalid route to non-existing server \"%s\"", name(), server_name);

								service = _child_services->find(service_name, server);
								if (service)
									return service;

								if (!service_wildcard) {
									PWRN("%s: lookup to child service \"%s\" failed", name(), service_name);
									return 0;
								}
							}

							if (target.has_type("any-child")) {
								if (_child_services->is_ambiguous(service_name)) {
									PERR("%s: ambiguous routes to service \"%s\"", name(), service_name);
									return 0;
								}
								service = _child_services->find(service_name);
								if (service)
									return service;

								if (!service_wildcard) {
									PWRN("%s: lookup for service \"%s\" failed", name(), service_name);
									return 0;
								}
							}

							if (target.is_last())
								break;
						}
					}
				} catch (...) {
					PWRN("%s: no route to service \"%s\"", name(), service_name);
				}
				return service;
			}

			void filter_session_args(const char *service,
			                         char *args, Genode::size_t args_len)
			{
				_labeling_policy.filter_session_args(service, args, args_len);
				_priority_policy.filter_session_args(service, args, args_len);
				_configfile_policy.filter_session_args(service, args, args_len);
			}

			bool announce_service(const char             *service_name,
			                      Genode::Root_capability root,
			                      Genode::Allocator      *alloc,
			                      Genode::Server         *server)
			{
				if (config_verbose)
					Genode::printf("child \"%s\" announces service \"%s\"\n",
					               name(), service_name);

				Genode::Service *s = _child_services->find(service_name, &_server);
				Routed_service *rs = dynamic_cast<Routed_service *>(s);
				if (!s || !rs) {
					PERR("%s: illegal announcement of service \"%s\"", name(), service_name);
					return false;
				}

				rs->announce(root);
				return true;
			}
	};
}

#endif /* _INCLUDE__INIT__CHILD_H_ */
