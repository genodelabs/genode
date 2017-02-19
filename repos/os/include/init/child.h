/*
 * \brief  Representation used for children of the init process
 * \author Norman Feske
 * \date   2010-05-04
 */

/*
 * Copyright (C) 2010-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__INIT__CHILD_H_
#define _INCLUDE__INIT__CHILD_H_

/* Genode includes */
#include <base/log.h>
#include <base/child.h>
#include <os/session_requester.h>
#include <os/session_policy.h>

/* init includes */
#include <init/verbose.h>
#include <init/child_config.h>
#include <init/child_policy.h>
#include <init/report.h>

namespace Init {

	class Routed_service;
	class Name_registry;
	class Child_registry;
	class Child;

	using namespace Genode;
	using Genode::size_t;
	using Genode::strlen;

	typedef Genode::Registered<Genode::Parent_service> Parent_service;
}


/***************
 ** Utilities **
 ***************/

namespace Init {

	static void warn_insuff_quota(size_t const avail)
	{
		warning("specified quota exceeds available quota, "
		        "proceeding with a quota of ", avail);
	}

	inline long read_priority(Xml_node start_node, long prio_levels)
	{
		long priority = Cpu_session::DEFAULT_PRIORITY;
		try { start_node.attribute("priority").value(&priority); }
		catch (...) { }

		/*
		 * All priority declarations in the config file are
		 * negative because child priorities can never be higher
		 * than parent priorities. To simplify priority
		 * calculations, we use inverted values. Lower values
		 * correspond to higher priorities.
		 */
		priority = -priority;

		if (priority && (priority >= prio_levels)) {
			long new_prio = prio_levels ? prio_levels-1 : 0;
			char name[Service::Name::capacity()];
			start_node.attribute("name").value(name, sizeof(name));
			warning(Cstring(name), ": invalid priority, upgrading "
			        "from ", -priority, " to ", -new_prio);
			return new_prio;
		}

		return priority;
	}


	inline Affinity::Location
	read_affinity_location(Affinity::Space const &space,
	                       Xml_node start_node)
	{
		typedef Affinity::Location Location;
		try {
			Xml_node node = start_node.sub_node("affinity");

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
			return Location(max(x1, 0L), max(y1, 0L),
			                min((unsigned)(x2 - x1 + 1), space.width()),
			                min((unsigned)(y2 - y1 + 1), space.height()));
		}
		catch (...) { return Location(0, 0, space.width(), space.height()); }
	}


	/**
	 * Return amount of RAM that is currently unused
	 */
	static inline size_t avail_slack_ram_quota(size_t ram_avail)
	{
		size_t const preserve = 148*1024;

		return ram_avail > preserve ? ram_avail - preserve : 0;
	}


	/**
	 * Return sub string of label with the leading child name stripped out
	 *
	 * \return character pointer to the scoped part of the label,
	 *         or nullptr if the label is not correctly prefixed with the child's
	 *         name
	 */
	inline char const *skip_label_prefix(char const *child_name, char const *label)
	{
		size_t const child_name_len = strlen(child_name);

		if (strcmp(child_name, label, child_name_len) != 0)
			return nullptr;

		label += child_name_len;

		/*
		 * Skip label separator. This condition should be always satisfied.
		 */
		if (strcmp(" -> ", label, 4) != 0)
			return nullptr;

		return label + 4;
	}


