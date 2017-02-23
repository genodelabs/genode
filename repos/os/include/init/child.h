/*
 * \brief  Representation used for children of the init process
 * \author Norman Feske
 * \date   2010-05-04
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
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
#include <init/child_policy.h>
#include <init/report.h>

namespace Init {

	class Abandonable;
	class Parent_service;
	class Buffered_xml;
	class Routed_service;
	class Name_registry;
	class Child_registry;
	class Child;

	using namespace Genode;
	using Genode::size_t;
	using Genode::strlen;

	struct Ram_quota { size_t value; };
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
	inline T *find_service(Registry<T> &services,
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


class Init::Buffered_xml
{
	private:

		Allocator         &_alloc;
		char const * const _ptr;   /* pointer to dynamically allocated buffer */
		Xml_node     const _xml;   /* referring to buffer of '_ptr' */

		/**
		 * \throw Allocator::Out_of_memory
		 */
		static char const *_init_ptr(Allocator &alloc, Xml_node node)
		{
			char *ptr = (char *)alloc.alloc(node.size());
			Genode::memcpy(ptr, node.addr(), node.size());
			return ptr;
		}

	public:

		/**
		 * Constructor
		 *
		 * \throw Allocator::Out_of_memory
		 */
		Buffered_xml(Allocator &alloc, Xml_node node)
		:
			_alloc(alloc), _ptr(_init_ptr(alloc, node)), _xml(_ptr, node.size())
		{ }

		~Buffered_xml() { _alloc.free(const_cast<char *>(_ptr), _xml.size()); }

		Xml_node xml() const { return _xml; }
};


/**
 * Init-specific representation of a child service
 */
class Init::Routed_service : public Child_service, public Abandonable
{
	public:

		typedef Child_policy::Name Child_name;

		struct Ram_accessor { virtual Ram_session_capability ram() const = 0; };

	private:

		Child_name _child_name;

