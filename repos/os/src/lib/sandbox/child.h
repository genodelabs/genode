/*
 * \brief  Child representation
 * \author Norman Feske
 * \date   2010-05-04
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIB__SANDBOX__CHILD_H_
#define _LIB__SANDBOX__CHILD_H_

/* Genode includes */
#include <base/log.h>
#include <base/child.h>
#include <os/session_requester.h>
#include <os/session_policy.h>
#include <os/buffered_xml.h>
#include <os/sandbox.h>

/* local includes */
#include <types.h>
#include <verbose.h>
#include <report.h>
#include <name_registry.h>
#include <service.h>
#include <utils.h>

namespace Sandbox { class Child; }

class Sandbox::Child : Child_policy, Routed_service::Wakeup
{
	public:

		typedef String<80> Version;

		/**
		 * Exception types
		 */
		struct Child_name_is_not_unique : Exception { };
		struct Missing_name_attribute   : Exception { };

		/**
		 * Unique ID of the child, solely used for diagnostic purposes
		 */
		struct Id { unsigned value; };

		struct Default_route_accessor : Interface { virtual Xml_node default_route() = 0; };
		struct Default_caps_accessor  : Interface { virtual Cap_quota default_caps() = 0; };

		template <typename QUOTA>
		struct Resource_limit_accessor : Interface
		{
			/*
			 * The argument is unused. It exists solely as an overload selector.
			 */
			virtual QUOTA resource_limit(QUOTA const &) const = 0;
		};

		typedef Resource_limit_accessor<Ram_quota> Ram_limit_accessor;
		typedef Resource_limit_accessor<Cap_quota> Cap_limit_accessor;

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

		/*
		 * Version attribute of the start node, used to force child restarts.
		 */
		Version _version { _start_node->xml().attribute_value("version", Version()) };

		/*
		 * True if the binary is loaded with ld.lib.so
		 */
		bool const _use_ld = _start_node->xml().attribute_value("ld", true);

		Default_route_accessor &_default_route_accessor;
		Default_caps_accessor  &_default_caps_accessor;
		Ram_limit_accessor     &_ram_limit_accessor;
		Cap_limit_accessor     &_cap_limit_accessor;

		Name_registry &_name_registry;

		/**
		 * Read name from XML and check for name confict with other children
		 *
		 * \throw Missing_name_attribute
		 */
		static Name _name_from_xml(Xml_node start_node)
		{
			Name const name = start_node.attribute_value("name", Name());
			if (name.valid())
				return name;

			warning("missint 'name' attribute in '<start>' entry");
			throw Missing_name_attribute();
		}

		typedef String<64> Name;
		Name const _unique_name { _name_from_xml(_start_node->xml()) };

		static Binary_name _binary_from_xml(Xml_node start_node,
		                                    Name const &unique_name)
		{
			if (!start_node.has_sub_node("binary"))
				return unique_name;

			return start_node.sub_node("binary").attribute_value("name", Name());
		}

		/* updated on configuration update */
		Binary_name _binary_name { _binary_from_xml(_start_node->xml(), _unique_name) };

		/* initialized in constructor, updated by 'apply_config' */
		bool _heartbeat_enabled;

		/*
		 * Number of skipped heartbeats when last checked
		 *
		 * This variable is used for the triggering of state-report updates
		 * due to heartbeat events.
		 */
		unsigned _last_skipped_heartbeats = 0;

		/* return true if heartbeat tracking is active */
		bool _heartbeat_expected() const
		{
			/* don't expect heartbeats from a child that is not yet complete */
			return _heartbeat_enabled && (_state == STATE_ALIVE);
		}

		/**
		 * Resources assigned to the child
		 */
		struct Resources
		{
			long      prio_levels_log2;
			long      priority;
			Affinity  affinity;
			Ram_quota assigned_ram_quota;
			Cap_quota assigned_cap_quota;
			size_t    cpu_quota_pc;
			bool      constrain_phys;

			Ram_quota effective_ram_quota() const
			{
				return Genode::Child::effective_quota(assigned_ram_quota);
			}

