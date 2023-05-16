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
#include <sandbox/sandbox.h>

/* local includes */
#include <types.h>
#include <verbose.h>
#include <report.h>
#include <name_registry.h>
#include <service.h>
#include <utils.h>
#include <route_model.h>

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
		typedef Resource_limit_accessor<Cpu_quota> Cpu_limit_accessor;

		struct Cpu_quota_transfer : Interface
		{
			virtual void transfer_cpu_quota(Capability<Pd_session>, Pd_session &,
			                                Capability<Cpu_session>, Cpu_quota) = 0;
		};

		enum class Sample_state_result { CHANGED, UNCHANGED };

		/*
		 * Helper for passing lambda functions as 'Pd_intrinsics::Fn'
		 */

		using Pd_intrinsics = Genode::Sandbox::Pd_intrinsics;

		template <typename PD_SESSION, typename FN>
		static void with_pd_intrinsics(Pd_intrinsics &pd_intrinsics,
		                               Capability<Pd_session> cap, PD_SESSION &pd,
		                               FN const &fn)
		{
			struct Impl : Pd_intrinsics::Fn
			{
				using Intrinsics = Pd_intrinsics::Intrinsics;

				FN const &_fn;
				Impl(FN const &fn) : _fn(fn) { }
				void call(Intrinsics &intrinsics) const override { _fn(intrinsics); }
			};

			pd_intrinsics.with_intrinsics(cap, pd, Impl { fn });
		}

	private:

		friend class Child_registry;

		Env &_env;

		Allocator &_alloc;

		Verbose const &_verbose;

		Id const _id;

		enum class State {

			/*
			 * States modelling the child's boostrap phase
			 */
			INITIAL, RAM_INITIALIZED, ALIVE,

			/*
			 * The child is present in the config model but its bootstrapping
			 * permanently failed.
			 */
			STUCK,

			/*
			 * The child must be restarted because a fundamental dependency
			 * changed. While the child is in this state, it is still
			 * referenced by the config model.
			 */
			RESTART_SCHEDULED,

			/*
			 * The child is no longer referenced by config model and can
			 * safely be destructed.
			 */
			ABANDONED
		};

		State _state = State::INITIAL;

		Report_update_trigger &_report_update_trigger;

		List_element<Child> _list_element;

		Reconstructible<Buffered_xml> _start_node;

		Constructible<Route_model> _route_model { };

		void _construct_route_model_from_start_node(Xml_node const &start)
		{
			_route_model.destruct();

			start.with_sub_node("route",
				[&] (Xml_node const &route) {
					_route_model.construct(_alloc, route); },
				[&] () {
					_route_model.construct(_alloc, _default_route_accessor.default_route()); });
		}

		/*
		 * Version attribute of the start node, used to force child restarts.
		 */
		Version _version { _start_node->xml().attribute_value("version", Version()) };

		bool _uncertain_dependencies = false;

		/*
		 * True if the binary is loaded with ld.lib.so
		 */
		bool const _use_ld = _start_node->xml().attribute_value("ld", true);

		Default_route_accessor &_default_route_accessor;
		Default_caps_accessor  &_default_caps_accessor;
		Ram_limit_accessor     &_ram_limit_accessor;
		Cap_limit_accessor     &_cap_limit_accessor;
		Cpu_limit_accessor     &_cpu_limit_accessor;
		Cpu_quota_transfer     &_cpu_quota_transfer;

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

			warning("missing 'name' attribute in '<start>' entry");
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
			return _heartbeat_enabled && (_state == State::ALIVE);
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
			Cpu_quota assigned_cpu_quota;

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

		static
		Resources _resources_from_start_node(Xml_node start_node, Prio_levels prio_levels,
		                                     Affinity::Space const &affinity_space,
		                                     Cap_quota default_cap_quota)
		{
			unsigned cpu_percent = 0;
			Number_of_bytes ram_bytes = 0;

			size_t caps = start_node.attribute_value("caps", default_cap_quota.value);

			start_node.for_each_sub_node("resource", [&] (Xml_node rsc) {

				typedef String<8> Name;
				Name const name = rsc.attribute_value("name", Name());

				if (name == "RAM")
					ram_bytes = rsc.attribute_value("quantum", ram_bytes);

				if (name == "CPU")
					cpu_percent = rsc.attribute_value("quantum", 0U);

				if (name == "CAP")
					caps = rsc.attribute_value("quantum", 0UL);
			});

			return Resources { log2(prio_levels.value),
			                   priority_from_xml(start_node, prio_levels),
			                   Affinity(affinity_space,
			                            affinity_location_from_xml(affinity_space, start_node)),
			                   Ram_quota { ram_bytes },
			                   Cap_quota { caps },
			                   Cpu_quota { cpu_percent } };
		}

		Resources _resources;

		Ram_quota _configured_ram_quota() const;
		Cap_quota _configured_cap_quota() const;

		Pd_intrinsics &_pd_intrinsics;

		template <typename FN>
		void _with_pd_intrinsics(FN const &fn)
		{
			with_pd_intrinsics(_pd_intrinsics, _child.pd_session_cap(), _child.pd(), fn);
		}

		Capability<Pd_session> _ref_pd_cap { }; /* defined by 'init' */

		using Local_service = Genode::Sandbox::Local_service_base;

		Registry<Parent_service> &_parent_services;
		Registry<Routed_service> &_child_services;
		Registry<Local_service>  &_local_services;

		struct Inline_config_rom_service : Abandonable, Dynamic_rom_session::Content_producer
		{
			typedef Genode::Local_service<Dynamic_rom_session> Service;

			Child &_child;

			Dynamic_rom_session _session { _child._env.ep().rpc_ep(),
			                               _child._env.ram(), _child._env.rm(),
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

		Cpu_quota const _effective_cpu_quota {
			min(_cpu_limit_accessor.resource_limit(Cpu_quota{}).percent,
			    _resources.assigned_cpu_quota.percent) };

		/**
		 * If set to true, the child is allowed to do system management,
		 * e.g., constrain physical RAM allocations.
		 */
		bool const _managing_system {
			_start_node->xml().attribute_value("managing_system", false) };

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

		enum class Route_state { VALID, MISMATCH, UNAVAILABLE };

		/**
		 * Return true if the policy results in the current route of the session
		 *
		 * This method is used to check if a policy change affects an existing
		 * client session of a child, i.e., to determine whether the child must
		 * be restarted.
		 */
		Route_state _route_valid(Session_state const &session)
		{
			try {
				Route const route =
					resolve_session_request(session.service().name(),
					                        session.client_label(),
					                        session.diag());

				bool const valid = (session.service() == route.service)
				                && (route.label == session.label());

				return valid ? Route_state::VALID : Route_state::MISMATCH;
			}
			catch (Service_denied) { return Route_state::UNAVAILABLE; }
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

			return exists && !abandoned() && !restart_scheduled();
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

		/**
		 * Return true if it's safe to call the PD for requesting resource
		 * information
		 */
		bool _pd_alive() const
		{
			return !abandoned() && !restart_scheduled() && !_exited;
		}

		void _destroy_services();

		struct Sampled_state
		{
			Ram_info ram;
			Cap_info caps;

			static Sampled_state from_pd(Pd_session &pd)
			{
				return { .ram  = Ram_info::from_pd(pd),
				         .caps = Cap_info::from_pd(pd) };
			}

			bool operator != (Sampled_state const &other) const
			{
				return (ram != other.ram) || (caps != other.caps);
			}

		} _sampled_state { };

		void _abandon_services()
		{
			_child_services.for_each([&] (Routed_service &service) {
				if (service.has_id_space(_session_requester.id_space()))
					service.abandon(); });
		}

		void _schedule_restart()
		{
			_state = State::RESTART_SCHEDULED;
			_abandon_services();
		}

	public:

		/**
		 * Constructor
		 *
		 * \param alloc               allocator solely used for configuration-
		 *                            dependent allocations. It is not used for
		 *                            allocations on behalf of the child's
		 *                            behavior.
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
		      Ram_limit_accessor       &ram_limit_accessor,
		      Cap_limit_accessor       &cap_limit_accessor,
		      Cpu_limit_accessor       &cpu_limit_accessor,
		      Cpu_quota_transfer       &cpu_quota_transfer,
		      Prio_levels               prio_levels,
		      Affinity::Space const    &affinity_space,
		      Registry<Parent_service> &parent_services,
		      Registry<Routed_service> &child_services,
		      Registry<Local_service>  &local_services,
		      Pd_intrinsics            &pd_intrinsics);

		virtual ~Child();

		/**
		 * Return true if the child has the specified name
		 */
		bool has_name(Child_policy::Name const &str) const { return str == name(); }

		bool has_version(Version const &version) const { return version == _version; }

		Ram_quota ram_quota() const { return _resources.assigned_ram_quota; }
		Cap_quota cap_quota() const { return _resources.assigned_cap_quota; }
		Cpu_quota cpu_quota() const { return _effective_cpu_quota; }

		void try_start()
		{
			if (_state == State::INITIAL) {
				_child.initiate_env_pd_session();
				_state = State::RAM_INITIALIZED;
			}

			/*
			 * Update the state if async env sessions have brought the child to
			 * life. Otherwise, we would wrongly call 'initiate_env_sessions()'
			 * another time.
			 */
			if (_state == State::RAM_INITIALIZED && _child.active())
				_state = State::ALIVE;

			if (_state == State::RAM_INITIALIZED) {
				_child.initiate_env_sessions();

				if (_child.active())
					_state = State::ALIVE;
				else
					_uncertain_dependencies = true;
			}
		}

		/*
		 * Mark child as to be removed because its was dropped from the
		 * config model. Either <start> node disappeared or 'restart_scheduled'
		 * was handled.
		 */
		void abandon()
		{
			_state = State::ABANDONED;
			_abandon_services();
		}

		void destroy_services();

		void close_all_sessions() { _child.close_all_sessions(); }

		bool abandoned() const { return _state == State::ABANDONED; }

		bool restart_scheduled() const { return _state == State::RESTART_SCHEDULED; }

		bool stuck() const { return _state == State::STUCK; }

		bool env_sessions_closed() const { return _child.env_sessions_closed(); }

		enum Apply_config_result { PROVIDED_SERVICES_CHANGED, NO_SIDE_EFFECTS };

		/**
		 * Apply new configuration to child
		 *
		 * \throw Allocator::Out_of_memory  unable to allocate buffer for new
		 *                                  config
		 */
		Apply_config_result apply_config(Xml_node start_node);

		bool uncertain_dependencies() const { return _uncertain_dependencies; }

		/**
		 * Validate that the routes of all existing sessions remain intact
		 *
		 * The child may become scheduled for restart or get stuck.
		 */
		void evaluate_dependencies();

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

		void report_state(Xml_generator &, Report_detail const &) const;

		Sample_state_result sample_state();


		/****************************
		 ** Child-policy interface **
		 ****************************/

		Child_policy::Name name() const override { return _unique_name; }

		Pd_session &ref_pd() override
		{
			Pd_session *_ref_pd_ptr = nullptr;
			_with_pd_intrinsics([&] (Pd_intrinsics::Intrinsics &intrinsics) {
				_ref_pd_ptr = &intrinsics.ref_pd; });
			return *_ref_pd_ptr;
		}

		Pd_session_capability ref_pd_cap() const override { return _ref_pd_cap; }

		Ram_allocator &session_md_ram() override { return _env.ram(); }

		void init(Pd_session  &, Pd_session_capability)  override;
		void init(Cpu_session &, Cpu_session_capability) override;

		Id_space<Parent::Server> &server_id_space() override {
			return _session_requester.id_space(); }

		Route resolve_session_request(Service::Name const &,
		                              Session_label const &, Session::Diag) override;

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

			_report_update_trigger.trigger_immediate_report_update();

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

		void _with_address_space(Pd_session &, With_address_space_fn const &fn) override
		{
			_with_pd_intrinsics([&] (Pd_intrinsics::Intrinsics &intrinsics) {
				fn.call(intrinsics.address_space); });
		}

		void start_initial_thread(Capability<Cpu_thread> cap, addr_t ip) override
		{
			_pd_intrinsics.start_initial_thread(cap, ip);
		}

		void yield_response() override
		{
			apply_downgrade();
			_report_update_trigger.trigger_report_update();
		}
};

#endif /* _LIB__SANDBOX__CHILD_H_ */
