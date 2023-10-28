/*
 * \brief  State of the components hosted in the runtime subsystem
 * \author Norman Feske
 * \date   2018-08-22
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__RUNTIME_STATE_H_
#define _MODEL__RUNTIME_STATE_H_

/* Genode includes */
#include <util/xml_node.h>
#include <util/list_model.h>

/* local includes */
#include <types.h>
#include <runtime.h>
#include <model/runtime_config.h>
#include <model/component.h>

namespace Sculpt { class Runtime_state; }

class Sculpt::Runtime_state : public Runtime_info
{
	public:

		using Version = Child_state::Version;

		struct Info
		{
			bool selected;

			/* true if component is in the TCB of the selected one */
			bool tcb;

			/* true if 'tcb' is updated for the immediate dependencies */
			bool tcb_updated;

			unsigned long assigned_ram;
			unsigned long avail_ram;

			unsigned long assigned_caps;
			unsigned long avail_caps;

			Version version;
		};

	private:

		Allocator &_alloc;

		Storage_target const &_storage_target;

		struct Child : List_model<Child>::Element
		{
			Start_name const name;

			Info info { .selected      = false,
			            .tcb           = false,
			            .tcb_updated   = false,
			            .assigned_ram  = 0,
			            .avail_ram     = 0,
			            .assigned_caps = 0,
			            .avail_caps    = 0,
			            .version       = { 0 }};

			bool abandoned_by_user = false;

			Child(Start_name const &name) : name(name) { }

			bool matches(Xml_node const &node) const
			{
				return node.attribute_value("name", Start_name()) == name;
			}

			static bool type_matches(Xml_node const &node)
			{
				return node.has_type("child");
			}

			void update_from_xml(Xml_node const &node)
			{
				node.with_optional_sub_node("ram", [&] (Xml_node const &ram) {
					info.assigned_ram = max(ram.attribute_value("assigned", Number_of_bytes()),
					                        ram.attribute_value("quota",    Number_of_bytes()));
					info.avail_ram    =     ram.attribute_value("avail",    Number_of_bytes());
				});

				node.with_optional_sub_node("caps", [&] (Xml_node const &caps) {
					info.assigned_caps = max(caps.attribute_value("assigned", 0UL),
					                         caps.attribute_value("quota",    0UL));
					info.avail_caps    =     caps.attribute_value("avail",    0UL);
				});

				info.version.value = node.attribute_value("version", 0U);
			}
		};

		List_model<Child> _children { };

		bool _usb_in_tcb     = false;
		bool _storage_in_tcb = false;

		/**
		 * Child present in initial deploy config but interactively removed
		 */
		struct Abandoned_child : Interface
		{
			Start_name const name;

			Abandoned_child(Start_name const &name) : name(name) { };
		};

		Registry<Registered<Abandoned_child> > _abandoned_children { };

		/**
		 * Child that was interactively restarted
		 */
		struct Restarted_child : Interface
		{
			Start_name const name;

			Version version;

			Restarted_child(Start_name const &name, Version const &version)
			: name(name), version(version) { };
		};

		Registry<Registered<Restarted_child> > _restarted_children { };

		/**
		 * Child that was interactively launched
		 */
		struct Launched_child : Interface
		{
			Start_name const name;
			Path       const launcher;

			Constructible<Component> construction { };

			bool launched;

			/**
			 * Constructor used for child started via launcher
			 */
			Launched_child(Start_name const &name, Path const &launcher)
			: name(name), launcher(launcher), launched(true) { };

			/**
			 * Constructor used for interactively configured child
			 */
			Launched_child(Allocator &alloc, Start_name const &name,
			               Component::Path const &pkg_path,
			               Verify          const  verify,
			               Component::Info const &info,
			               Affinity::Space const space)
			:
				name(name), launcher(), launched(false)
			{
				construction.construct(alloc, name, pkg_path, verify, info, space);
			}