	/**
	 * Return true if service XML node matches service request
	 *
	 * \param args          session arguments, inspected for the session label
	 * \param child_name    name of the originator of the session request
	 * \param service_name  name of the requested service
	 */
	inline bool service_node_matches(Xml_node           const  service_node,
	                                 Session_label      const &label,
	                                 Child_policy::Name const &child_name,
	                                 Service::Name      const &service_name)
	{
		bool const service_matches =
			service_node.has_type("any-service") ||
			(service_node.has_type("service") &&
			 service_node.attribute("name").has_value(service_name.string()));

		if (!service_matches)
			return false;

		bool const route_depends_on_child_provided_label =
			service_node.has_attribute("label") ||
			service_node.has_attribute("label_prefix") ||
			service_node.has_attribute("label_suffix");

		char const *unscoped_attr = "unscoped_label";
		if (service_node.has_attribute(unscoped_attr)) {

			/*
			 * If an 'unscoped_label' attribute is provided, don't consider any
			 * scoped label attribute.
			 */
			if (route_depends_on_child_provided_label)
				warning("service node contains both scoped and unscoped label attributes");

			typedef String<Session_label::capacity()> Label;
			return label == service_node.attribute_value(unscoped_attr, Label());
		}

		if (!route_depends_on_child_provided_label)
			return true;

		char const * const scoped_label = skip_label_prefix(
			child_name.string(), label.string());

		if (!scoped_label)
			return false;

		Session_label const session_label(scoped_label);

		return !Xml_node_label_score(service_node, session_label).conflict();
	}


	/**
	 * Check if service name is ambiguous
	 *
	 * \return true  if the same service is provided multiple
	 *               times
	 *
	 * \deprecated
	 */
	template <typename T>
	inline bool is_ambiguous(Registry<T> const &services,
	                         Service::Name const &name)
	{
		/* count number of services with the specified name */
		unsigned cnt = 0;
		services.for_each([&] (T const &service) {
			cnt += (service.name() == name); });

		return cnt > 1;
	}


	template <typename T>
	inline Service *find_service(Registry<T> &services,
	                             Service::Name const &name)
	{
		T *service = nullptr;
		services.for_each([&] (T &s) {
			if (!service && (s.name() == name))
				service = &s; });
		return service;
	}


	inline void generate_ram_info(Xml_generator &xml, Ram_session const &ram)
	{
		/*
		 * The const cast is needed because the 'Ram_session' accessors are
		 * non-const methods.
		 */
		Ram_session &ram_nonconst = const_cast<Ram_session &>(ram);

		typedef String<32> Value;
		xml.attribute("quota", Value(Number_of_bytes(ram_nonconst.quota())));
		xml.attribute("used",  Value(Number_of_bytes(ram_nonconst.used())));
		xml.attribute("avail", Value(Number_of_bytes(ram_nonconst.avail())));
	}
}


/**
 * Init-specific representation of a child service
 */
class Init::Routed_service : public Child_service
{
	public:

		typedef Child_policy::Name Child_name;

	private:

		Child_name _child_name;

		Registry<Routed_service>::Element _registry_element;

	public:

		/**
		 * Constructor
		 *
		 * \param services    registry of all services provides by children
		 * \param child_name  child name of server, used for session routing
		 *
		 * The other arguments correspond to the arguments of 'Child_service'.
		 */
		Routed_service(Registry<Routed_service>         &services,
		               Child_name                 const &child_name,
		               Id_space<Parent::Server>         &server_id_space,
		               Session_state::Factory           &factory,
		               Service::Name              const &name,
		               Ram_session_capability            ram,
		               Child_service::Wakeup            &wakeup)
		:
			Child_service(server_id_space, factory, name, ram, wakeup),
			_child_name(child_name), _registry_element(services, *this)
		{ }

		Child_name const &child_name() const { return _child_name; }
};


/**
 * Interface for name database
 */
struct Init::Name_registry
{
	virtual ~Name_registry() { }

	typedef Child_policy::Name Name;

	/**
	 * Check if specified name is unique
	 *
	 * \return false if name already exists
	 */
	virtual bool unique(const char *name) const = 0;

	/**
	 * Return child name for a given alias name
	 *
	 * If there is no alias, the function returns the original name.
	 */
	virtual Name deref_alias(Name const &) = 0;
};


class Init::Child : Child_policy, Child_service::Wakeup
{
	public:

		/**
		 * Exception types
		 */
		struct Child_name_is_not_unique : Exception { };
		struct Missing_name_attribute   : Exception { };