			Cap_quota effective_cap_quota() const
			{
				/* capabilities consumed by 'Genode::Child' */
				Cap_quota const effective =
					Genode::Child::effective_quota(assigned_cap_quota);

				/* capabilities additionally consumed by init */
				enum {
					STATIC_COSTS = 1  /* possible heap backing-store
					                     allocation for session object */
					             + 1  /* buffered XML start node */
					             + 2  /* dynamic ROM for config */
					             + 2  /* dynamic ROM for session requester */
				};

				if (effective.value < STATIC_COSTS)
					return Cap_quota{0};

				return Cap_quota{effective.value - STATIC_COSTS};
			}
		};

		Resources _resources_from_start_node(Xml_node start_node, Prio_levels prio_levels,
		                                     Affinity::Space const &affinity_space,
		                                     Cap_quota default_cap_quota, Cap_quota)
		{
			size_t          cpu_quota_pc   = 0;
			bool            constrain_phys = false;
			Number_of_bytes ram_bytes      = 0;

			size_t caps = start_node.attribute_value("caps", default_cap_quota.value);

			start_node.for_each_sub_node("resource", [&] (Xml_node rsc) {

				typedef String<8> Name;
				Name const name = rsc.attribute_value("name", Name());

				if (name == "RAM") {
					ram_bytes      = rsc.attribute_value("quantum", ram_bytes);
					constrain_phys = rsc.attribute_value("constrain_phys", false);
				}

				if (name == "CPU") {
					cpu_quota_pc = rsc.attribute_value("quantum", 0UL);
				}

				if (name == "CAP") {
					caps = rsc.attribute_value("quantum", 0UL);
				}
			});

			return Resources { log2(prio_levels.value),
			                   priority_from_xml(start_node, prio_levels),
			                   Affinity(affinity_space,
			                            affinity_location_from_xml(affinity_space, start_node)),
			                   Ram_quota { ram_bytes },
			                   Cap_quota { caps },
			                   cpu_quota_pc,
			                   constrain_phys };
		}

		Resources _resources;

		/**
		 * Print diagnostic information on misconfiguration
		 */
		void _clamp_resources(Ram_quota ram_limit, Cap_quota cap_limit)
		{
			if (_resources.assigned_ram_quota.value > ram_limit.value) {
				warning(name(), " assigned RAM (", _resources.assigned_ram_quota, ") "
				        "exceeds available RAM (", ram_limit, ")");
				_resources.assigned_ram_quota = ram_limit;
			}

			if (_resources.assigned_cap_quota.value > cap_limit.value) {
				warning(name(), " assigned caps (", _resources.assigned_cap_quota, ") "
				        "exceed available caps (", cap_limit, ")");
				_resources.assigned_cap_quota = cap_limit;
			}
		}

		Ram_quota _configured_ram_quota() const;
		Cap_quota _configured_cap_quota() const;

		bool const _resources_clamped_to_limit;

		using Local_service = Genode::Sandbox::Local_service_base;

		Registry<Parent_service> &_parent_services;
		Registry<Routed_service> &_child_services;
		Registry<Local_service>  &_local_services;

		struct Inline_config_rom_service : Abandonable, Dynamic_rom_session::Content_producer
		{
			typedef Genode::Local_service<Dynamic_rom_session> Service;

			Child &_child;

			Dynamic_rom_session _session { _child._env.ep().rpc_ep(),
			                               _child.ref_pd(), _child._env.rm(),
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

				config.with_raw_node([&] (char const *start, size_t length) {

					/*
					 * The 'length' is the number of bytes of the config-node
					 * content, which is not null-terminated. Since
					 * 'Genode::copy_cstring' always null-terminates the
					 * result, the last byte of the source string is not
					 * copied. Hence, it is safe to add '1' to 'length' and
					 * thereby include the last actual config-content character
					 * in the result.
					 */
					copy_cstring(dst, start, length + 1);
				});
			}

			void trigger_update() { _session.trigger_update(); }

