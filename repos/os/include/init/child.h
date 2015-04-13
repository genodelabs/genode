/*
 * \brief  Representation used for children of the init process
 * \author Norman Feske
 * \date   2010-05-04
 */

/*
 * Copyright (C) 2010-2013 Genode Labs GmbH
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

	class Routed_service;
	class Name_registry;
	class Child_registry;
	class Child;
}


/***************
 ** Utilities **
 ***************/

namespace Init {

	extern bool config_verbose;

	static void warn_insuff_quota(Genode::size_t const avail)
	{
		if (!config_verbose) { return; }
		Genode::printf("Warning: Specified quota exceeds available quota.\n");
		Genode::printf("         Proceeding with a quota of %zu.\n", avail);
	}

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


	inline Genode::Affinity::Location
	read_affinity_location(Genode::Affinity::Space const &space,
	                       Genode::Xml_node start_node)
	{
		typedef Genode::Affinity::Location Location;
		try {
			Genode::Xml_node node = start_node.sub_node("affinity");

			/* if no position value is specified, select the whole row/column */
			unsigned long const
				default_width  = node.has_attribute("xpos") ? 1 : space.width(),
				default_height = node.has_attribute("ypos") ? 1 : space.height();

			unsigned long const
				width  = node.attribute_value<unsigned long>("width",  default_width),
				height = node.attribute_value<unsigned long>("height", default_height);

			long const x1 = node.attribute_value<long>("xpos", 0),
			           y1 = node.attribute_value<long>("ypos", 0),
			           x2 = x1 + width  - 1,
			           y2 = y1 + height - 1;

			/* clip location to space boundary */
			return Location(Genode::max(x1, 0L), Genode::max(y1, 0L),
			                Genode::min((unsigned)(x2 - x1 + 1), space.width()),
			                Genode::min((unsigned)(y2 - y1 + 1), space.height()));
		}
		catch (...) { return Location(0, 0, space.width(), space.height()); }
	}


	/**
	 * Return amount of RAM that is currently unused
	 */
	static inline Genode::size_t avail_slack_ram_quota()
	{
		Genode::size_t const preserve = 128*1024;
		Genode::size_t const avail    = Genode::env()->ram_session()->avail();

		return avail > preserve ? avail - preserve : 0;
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
	 * Return sub string of label with the leading child name stripped out
	 *
	 */
	inline char const *skip_label_prefix(char const *child_name, char const *label)
	{
		Genode::size_t const child_name_len = Genode::strlen(child_name);

		/*
		 * If the method was called with a valid "label" string, the
		 * following condition should be always satisfied. See the
		 * comment in 'service_node_args_condition_satisfied'.
		 */
		if (Genode::strcmp(child_name, label, child_name_len) == 0)
			label += child_name_len;

		/*
		 * If the original label was empty, the 'Child_policy_enforce_labeling'
		 * does not append a label separator after the child-name prefix. In
		 * this case, we resulting label is empty.
		 */
		if (*label == 0)
			return label;

		/*
		 * Skip label separator. This condition should be always satisfied.
		 */
		if (Genode::strcmp(" -> ", label, 4) == 0)
			return label + 4;

		PWRN("cannot skip label prefix while processing <if-arg>");
		return label;
	}


	/**
	 * Check if arguments satisfy the condition specified for the route
	 */
	inline bool service_node_args_condition_satisfied(Genode::Xml_node service_node,
	                                                  const char *args,
	                                                  char const *child_name)
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

			/*
			 * Skip child-name prefix if the key is the process "label".
			 *
			 * Because 'filter_session_args' is called prior the call of
			 * 'resolve_session_request' from the 'Child::session' method,
			 * 'args' contains the filtered arguments, in particular the label
			 * prefixed with the child's name. For the 'if-args' declaration,
			 * however, we want to omit specifying this prefix because the
			 * session route is specific to the named start node anyway. So
			 * the prefix information is redundant.
			 */
			if (Genode::strcmp("label", key) == 0)
				return Genode::strcmp(value, skip_label_prefix(child_name, arg_value)) == 0;

			return Genode::strcmp(value, arg_value) == 0;
		} catch (...) { }