		/**
		 * Unique ID of the child, solely used for diagnostic purposes
		 */
		struct Id { unsigned value; };

	private:

		friend class Child_registry;

		Env &_env;

		Allocator &_alloc;

		Verbose const &_verbose;

		Id const _id;

		Report_update_trigger &_report_update_trigger;

		List_element<Child> _list_element;

		Xml_node _start_node;

		Xml_node _default_route_node;

		Name_registry &_name_registry;

		typedef String<64> Name;

		struct Unique_name : Name
		{
			/**
			 * Read name from XML and check for name confict with other children
			 *
			 * \throw Missing_name_attribute
			 */
			static Name _checked(Xml_node start_node, Name_registry const &registry)
			{
				Name const name = start_node.attribute_value("name", Name());
				if (!name.valid()) {
					warning("missing 'name' attribute in '<start>' entry");
					throw Missing_name_attribute();
				}

				if (registry.unique(name.string()))
					return name;

				error("child name \"", name, "\" is not unique");
				throw Child_name_is_not_unique();
			}

			/**
			 * Constructor
			 *
			 * Obtains file name and unique process name from XML node
			 *
			 * \param start_node XML start node
			 * \param registry   registry tracking unique names
			 *
			 * \throw Missing_name_attribute
			 */
			Unique_name(Xml_node start_node, Name_registry const &registry)
			: Name(_checked(start_node, registry)) { }

		} _unique_name;

		static Binary_name _binary_name_from_xml(Xml_node start_node,
		                                         Unique_name const &unique_name)
		{
			if (!start_node.has_sub_node("binary"))
				return unique_name;

			return start_node.sub_node("binary").attribute_value("name", Name());
		}

		Binary_name const _binary_name;

		struct Read_quota
		{
			Read_quota(Xml_node       start_node,
			           size_t        &ram_quota,
			           size_t        &cpu_quota_pc,
			           bool          &constrain_phys,
			           size_t  const  ram_avail,
			           Verbose const &verbose)
			{
				cpu_quota_pc   = 0;
				constrain_phys = false;

				Number_of_bytes ram_bytes = 0;

				try {
					Xml_node rsc = start_node.sub_node("resource");
					for (;; rsc = rsc.next("resource")) {
						try {
							if (rsc.attribute("name").has_value("RAM")) {
								rsc.attribute("quantum").value(&ram_bytes);
								constrain_phys = rsc.attribute_value("constrain_phys", false);
							} else if (rsc.attribute("name").has_value("CPU")) {
								rsc.attribute("quantum").value(&cpu_quota_pc); }
						} catch (...) { }
					}
				} catch (...) { }
				ram_quota = ram_bytes;

				/*
				 * If the configured RAM quota exceeds our own quota, we donate
				 * all remaining quota to the child.
				 */
				if (ram_quota > ram_avail) {
					ram_quota = ram_avail;

					if (verbose.enabled())
						warn_insuff_quota(ram_avail);
				}
			}
		};

		/**
		 * Resources assigned to the child
		 */
		struct Resources : Read_quota
		{
			long     prio_levels_log2;
			long     priority;
			Affinity affinity;
			size_t   ram_quota;
			size_t   cpu_quota_pc;
			bool     constrain_phys;

			Resources(Xml_node start_node, long prio_levels,
			          Affinity::Space const &affinity_space, size_t ram_avail,
			          Verbose const &verbose)
			:
				Read_quota(start_node, ram_quota, cpu_quota_pc,
				           constrain_phys, ram_avail, verbose),
				prio_levels_log2(log2(prio_levels)),
				priority(read_priority(start_node, prio_levels)),
				affinity(affinity_space,
				         read_affinity_location(affinity_space, start_node))
			{
				/* deduce session costs from usable ram quota */
				ram_quota = Genode::Child::effective_ram_quota(ram_quota);
			}
		} _resources;