			Service &service() { return _service; }
		};

		Constructible<Inline_config_rom_service> _config_rom_service { };

		Session_requester _session_requester;

		/**
		 * CPU-session priority parameters
		 */
		long const _prio_levels_log2 { _resources.prio_levels_log2 };
		long const _priority         { _resources.priority };

		/**
		 * If set to true, the child is allowed to constrain physical RAM
		 * allocations.
		 */
		bool const _constrain_phys { _resources.constrain_phys };

		/**
		 * Resource request initiated by the child
		 */
		struct Requested_resources
		{
			Ram_quota const ram;
			Cap_quota const caps;

			Requested_resources(Parent::Resource_args const &args)
			:
				ram (ram_quota_from_args(args.string())),
				caps(cap_quota_from_args(args.string()))
			{ }
		};

		Constructible<Requested_resources> _requested_resources { };

		Genode::Child _child { _env.rm(), _env.ep().rpc_ep(), *this };

		struct Pd_accessor : Routed_service::Pd_accessor
		{
			Genode::Child &_child;

			Pd_accessor(Genode::Child &child) : _child(child) { }

			Pd_session           &pd()            override { return _child.pd(); }
			Pd_session_capability pd_cap()  const override { return _child.pd_session_cap(); }

		} _pd_accessor { _child };

		struct Ram_accessor : Routed_service::Ram_accessor
		{
			Genode::Child &_child;

			Ram_accessor(Genode::Child &child) : _child(child) { }

			Pd_session           &ram()            override { return _child.pd(); }
			Pd_session_capability ram_cap()  const override { return _child.pd_session_cap(); }

		} _ram_accessor { _child };