		Ram_accessor &_ram_accessor;

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
		               Ram_accessor                     &ram_accessor,
		               Id_space<Parent::Server>         &server_id_space,
		               Session_state::Factory           &factory,
		               Service::Name              const &name,
		               Child_service::Wakeup            &wakeup)
		:
			Child_service(server_id_space, factory, name,
			              Ram_session_capability(), wakeup),
			_child_name(child_name), _ram_accessor(ram_accessor),
			_registry_element(services, *this)
		{ }

		Child_name const &child_name() const { return _child_name; }

		Ram_session_capability ram() const { return _ram_accessor.ram(); }
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

		struct Default_route_accessor { virtual Xml_node default_route() = 0; };

		struct Ram_limit_accessor { virtual Ram_quota ram_limit() = 0; };

	private:

		friend class Child_registry;

		Env &_env;

		Allocator &_alloc;

		Verbose const &_verbose;

		Id const _id;

		enum State { STATE_INITIAL, STATE_RAM_INITIALIZED, STATE_ALIVE,
		             STATE_ABANDONED };

		State _state = STATE_INITIAL;

		Report_update_trigger &_report_update_trigger;

		List_element<Child> _list_element;

		Reconstructible<Buffered_xml> _start_node;

		Default_route_accessor &_default_route_accessor;

		Ram_limit_accessor &_ram_limit_accessor;

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
			Read_quota(Xml_node        start_node,
			           size_t         &ram_quota,
			           size_t         &cpu_quota_pc,
			           bool           &constrain_phys,
			           Ram_quota const ram_limit,
			           Verbose  const &verbose)
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
				if (ram_quota > ram_limit.value) {
					ram_quota = ram_limit.value;

					if (verbose.enabled())
						warn_insuff_quota(ram_limit.value);
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
			size_t   assigned_ram_quota;
			size_t   effective_ram_quota;
			size_t   cpu_quota_pc;
			bool     constrain_phys;

			Resources(Xml_node start_node, long prio_levels,
			          Affinity::Space const &affinity_space, Ram_quota ram_limit,
			          Verbose const &verbose)
			:
				Read_quota(start_node, assigned_ram_quota, cpu_quota_pc,
				           constrain_phys, ram_limit, verbose),
				prio_levels_log2(log2(prio_levels)),
				priority(read_priority(start_node, prio_levels)),
				affinity(affinity_space,
				         read_affinity_location(affinity_space, start_node))
			{
				effective_ram_quota = Genode::Child::effective_ram_quota(assigned_ram_quota);
			}
		} _resources;

		Genode::Parent_service _env_ram_service { _env, Ram_session::service_name() };
		Genode::Parent_service _env_cpu_service { _env, Cpu_session::service_name() };
		Genode::Parent_service _env_pd_service  { _env, Pd_session::service_name() };
		Genode::Parent_service _env_log_service { _env, Log_session::service_name() };
		Genode::Parent_service _env_rom_service { _env, Rom_session::service_name() };

		Registry<Parent_service> &_parent_services;
		Registry<Routed_service> &_child_services;

		struct Inline_config_rom_service : Abandonable, Dynamic_rom_session::Content_producer
		{
			typedef Local_service<Dynamic_rom_session> Service;

			Child &_child;

			Dynamic_rom_session _session { _child._env.ep().rpc_ep(),
			                               _child.ref_ram(), _child._env.rm(),
			                               *this };

			Service::Single_session_factory _factory { _session };
			Service                         _service { _factory };

			Inline_config_rom_service(Child &child) : _child(child) { }

			/**
			 * Dynamic_rom_session::Content_producer interface
			 */
			void produce_content(char *dst, Genode::size_t dst_len) override
			{
				Xml_node config = _child._start_node->xml().has_sub_node("config")
				                ? _child._start_node->xml().sub_node("config")
				                : Xml_node("<config/>");

				size_t const config_len = config.size();

				if (config_len + 1 /* null termination */ >= dst_len)
					throw Buffer_capacity_exceeded();

				/*
				 * The 'config.size()' method returns the number of bytes of
				 * the config-node content, which is not null-terminated. Since
				 * 'Genode::strncpy' always null-terminates the result, the
				 * last byte of the source string is not copied. Hence, it is
				 * safe to add '1' to 'config_len' and thereby include the
				 * last actual config-content character in the result.
				 */
				Genode::strncpy(dst, config.addr(), config_len + 1);
			}

			void trigger_update() { _session.trigger_update(); }

			Service &service() { return _service; }
		};

		Constructible<Inline_config_rom_service> _config_rom_service;

		Session_requester _session_requester;

		/**
		 * Policy helpers
		 */
		Init::Child_policy_handle_cpu_priorities _priority_policy;
		Init::Child_policy_ram_phys              _ram_session_policy;

		Genode::Child _child { _env.rm(), _env.ep().rpc_ep(), *this };

		struct Ram_accessor : Routed_service::Ram_accessor
		{
			Genode::Child &_child;
			Ram_accessor(Genode::Child &child) : _child(child) { }
			Ram_session_capability ram() const override {
				return _child.ram_session_cap(); }
		} _ram_accessor { _child };

		/**
		 * Child_service::Wakeup callback
		 */
		void wakeup_child_service() override
		{
			_session_requester.trigger_update();
		}

		/**
		 * Return true if the policy results in the current route of the session
		 *
		 * This method is used to check if a policy change affects an existing
		 * client session of a child, i.e., to determine whether the child must
		 * be restarted.
		 */
		bool _route_valid(Session_state const &session)
		{
			try {
				Route const route =
					resolve_session_request(session.service().name(),
					                        session.client_label());

				return (session.service() == route.service)
				    && (route.label == session.label());
			}
			catch (Parent::Service_denied) { return false; }
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
		 * \throw Allocator::Out_of_memory   could not buffer the XML start node
		 */
		Child(Env                      &env,
		      Allocator                &alloc,
		      Verbose            const &verbose,
		      Id                        id,
		      Report_update_trigger    &report_update_trigger,
		      Xml_node                  start_node,
		      Default_route_accessor   &default_route_accessor,
		      Name_registry            &name_registry,
		      Ram_limit_accessor       &ram_limit_accessor,
		      long                      prio_levels,
		      Affinity::Space const    &affinity_space,
		      Registry<Parent_service> &parent_services,
		      Registry<Routed_service> &child_services)
		:
			_env(env), _alloc(alloc), _verbose(verbose), _id(id),
			_report_update_trigger(report_update_trigger),
			_list_element(this),
			_start_node(_alloc, start_node),
			_default_route_accessor(default_route_accessor),
			_ram_limit_accessor(ram_limit_accessor),
			_name_registry(name_registry),
			_unique_name(start_node, name_registry),
			_binary_name(_binary_name_from_xml(start_node, _unique_name)),
			_resources(start_node, prio_levels, affinity_space,
			           ram_limit_accessor.ram_limit(), _verbose),
			_parent_services(parent_services),
			_child_services(child_services),
			_session_requester(_env.ep().rpc_ep(), _env.ram(), _env.rm()),
			_priority_policy(_resources.prio_levels_log2, _resources.priority),
			_ram_session_policy(_resources.constrain_phys)
		{
			if (_resources.effective_ram_quota == 0)
				warning("no valid RAM resource for child "
				        "\"", _unique_name, "\"");

			if (_verbose.enabled()) {
				log("child \"",       _unique_name, "\"");
				log("  RAM quota:  ", _resources.effective_ram_quota);
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
						Routed_service(child_services, this->name(), _ram_accessor,
						               _session_requester.id_space(),
						               _child.session_factory(),
						               name, *this);
				}
			}
			catch (Xml_node::Nonexistent_sub_node) { }

			/*
			 * Construct inline config ROM service if "config" node is present.
			 */
			if (start_node.has_sub_node("config"))
				_config_rom_service.construct(*this);
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

		Ram_quota ram_quota() const { return Ram_quota { _resources.assigned_ram_quota }; }

		void initiate_env_ram_session()
		{
			if (_state == STATE_INITIAL) {
				_child.initiate_env_ram_session();
				_state = STATE_RAM_INITIALIZED;
			}
		}

		void initiate_env_sessions()
		{
			if (_state == STATE_RAM_INITIALIZED) {

				_child.initiate_env_sessions();

				/* check for completeness of the child's environment */
				if (_verbose.enabled())
					_child.for_each_session([&] (Session_state const &session) {
						if (!session.alive())
							warning(name(), ": incomplete environment ",
							        session.service().name(), " session "
							        "(", session.label(), ")"); });

				_state = STATE_ALIVE;
			}
		}

		void abandon()
		{
			_state = STATE_ABANDONED;

			_child_services.for_each([&] (Routed_service &service) {
				if (service.has_id_space(_session_requester.id_space()))
					service.abandon(); });
		}

		bool abandoned() const { return _state == STATE_ABANDONED; }

		enum Apply_config_result { MAY_HAVE_SIDE_EFFECTS, NO_SIDE_EFFECTS };

		/**
		 * Apply new configuration to child
		 *
		 * \throw Allocator::Out_of_memory  unable to allocate buffer for new
		 *                                  config
		 */
		Apply_config_result apply_config(Xml_node start_node)
		{
			Child_policy &policy = *this;

			if (_state == STATE_ABANDONED)
				return NO_SIDE_EFFECTS;

			enum Config_update { CONFIG_APPEARED, CONFIG_VANISHED,
			                     CONFIG_CHANGED,  CONFIG_UNCHANGED };

			Config_update config_update = CONFIG_UNCHANGED;

			/* import new start node if new version differs */
			if (start_node.size() != _start_node->xml().size() ||
			    Genode::memcmp(start_node.addr(), _start_node->xml().addr(),
			                   start_node.size()) != 0)
			{
				/*
				 * Start node changed
				 *
				 * Determine how the inline config is affected.
				 */
				char const * const tag = "config";
				bool const config_was_present = _start_node->xml().has_sub_node(tag);
				bool const config_is_present  = start_node.has_sub_node(tag);

				if (config_was_present && !config_is_present)
					config_update = CONFIG_VANISHED;

				if (!config_was_present && config_is_present)
					config_update = CONFIG_APPEARED;

				if (config_was_present && config_is_present) {

					Xml_node old_config = _start_node->xml().sub_node(tag);
					Xml_node new_config = start_node.sub_node(tag);

					if (Genode::memcmp(old_config.addr(), new_config.addr(),
					                   min(old_config.size(), new_config.size())))
						config_update = CONFIG_CHANGED;
				}

				/* import new start node */
				_start_node.construct(_alloc, start_node);
			}

			/*
			 * Apply change to '_config_rom_service'. This will
			 * potentially result in a change of the "config" ROM route, which
			 * may in turn prompt the routing-check below to abandon (restart)
			 * the child.
			 */
			switch (config_update) {
			case CONFIG_UNCHANGED:                                       break;
			case CONFIG_CHANGED:  _config_rom_service->trigger_update(); break;
			case CONFIG_APPEARED: _config_rom_service.construct(*this);  break;
			case CONFIG_VANISHED: _config_rom_service->abandon();        break;
			}

			/* validate that the routes of all existing sessions remain intact */
			{
				bool routing_changed = false;
				_child.for_each_session([&] (Session_state const &session) {
					if (!_route_valid(session))
						routing_changed = true; });

				if (routing_changed) {
					abandon();
					return MAY_HAVE_SIDE_EFFECTS;
				}
			}
			return NO_SIDE_EFFECTS;
		}

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

			size_t const initial_session_costs =
				session_alloc_batch_size()*_child.session_factory().session_costs();

			size_t const transfer_ram = _resources.effective_ram_quota > initial_session_costs
			                          ? _resources.effective_ram_quota - initial_session_costs
			                          : 0;
			if (transfer_ram)
				_env.ram().transfer_quota(cap, transfer_ram);
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
			/* check for "config" ROM request */
			if (service_name == Rom_session::service_name() &&
			    label.last_element() == "config") {

				if (_config_rom_service.constructed() &&
				   !_config_rom_service->abandoned())
					return Route { _config_rom_service->service(), label };

				/*
				 * \deprecated  the support for the <configfile> tag will
				 *              be removed
				 */
				if (_start_node->xml().has_sub_node("configfile")) {

					typedef String<50> Name;
					Name const rom =
						_start_node->xml().sub_node("configfile")
						                  .attribute_value("name", Name());

					/* prevent infinite recursion */
					if (rom == "config") {
						error("configfile must not be named 'config'");
						throw Parent::Service_denied();
					}

					return resolve_session_request(service_name,
					                               prefixed_label(name(), rom));
				}

				/*
				 * If there is neither an inline '<config>' nor a
				 * '<configfile>' node present, we apply the regular session
				 * routing to the "config" ROM request.
				 */
			}

			/* check for "session_requests" ROM request */
			if (service_name == Rom_session::service_name()
			 && label.last_element() == Session_requester::rom_name())
				return Route { _session_requester.service() };

			try {
				Xml_node route_node = _default_route_accessor.default_route();
				try {
					route_node = _start_node->xml().sub_node("route"); }
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

							Parent_service *service = nullptr;

							if ((service = find_service(_parent_services, service_name)))
								return Route { *service, target_label };

							if (service && service->abandoned())
								throw Parent::Service_denied();

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

							Routed_service *service = nullptr;

							_child_services.for_each([&] (Routed_service &s) {
								if (s.name()       == Service::Name(service_name)
								 && s.child_name() == server_name)
									service = &s; });

							if (service && service->abandoned())
								throw Parent::Service_denied();

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

							Routed_service *service = nullptr;

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
			} catch (Xml_node::Nonexistent_sub_node) { }

			warning(name(), ": no route to service \"", service_name, "\"");
			throw Parent::Service_denied();
		}

		void filter_session_args(Service::Name const &service,
		                         char *args, size_t args_len) override
		{
			_priority_policy.   filter_session_args(service.string(), args, args_len);
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

			if (_ram_limit_accessor.ram_limit().value < requested_ram_quota) {
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
				if (_start_node->xml().sub_node("exit").attribute_value("propagate", false)) {
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

		bool initiate_env_sessions() const override { return false; }
};

#endif /* _INCLUDE__INIT__CHILD_H_ */