		Genode::Parent_service _env_ram_service { _env, Ram_session::service_name() };
		Genode::Parent_service _env_cpu_service { _env, Cpu_session::service_name() };
		Genode::Parent_service _env_pd_service  { _env, Pd_session::service_name() };
		Genode::Parent_service _env_log_service { _env, Log_session::service_name() };
		Genode::Parent_service _env_rom_service { _env, Rom_session::service_name() };

		Registry<Parent_service> &_parent_services;
		Registry<Routed_service> &_child_services;

		/**
		 * Private child configuration
		 */
		Init::Child_config _config;

		Session_requester _session_requester;

		/**
		 * Policy helpers
		 */
		Init::Child_policy_handle_cpu_priorities _priority_policy;
		Init::Child_policy_provide_rom_file      _config_policy;
		Init::Child_policy_redirect_rom_file     _configfile_policy;
		Init::Child_policy_ram_phys              _ram_session_policy;

		Genode::Child _child { _env.rm(), _env.ep().rpc_ep(), *this };

		/**
		 * Child_service::Wakeup callback
		 */
		void wakeup_child_service() override
		{
			_session_requester.trigger_update();
		}

	public:

		/**
		 * Constructor
		 *
		 * \param alloc  allocator solely used for configuration-dependent
		 *               allocations. It is not used for allocations on behalf
		 *               of the child's behavior.
		 *
		 *
		 * \throw Ram_session::Alloc_failed  allocation of config buffer failed
		 * \throw Region_map::Attach_failed  failed to temporarily attach
		 *                                   config dataspace to local address
		 *                                   space
		 */
		Child(Env                      &env,
		      Allocator                &alloc,
		      Verbose            const &verbose,
		      Id                        id,
		      Report_update_trigger    &report_update_trigger,
		      Xml_node                  start_node,
		      Xml_node                  default_route_node,
		      Name_registry            &name_registry,
		      long                      prio_levels,
		      Affinity::Space const    &affinity_space,
		      Registry<Parent_service> &parent_services,
		      Registry<Routed_service> &child_services)
		:
			_env(env), _alloc(alloc), _verbose(verbose), _id(id),
			_report_update_trigger(report_update_trigger),
			_list_element(this),
			_start_node(start_node),
			_default_route_node(default_route_node),
			_name_registry(name_registry),
			_unique_name(start_node, name_registry),
			_binary_name(_binary_name_from_xml(start_node, _unique_name)),
			_resources(start_node, prio_levels, affinity_space,
			           avail_slack_ram_quota(_env.ram().avail()), _verbose),
			_parent_services(parent_services),
			_child_services(child_services),
			_config(_env.ram(), _env.rm(), start_node),
			_session_requester(_env.ep().rpc_ep(), _env.ram(), _env.rm()),
			_priority_policy(_resources.prio_levels_log2, _resources.priority),
			_config_policy("config", _config.dataspace(), &_env.ep().rpc_ep()),
			_configfile_policy("config", _config.filename()),
			_ram_session_policy(_resources.constrain_phys)
		{
			if (_resources.ram_quota == 0)
				warning("no valid RAM resource for child "
				        "\"", _unique_name, "\"");

			if (_verbose.enabled()) {
				log("child \"",       _unique_name, "\"");
				log("  RAM quota:  ", _resources.ram_quota);
				log("  ELF binary: ", _binary_name);
				log("  priority:   ", _resources.priority);
			}

			/*
			 * Determine services provided by the child
			 */
			try {
				Xml_node service_node = start_node.sub_node("provides").sub_node("service");

				for (; ; service_node = service_node.next("service")) {

					char name[Service::Name::capacity()];
					service_node.attribute("name").value(name, sizeof(name));

					if (_verbose.enabled())
						log("  provides service ", Cstring(name));

					new (_alloc)
						Routed_service(child_services, this->name(),
						               _session_requester.id_space(),
						               _child.session_factory(),
						               name, _child.ram_session_cap(), *this);
				}
			}
			catch (Xml_node::Nonexistent_sub_node) { }
		}