		/**
		 * Async_service::Wakeup callback
		 */
		void wakeup_async_service() override
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
			catch (Service_denied) { return false; }
		}

		static Xml_node _provides_sub_node(Xml_node start_node)
		{
			return start_node.has_sub_node("provides")
			     ? start_node.sub_node("provides") : Xml_node("<provides/>");
		}

		/**
		 * Return true if service is provided by this child
		 */
		bool _provided_by_this(Routed_service const &service) const
		{
			return service.has_id_space(_session_requester.id_space());
		}

		/**
		 * Return true if service of specified <provides> sub node is known
		 */
		bool _service_exists(Xml_node node) const
		{
			bool exists = false;
			_child_services.for_each([&] (Routed_service const &service) {
				if (_provided_by_this(service) &&
				    service.name() == node.attribute_value("name", Service::Name()))
					exists = true; });

			return exists && !abandoned();
		}

		void _add_service(Xml_node service)
		{
			Service::Name const name =
				service.attribute_value("name", Service::Name());

			if (_verbose.enabled())
				log("  provides service ", name);

			new (_alloc)
				Routed_service(_child_services, this->name(),
				               _pd_accessor, _ram_accessor,
				               _session_requester.id_space(),
				               _child.session_factory(),
				               name, *this);
		}

		/*
		 * Exit state of the child set when 'exit()' is executed
		 * and reported afterwards through the state report.
		 */

		bool _exited     { false };
		int  _exit_value { -1 };

		void _destroy_services();

	public:

		/**
		 * Constructor
		 *
		 * \param alloc               allocator solely used for configuration-
		 *                            dependent allocations. It is not used for
		 *                            allocations on behalf of the child's
		 *                            behavior.
		 *
		 * \param ram_limit           maximum amount of RAM to be consumed at
		 *                            creation time.
		 *
		 * \param ram_limit_accessor  interface for querying the available
		 *                            RAM, used for dynamic RAM balancing at
		 *                            runtime.
		 *
		 * \throw Allocator::Out_of_memory  could not buffer the XML start node
		 */
		Child(Env                      &env,
		      Allocator                &alloc,
		      Verbose            const &verbose,
		      Id                        id,
		      Report_update_trigger    &report_update_trigger,
		      Xml_node                  start_node,
		      Default_route_accessor   &default_route_accessor,
		      Default_caps_accessor    &default_caps_accessor,
		      Name_registry            &name_registry,
		      Ram_quota                 ram_limit,
		      Cap_quota                 cap_limit,
		      Ram_limit_accessor       &ram_limit_accessor,
		      Cap_limit_accessor       &cap_limit_accessor,
		      Prio_levels               prio_levels,
		      Affinity::Space const    &affinity_space,
		      Registry<Parent_service> &parent_services,
		      Registry<Routed_service> &child_services,
		      Registry<Local_service>  &local_services);

		virtual ~Child();

		/**
		 * Return true if the child has the specified name
		 */
		bool has_name(Child_policy::Name const &str) const { return str == name(); }

		bool has_version(Version const &version) const { return version == _version; }

		Ram_quota ram_quota() const { return _resources.assigned_ram_quota; }
		Cap_quota cap_quota() const { return _resources.assigned_cap_quota; }

		void initiate_env_pd_session()
		{
			if (_state == STATE_INITIAL) {
				_child.initiate_env_pd_session();
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

		void destroy_services();

		void close_all_sessions() { _child.close_all_sessions(); }

		bool abandoned() const { return _state == STATE_ABANDONED; }

		bool env_sessions_closed() const { return _child.env_sessions_closed(); }

		enum Apply_config_result { MAY_HAVE_SIDE_EFFECTS, NO_SIDE_EFFECTS };

		/**
		 * Apply new configuration to child
		 *
		 * \throw Allocator::Out_of_memory  unable to allocate buffer for new
		 *                                  config
		 */
		Apply_config_result apply_config(Xml_node start_node);

		/* common code for upgrading RAM and caps */
		template <typename QUOTA, typename LIMIT_ACCESSOR>
		void _apply_resource_upgrade(QUOTA &, QUOTA, LIMIT_ACCESSOR const &);

		template <typename QUOTA, typename CHILD_AVAIL_QUOTA_FN>
		void _apply_resource_downgrade(QUOTA &, QUOTA, QUOTA,
                                       CHILD_AVAIL_QUOTA_FN const &);

		void apply_upgrade();
		void apply_downgrade();

		void heartbeat()
		{
			if (_heartbeat_expected())
				_child.heartbeat();

			unsigned const skipped_heartbeats = _child.skipped_heartbeats();

			if (_last_skipped_heartbeats != skipped_heartbeats)
				_report_update_trigger.trigger_report_update();

			_last_skipped_heartbeats = skipped_heartbeats;

		}

		unsigned skipped_heartbeats() const
		{
			return _heartbeat_expected() ? _child.skipped_heartbeats() : 0;
		}

		void report_state(Xml_generator &xml, Report_detail const &detail) const;


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Child_policy::Name name() const override { return _unique_name; }

		Pd_session           &ref_pd()           override { return _env.pd(); }
		Pd_session_capability ref_pd_cap() const override { return _env.pd_session_cap(); }

		void init(Pd_session  &, Pd_session_capability)  override;
		void init(Cpu_session &, Cpu_session_capability) override;

		Id_space<Parent::Server> &server_id_space() override {
			return _session_requester.id_space(); }

		Route resolve_session_request(Service::Name const &,
		                              Session_label const &) override;

		void     filter_session_args(Service::Name const &, char *, size_t) override;
		Affinity filter_session_affinity(Affinity const &) override;
		void     announce_service(Service::Name const &) override;
		void     resource_request(Parent::Resource_args const &) override;

		void exit(int exit_value) override
		{
			try {
				if (_start_node->xml().sub_node("exit").attribute_value("propagate", false)) {
					_env.parent().exit(exit_value);
					return;
				}
			} catch (...) { }

			/*
			 * Trigger a new report for exited children so that any management
			 * component may react upon it.
			 */
			_exited     = true;
			_exit_value = exit_value;

			_child.close_all_sessions();

			_report_update_trigger.trigger_report_update();

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

		void yield_response() override
		{
			apply_downgrade();
			_report_update_trigger.trigger_report_update();
		}
};

#endif /* _LIB__SANDBOX__CHILD_H_ */