			void gen_deploy_start_node(Xml_generator &xml, Runtime_state const &state) const
			{
				if (!launched)
					return;

				gen_named_node(xml, "start", name, [&] () {

					Version const version = state.restarted_version(name);

					if (version.value > 0)
						xml.attribute("version", version.value);

					/* interactively constructed */
					if (construction.constructed()) {
						xml.attribute("pkg", construction->path);

						construction->gen_priority(xml);
						construction->gen_system_control(xml);
						construction->gen_affinity(xml);
						construction->gen_monitor(xml);

						xml.node("route", [&] () {
							construction->gen_pd_cpu_route(xml);

							construction->routes.for_each([&] (Route const &route) {
								route.gen_xml(xml); }); });
					}

					/* created via launcher */
					else {
						if (name != launcher)
							xml.attribute("launcher", launcher);
					}
				});
			}
		};

		Registry<Registered<Launched_child> > _launched_children { };

		Registered<Launched_child> *_currently_constructed = nullptr;

		bool _construction_in_progress() const
		{
			return _currently_constructed
			    && _currently_constructed->construction.constructed();
		}

		/*
		 * Noncopyable
		 */
		Runtime_state(Runtime_state const &);
		Runtime_state &operator = (Runtime_state const &);

	public:

		Runtime_state(Allocator &alloc, Storage_target const &storage_target)
		: _alloc(alloc), _storage_target(storage_target) { }

		~Runtime_state() { reset_abandoned_and_launched_children(); }

		void update_from_state_report(Xml_node state)
		{
			_children.update_from_xml(state,

				/* create */
				[&] (Xml_node const &node) -> Child & {
					return *new (_alloc)
						Child(node.attribute_value("name", Start_name())); },

				/* destroy */
				[&] (Child &child) { destroy(_alloc, &child); },

				/* update */
				[&] (Child &child, Xml_node const &node) {
					child.update_from_xml(node); }
			);
		}

		/**
		 * Runtime_info interface
		 */
		bool present_in_runtime(Start_name const &name) const override
		{
			bool result = false;

			_children.for_each([&] (Child const &child) {
				if (!result && child.name == name)
					result = true; });

			_launched_children.for_each([&] (Launched_child const &child) {
				if (!result && child.name == name && child.launched)
					result = true; });

			return result;
		}

		/**
		 * Runtime_info interface
		 */
		bool abandoned_by_user(Start_name const &name) const override
		{
			bool result = false;

			_abandoned_children.for_each([&] (Abandoned_child const &child) {
				if (!result && child.name == name)
					result = true; });

			return result;
		}

		/**
		 * Return version of restarted child
		 *
		 * If the returned value is version 0, the child has not been
		 * restarted.
		 */
		Version restarted_version(Start_name const &name) const override
		{
			Version result { 0 };

			_restarted_children.for_each([&] (Restarted_child const &child) {
				if (!result.value && child.name == name)
					result = child.version; });

			return result;
		}

		/**
		 * Runtime_info interface
		 */
		void gen_launched_deploy_start_nodes(Xml_generator &xml) const override
		{
			_launched_children.for_each([&] (Launched_child const &child) {
				child.gen_deploy_start_node(xml, *this); });
		}

		Info info(Start_name const &name) const
		{
			Info result { };
			_children.for_each([&] (Child const &child) {
				if (child.name == name)
					result = child.info; });
			return result;
		}

		Start_name selected() const
		{
			Start_name result;
			_children.for_each([&] (Child const &child) {
				if (child.info.selected)
					result = child.name; });
			return result;
		}

		bool usb_in_tcb()     const { return _usb_in_tcb; }
		bool storage_in_tcb() const { return _storage_in_tcb; }

		static bool blacklisted_from_graph(Start_name const &name)
		{
			/*
			 * Connections to depot_rom do not reveal any interesting
			 * information but create a lot of noise.
			 */
			return name == "depot_rom" || name == "dynamic_depot_rom";
		}