		/* if no if-arg node exists, the condition is met */
		return true;
	}
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
class Init::Routed_service : public Genode::Service
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

		Genode::Session_capability session(const char *args,
		                                   Genode::Affinity const &affinity)
		{
			/*
			 * This method is called from the context of the client's
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
			try { cap = Genode::Root_client(_root).session(args, affinity); }
			catch (Genode::Root::Invalid_args)   { throw Invalid_args();   }
			catch (Genode::Root::Unavailable)    { throw Unavailable();    }
			catch (Genode::Root::Quota_exceeded) { throw Quota_exceeded(); }
			catch (Genode::Ipc_error)            { throw Unavailable();  }

			if (!cap.valid())
				throw Unavailable();

			return cap;
		}

		void upgrade(Genode::Session_capability sc, const char *args)
		{
			try { Genode::Root_client(_root).upgrade(sc, args); }
			catch (Genode::Root::Invalid_args) { throw Invalid_args(); }
			catch (Genode::Ipc_error)          { throw Unavailable(); }
		}

		void close(Genode::Session_capability sc)
		{
			try { Genode::Root_client(_root).close(sc); }
			catch (Genode::Ipc_error) { throw Genode::Blocking_canceled(); }
		}
};


/**
 * Interface for name database
 */
struct Init::Name_registry
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


class Init::Child : Genode::Child_policy
{
	public:

		/**
		 * Exception type
		 */
		class Child_name_is_not_unique { };

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
		 * Platform-specific PD-session arguments
		 */
		struct Pd_args : Genode::Native_pd_args
		{
			Pd_args(Genode::Xml_node start_node);
		} _pd_args;

		struct Read_quota
		{
			Read_quota(Genode::Xml_node start_node,
			           Genode::size_t & ram_quota,
			           Genode::size_t & cpu_quota_pc,
			           bool           & constrain_phys)
			{
				Genode::Number_of_bytes ram_bytes = 0;

				try {
					Genode::Xml_node rsc = start_node.sub_node("resource");
					for (;; rsc = rsc.next("resource")) {
						try {
							if (rsc.attribute("name").has_value("RAM")) {
								rsc.attribute("quantum").value(&ram_bytes);
								constrain_phys = rsc.attribute("constrain_phys").has_value("yes");
							} else if (rsc.attribute("name").has_value("CPU")) {
								rsc.attribute("quantum").value(&cpu_quota_pc); }
						} catch (...) { }
					}
				} catch (...) { }
				ram_quota = ram_bytes;

				/*
				 * If the configured RAM quota exceeds our own quota, we donate
				 * all remaining quota to the child but we need to count in
				 * our allocation of the child meta data from the heap.
				 * Hence, we preserve some of our own quota.
				 */
				Genode::size_t const ram_avail = avail_slack_ram_quota();
				if (ram_quota > ram_avail) {
					ram_quota = ram_avail;
					warn_insuff_quota(ram_avail);
				}
			}
		};

		/**
		 * Resources assigned to the child
		 */
		struct Resources : Read_quota
		{
			long                   prio_levels_log2;
			long                   priority;
			Genode::Affinity       affinity;
			Genode::size_t         ram_quota;
			Genode::size_t         cpu_quota_pc;
			bool                   constrain_phys;
			Genode::Ram_connection ram;
			Genode::Cpu_connection cpu;
			Genode::Rm_connection  rm;

			inline void transfer_cpu_quota();

			Resources(Genode::Xml_node start_node, const char *label,
			          long prio_levels_log2,
			          Genode::Affinity::Space const &affinity_space)
			:
				Read_quota(start_node, ram_quota, cpu_quota_pc, constrain_phys),
				prio_levels_log2(prio_levels_log2),
				priority(read_priority(start_node)),
				affinity(affinity_space,
				         read_affinity_location(affinity_space, start_node)),
				ram(label),
				cpu(label,
				    priority*(Genode::Cpu_session::PRIORITY_LIMIT >> prio_levels_log2),
				    affinity)
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

				transfer_cpu_quota();
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
		Genode::Rom_connection       _binary_rom;
		Genode::Dataspace_capability _binary_rom_ds;
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
		Init::Child_policy_pd_args               _pd_args_policy;
		Init::Child_policy_ram_phys              _ram_session_policy;

	public:

		Child(Genode::Xml_node              start_node,
		      Genode::Xml_node              default_route_node,
		      Name_registry                *name_registry,
		      long                          prio_levels_log2,
		      Genode::Affinity::Space const &affinity_space,
		      Genode::Service_registry      *parent_services,
		      Genode::Service_registry      *child_services,
		      Genode::Cap_session           *cap_session)
		:
			_list_element(this),
			_start_node(start_node),
			_default_route_node(default_route_node),
			_name_registry(name_registry),
			_name(start_node, name_registry),
			_pd_args(start_node),
			_resources(start_node, _name.unique, prio_levels_log2, affinity_space),
			_entrypoint(cap_session, ENTRYPOINT_STACK_SIZE, _name.unique, false, _resources.affinity.location()),
			_binary_rom(_name.file, _name.unique),
			_binary_rom_ds(_binary_rom.dataspace()),
			_config(_resources.ram.cap(), start_node),
			_server(_resources.ram.cap()),
			_child(_binary_rom_ds, _resources.ram.cap(),
			       _resources.cpu.cap(), _resources.rm.cap(), &_entrypoint, this),
			_parent_services(parent_services),
			_child_services(child_services),
			_labeling_policy(_name.unique),
			_priority_policy(_resources.prio_levels_log2, _resources.priority),
			_config_policy("config", _config.dataspace(), &_entrypoint),
			_binary_policy("binary", _binary_rom_ds, &_entrypoint),
			_configfile_policy("config", _config.filename()),
			_pd_args_policy(&_pd_args),
			_ram_session_policy(_resources.constrain_phys)
		{
			using namespace Genode;

			if (_resources.ram_quota == 0)
				PWRN("no valid RAM resource for child \"%s\"", _name.unique);

			if (config_verbose) {
				Genode::printf("child \"%s\"\n", _name.unique);
				Genode::printf("  RAM quota:  %zu\n", _resources.ram_quota);
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

		virtual ~Child() {
			Genode::Service *s;
			while ((s = _child_services->find_by_server(&_server))) {
				_child_services->remove(s);
			}
		}

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

					if (!service_node_args_condition_satisfied(service_node, args, name()))
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
							if (!server) {
								PWRN("%s: invalid route to non-existing server \"%s\"", name(), server_name);
								return 0;
							}

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
			_labeling_policy.  filter_session_args(service, args, args_len);
			_priority_policy.  filter_session_args(service, args, args_len);
			_configfile_policy.filter_session_args(service, args, args_len);
			_pd_args_policy.   filter_session_args(service, args, args_len);
			_ram_session_policy.filter_session_args(service, args, args_len);
		}

		Genode::Affinity filter_session_affinity(Genode::Affinity const &session_affinity)
		{
			using namespace Genode;

			Affinity::Space    const &child_space    = _resources.affinity.space();
			Affinity::Location const &child_location = _resources.affinity.location();

			/* check if no valid affinity space was specified */
			if (session_affinity.space().total() == 0)
				return Affinity(child_space, child_location);

			Affinity::Space    const &session_space    = session_affinity.space();
			Affinity::Location const &session_location = session_affinity.location();

			/* scale resolution of resulting space */
			Affinity::Space space(child_space.multiply(session_space));

			/* subordinate session affinity to child affinity subspace */
			Affinity::Location location(child_location
			                            .multiply_position(session_space)
			                            .transpose(session_location.xpos(),
			                                       session_location.ypos()));

			return Affinity(space, location);
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

		void resource_request(Genode::Parent::Resource_args const &args)
		{
			Genode::printf("child \"%s\" requests resources: %s\n",
			               name(), args.string());

			Genode::size_t const requested_ram_quota =
				Genode::Arg_string::find_arg(args.string(), "ram_quota")
					.ulong_value(0);

			if (avail_slack_ram_quota() < requested_ram_quota) {
				PWRN("Cannot respond to resource request - out of memory");
				return;
			}

			/*
			 * XXX  Synchronize quota transfers from/to env()->ram_session()
			 *
			 * If multiple children issue concurrent resource requests, the
			 * value reported by 'avail_slack_ram_quota' may be out of date
			 * when calling 'transfer_quota'.
			 */

			Genode::env()->ram_session()->transfer_quota(_resources.ram.cap(),
			                                             requested_ram_quota);

			/* wake up child that was starved for resources */
			_child.notify_resource_avail();
		}

		Genode::Native_pd_args const *pd_args() const { return &_pd_args; }
};


void Init::Child::Resources::transfer_cpu_quota()
{
	using Genode::Cpu_session;
	using Genode::size_t;
	static size_t avail = Cpu_session::quota_lim_upscale(         100, 100);
	size_t const   need = Cpu_session::quota_lim_upscale(cpu_quota_pc, 100);
	size_t need_adj;
	if (need > avail) {
		warn_insuff_quota(Cpu_session::quota_lim_downscale(avail, 100));
		need_adj = Cpu_session::quota_lim_upscale(100, 100);
		avail    = 0;
	} else {
		need_adj = Cpu_session::quota_lim_upscale(need, avail);
		avail   -= need;
	}
	cpu.ref_account(Genode::env()->cpu_session_cap());
	Genode::env()->cpu_session()->transfer_quota(cpu.cap(), need_adj);
}

#endif /* _INCLUDE__INIT__CHILD_H_ */
