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
		};

	private:

		Allocator &_alloc;

		Storage_target const &_storage_target;

		struct Child : List_model<Child>::Element
		{
			Start_name const name;

			Info info { .selected    = false,
			            .tcb         = false,
			            .tcb_updated = false,
			            .assigned_ram  = 0, .avail_ram  = 0,
			            .assigned_caps = 0, .avail_caps = 0 };

			bool abandoned_by_user = false;

			Child(Start_name const &name) : name(name) { }
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
			               Component::Info const &info)
			:
				name(name), launcher(), launched(false)
			{
				construction.construct(alloc, pkg_path, info);
			}

			void gen_deploy_start_node(Xml_generator &xml) const
			{
				if (!launched)
					return;

				gen_named_node(xml, "start", name, [&] () {

					/* interactively constructed */
					if (construction.constructed()) {
						xml.attribute("pkg", construction->path);

						xml.node("route", [&] () {
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

		struct Update_policy : List_model<Child>::Update_policy
		{
			Allocator &_alloc;

			Update_policy(Allocator &alloc) : _alloc(alloc) { }

			void destroy_element(Child &elem)
			{
				destroy(_alloc, &elem);
			}

			Child &create_element(Xml_node node)
			{
				return *new (_alloc)
					Child(node.attribute_value("name", Start_name()));
			}

			void update_element(Child &child, Xml_node node)
			{
				if (node.has_sub_node("ram")) {
					Xml_node const ram = node.sub_node("ram");
					child.info.assigned_ram = max(ram.attribute_value("assigned", Number_of_bytes()),
					                              ram.attribute_value("quota",    Number_of_bytes()));
					child.info.avail_ram    = ram.attribute_value("avail",    Number_of_bytes());
				}

				if (node.has_sub_node("caps")) {
					Xml_node const caps = node.sub_node("caps");
					child.info.assigned_caps = max(caps.attribute_value("assigned", 0UL),
					                               caps.attribute_value("quota",    0UL));
					child.info.avail_caps    = caps.attribute_value("avail",    0UL);
				}
			}

			static bool element_matches_xml_node(Child const &elem, Xml_node node)
			{
				return node.attribute_value("name", Start_name()) == elem.name;
			}

			static bool node_is_element(Xml_node node) { return node.has_type("child"); }
		};

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
			Update_policy policy(_alloc);
			_children.update_from_xml(policy, state);
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
		 * Runtime_info interface
		 */
		void gen_launched_deploy_start_nodes(Xml_generator &xml) const override
		{
			_launched_children.for_each([&] (Launched_child const &child) {
				child.gen_deploy_start_node(xml); });
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

		void launch(Start_name const &name, Path const &launcher)
		{
			new (_alloc) Registered<Launched_child>(_launched_children, name, launcher);
		}

		Start_name new_construction(Component::Path const pkg,
		                                      Component::Info const &info)
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
				                           unique_name, pkg, info);
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
		}
};

#endif /* _MODEL__RUNTIME_STATE_H_ */