		virtual ~Child()
		{
			_child_services.for_each([&] (Routed_service &service) {
				if (service.has_id_space(_session_requester.id_space()))
					destroy(_alloc, &service); });
		}

		/**
		 * Return true if the child has the specified name
		 */
		bool has_name(Child_policy::Name const &str) const { return str == name(); }

		void report_state(Xml_generator &xml, Report_detail const &detail) const
		{
			xml.node("child", [&] () {

				xml.attribute("name",   _unique_name);
				xml.attribute("binary", _binary_name);

				if (detail.ids())
					xml.attribute("id", _id.value);

				if (detail.child_ram() && _child.ram_session_cap().valid()) {
					xml.node("ram", [&] () {
						/*
						 * The const cast is needed because there is no const
						 * accessor for the RAM session of the child.
						 */
						auto &nonconst_child = const_cast<Genode::Child &>(_child);
						generate_ram_info(xml, nonconst_child.ram());
					});
				}

				Session_state::Detail const
					session_detail { detail.session_args() ? Session_state::Detail::ARGS
					                                       : Session_state::Detail::NO_ARGS};

				if (detail.requested()) {
					xml.node("requested", [&] () {
						_child.for_each_session([&] (Session_state const &session) {
							xml.node("session", [&] () {
								session.generate_client_side_info(xml, session_detail); }); }); });
				}

				if (detail.provided()) {
					xml.node("provided", [&] () {

						auto fn = [&] (Session_state const &session) {
							xml.node("session", [&] () {
								session.generate_server_side_info(xml, session_detail); }); };

						server_id_space().for_each<Session_state const>(fn);
					});
				}
			});
		}


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Child_policy::Name name() const override { return _unique_name; }

		Binary_name binary_name() const override { return _binary_name; }

		Ram_session           &ref_ram()           override { return _env.ram(); }
		Ram_session_capability ref_ram_cap() const override { return _env.ram_session_cap(); }

		void init(Ram_session &session, Ram_session_capability cap) override
		{
			session.ref_account(_env.ram_session_cap());
			_env.ram().transfer_quota(cap, _resources.ram_quota);
		}

		void init(Cpu_session &session, Cpu_session_capability cap) override
		{
			static size_t avail = Cpu_session::quota_lim_upscale(                    100, 100);
			size_t const   need = Cpu_session::quota_lim_upscale(_resources.cpu_quota_pc, 100);
			size_t need_adj = 0;
			if (need > avail || avail == 0) {
				warn_insuff_quota(Cpu_session::quota_lim_downscale(avail, 100));
				need_adj = Cpu_session::quota_lim_upscale(100, 100);
				avail    = 0;
			} else {
				need_adj = Cpu_session::quota_lim_upscale(need, avail);
				avail   -= need;
			}
			session.ref_account(_env.cpu_session_cap());
			_env.cpu().transfer_quota(cap, need_adj);
		}

		Id_space<Parent::Server> &server_id_space() override {
			return _session_requester.id_space(); }