		void toggle_selection(Start_name const &name, Runtime_config const &config)
		{
			_children.for_each([&] (Child &child) {
				child.info.selected    = (child.name == name) && !child.info.selected;
				child.info.tcb         = child.info.selected;
				child.info.tcb_updated = false;
			});

			_usb_in_tcb = _storage_in_tcb = false;

			/*
			 * Update the TCB flag of the selected child's transitive
			 * dependencies.
			 */
			for (;;) {

				Start_name name_of_updated { };

				/*
				 * Search child that belongs to TCB but its dependencies
				 * have not been added to the TCB yet.
				 */
				_children.for_each([&] (Child &child) {
					if (!name_of_updated.valid() && child.info.tcb && !child.info.tcb_updated) {
						name_of_updated = child.name;
						child.info.tcb_updated = true; /* skip in next iteration */
					}
				});

				if (!name_of_updated.valid())
					break;

				/* tag all dependencies as part of the TCB */
				config.for_each_dependency(name_of_updated, [&] (Start_name dep) {

					if (dep == "default_fs_rw")
						dep = _storage_target.fs();

					if (!blacklisted_from_graph(dep))
						_children.for_each([&] (Child &child) {
							if (child.name == dep)
								child.info.tcb = true; });
				});
			}

			_children.for_each([&] (Child const &child) {
				if (child.info.tcb) {
					config.for_each_dependency(child.name, [&] (Start_name dep) {
						if (dep == "usb")     _usb_in_tcb     = true;
						if (dep == "storage") _storage_in_tcb = true;
					});
				}
			});
		}

		void abandon(Start_name const &name)
		{
			/*
			 * If child was launched interactively, remove corresponding
			 * entry from '_launched_children'.
			 */
			bool was_interactively_launched = false;
			_launched_children.for_each([&] (Registered<Launched_child> &child) {
				if (child.name == name) {
					was_interactively_launched = true;
					destroy(_alloc, &child);
				}
			});

			if (was_interactively_launched)
				return;

			/*
			 * Child was present at initial deploy config, mark as abandoned
			 */
			new (_alloc) Registered<Abandoned_child>(_abandoned_children, name);
		}

		void restart(Start_name const &name)
		{
			/* determin current version from most recent state report */
			Version current_version { 0 };
			_children.for_each([&] (Child const &child) {
				if (child.name == name)
					current_version = child.info.version; });

			Version const next_version { current_version.value + 1 };

			bool already_restarted = false;

			_restarted_children.for_each([&] (Restarted_child &child) {
				if (child.name == name) {
					already_restarted = true;
					child.version = next_version;
				}
			});

			if (!already_restarted)
				new (_alloc)
					Registered<Restarted_child>(_restarted_children, name, next_version);
		}

		void launch(Start_name const &name, Path const &launcher)
		{
			new (_alloc) Registered<Launched_child>(_launched_children, name, launcher);
		}

		Start_name new_construction(Component::Path const  pkg,
		                            Verify          const  verify,
		                            Component::Info const &info,
		                            Affinity::Space const  space)
		{
			/* allow only one construction at a time */
			discard_construction();

			/* determine unique name for new child */
			Depot::Archive::Name const archive_name = Depot::Archive::name(pkg);
			Start_name unique_name = archive_name;
			unsigned cnt = 1;
			while (present_in_runtime(unique_name))
				unique_name = Start_name(archive_name, ".", ++cnt);

			_currently_constructed = new (_alloc)
				Registered<Launched_child>(_launched_children, _alloc,
				                           unique_name, pkg, verify, info, space);
			return unique_name;
		}

		void discard_construction()
		{
			if (_currently_constructed) {
				destroy(_alloc, _currently_constructed);
				_currently_constructed = nullptr;
			}
		}

		template <typename FN>
		void apply_to_construction(FN const &fn)
		{
			if (_construction_in_progress())
				fn(*_currently_constructed->construction);
		}

		template <typename FN>
		void with_construction(FN const &fn) const
		{
			if (_construction_in_progress())
				fn(*_currently_constructed->construction);
		}

		void launch_construction()
		{
			if (_currently_constructed)
				_currently_constructed->launched = true;

			_currently_constructed = nullptr;
		}

		void reset_abandoned_and_launched_children()
		{
			/*
			 * Invalidate '_currently_constructed' pointer, which may point
			 * to a to-be-destructed 'Launched_child'.
			 */
			discard_construction();

			_abandoned_children.for_each([&] (Abandoned_child &child) {
				destroy(_alloc, &child); });

			_launched_children.for_each([&] (Launched_child &child) {
				destroy(_alloc, &child); });

			_restarted_children.for_each([&] (Restarted_child &child) {
				destroy(_alloc, &child); });
		}
};

#endif /* _MODEL__RUNTIME_STATE_H_ */