		Route resolve_session_request(Service::Name const &service_name,
		                              Session_label const &label) override
		{
			Service *service = nullptr;

			/* check for "config" ROM request */
			if ((service = _config_policy.resolve_session_request_with_label(service_name, label)))
				return Route { *service, label };

			/* check for "session_requests" ROM request */
			if (service_name == Rom_session::service_name()
			 && label.last_element() == Session_requester::rom_name())
				return Route { _session_requester.service() };

			try {
				Xml_node route_node = _default_route_node;
				try {
					route_node = _start_node.sub_node("route"); }
				catch (...) { }
				Xml_node service_node = route_node.sub_node();

				for (; ; service_node = service_node.next()) {

					bool service_wildcard = service_node.has_type("any-service");

					if (!service_node_matches(service_node, label, name(), service_name))
						continue;

					Xml_node target = service_node.sub_node();
					for (; ; target = target.next()) {

						/*
						 * Determine session label to be provided to the server.
						 *
						 * By default, the client's identity (accompanied with
						 * the a client-provided label) is presented as session
						 * label to the server. However, the target node can
						 * explicitly override the client's identity by a
						 * custom label via the 'label' attribute.
						 */
						typedef String<Session_label::capacity()> Label;
						Label const target_label =
							target.attribute_value("label", Label(label.string()));

						if (target.has_type("parent")) {

							if ((service = find_service(_parent_services, service_name)))
								return Route { *service, target_label };

							if (!service_wildcard) {
								warning(name(), ": service lookup for "
								        "\"", service_name, "\" at parent failed");
								throw Parent::Service_denied();
							}
						}

						if (target.has_type("child")) {

							typedef Name_registry::Name Name;
							Name server_name = target.attribute_value("name", Name());
							server_name = _name_registry.deref_alias(server_name);

							_child_services.for_each([&] (Routed_service &s) {
								if (s.name()       == Service::Name(service_name)
								 && s.child_name() == server_name)
									service = &s; });
							if (service)
								return Route { *service, target_label };

							if (!service_wildcard) {
								warning(name(), ": lookup to child "
								        "server \"", server_name, "\" failed");
								throw Parent::Service_denied();
							}
						}

						if (target.has_type("any-child")) {
							if (is_ambiguous(_child_services, service_name)) {
								error(name(), ": ambiguous routes to "
								      "service \"", service_name, "\"");
								throw Parent::Service_denied();
							}

							if ((service = find_service(_child_services, service_name)))
								return Route { *service, target_label };

							if (!service_wildcard) {
								warning(name(), ": lookup for service "
								        "\"", service_name, "\" failed");
								throw Parent::Service_denied();
							}
						}

						if (target.last())
							break;
					}
				}
			} catch (...) {
				warning(name(), ": no route to service \"", service_name, "\"");
			}

			if (!service)
				throw Parent::Service_denied();

			return Route { *service };
		}

		void filter_session_args(Service::Name const &service,
		                         char *args, size_t args_len) override
		{
			_priority_policy.   filter_session_args(service.string(), args, args_len);
			_configfile_policy. filter_session_args(service.string(), args, args_len);
			_ram_session_policy.filter_session_args(service.string(), args, args_len);
		}

		Affinity filter_session_affinity(Affinity const &session_affinity) override
		{
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

		void announce_service(Service::Name const &service_name) override
		{
			log("child \"", name(), "\" announces service \"", service_name, "\"");

			bool found = false;
			_child_services.for_each([&] (Routed_service &service) {
				if (service.has_id_space(_session_requester.id_space())
				 && service.name() == service_name)
					found = true; });

			if (!found)
				error(name(), ": illegal announcement of "
				      "service \"", service_name, "\"");
		}

		void resource_request(Parent::Resource_args const &args) override
		{
			log("child \"", name(), "\" requests resources: ", args.string());

			size_t const requested_ram_quota =
				Arg_string::find_arg(args.string(), "ram_quota")
					.ulong_value(0);

			if (avail_slack_ram_quota(_env.ram().avail()) < requested_ram_quota) {
				warning("cannot respond to resource request - out of memory");
				return;
			}

			_env.ram().transfer_quota(_child.ram_session_cap(),
			                          requested_ram_quota);

			/* wake up child that was starved for resources */
			_child.notify_resource_avail();
		}

		void exit(int exit_value) override
		{
			try {
				if (_start_node.sub_node("exit").attribute_value("propagate", false)) {
					_env.parent().exit(exit_value);
					return;
				}
			} catch (...) { }

			/*
			 * Print a message as the exit is not handled otherwise. There are
			 * a number of automated tests that rely on this message. It is
			 * printed by the default implementation of 'Child_policy::exit'.
			 */
			Child_policy::exit(exit_value);
		}

		void session_state_changed() override
		{
			_report_update_trigger.trigger_report_update();
		}
};

#endif /* _INCLUDE__INIT__CHILD_H_ */
