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

		struct Budget
		{
			size_t assigned, avail, requested;

			size_t wanted() const { return requested ? assigned + requested : 0ul; }

			bool operator != (Budget const &other) const
			{
				return assigned   != other.assigned
				    || avail      != other.avail
				    || requested  != other.requested;
			}

			static Budget from_node(Node const &node, auto default_value)
			{
				return {
					.assigned  = max(node.attribute_value("assigned",  default_value),
					                 node.attribute_value("quota",     default_value)),
					.avail     =     node.attribute_value("avail",     default_value),
					.requested =     node.attribute_value("requested", default_value)
				};
			}
		};

		struct Ram  : Budget { using Budget::operator=; };
		struct Caps : Budget { using Budget::operator=; };

		struct Info
		{
			Ram  ram;
			Caps caps;

			bool operator != (Info const &other) const
			{
				return ram != other.ram || caps != other.caps;
			}
		};

		struct Resource_request { Ram ram; Caps caps; };

	private:

		Allocator &_alloc;

		struct Child : List_model<Child>::Element
		{
			Start_name const name;

			Info info { };

			Child(Start_name const &name) : name(name) { }

			bool matches(Node const &node) const
			{
				return node.attribute_value("name", Start_name()) == name;
			}

			static bool type_matches(Node const &node)
			{
				return node.has_type("child");
			}

			Progress update_from_node(Node const &node)
			{
				Info const orig_info = info;
				node.with_optional_sub_node("ram", [&] (Node const &ram) {
					info.ram = Budget::from_node(ram, Number_of_bytes()); });

				node.with_optional_sub_node("caps", [&] (Node const &caps) {
					info.caps = Budget::from_node(caps, 0UL); });

				return { info != orig_info };
			}
		};

		List_model<Child> _children { };

		struct Emerging_child : Interface
		{
			Start_name const name;

			Constructible<Component> construction { };

			Emerging_child(Allocator &alloc, Start_name const &name,
			               Component::Path const &pkg_path,
			               Verify          const  verify,
			               Component::Info const &info,
			               Affinity::Space const space)
			:
				name(name)
			{
				construction.construct(alloc, name, pkg_path, verify, info, space);
			}

			void gen_deploy_child_node(Generator &g) const
			{
				gen_named_node(g, "child", name, [&] {

					/* interactively constructed */
					if (construction.constructed()) {
						g.attribute("pkg", construction->path);

						construction->gen_priority(g);
						construction->gen_system_control(g);
						construction->gen_affinity(g);
						construction->gen_monitor(g);

						g.tabular_node("connect", [&] {
							construction->gen_pd_cpu_connection(g);

							construction->connect.for_each([&] (Connection const &conn) {
								conn.generate(g); }); });
					}
				});
			}
		};

		Constructible<Emerging_child> _emerging { };

		bool _construction_in_progress() const
		{
			return _emerging.constructed()
			    && _emerging->construction.constructed();
		}

	public:

		Runtime_state(Allocator &alloc) : _alloc(alloc) { }

		~Runtime_state() { }

		Progress update_from_state_report(Node const &state)
		{
			Progress result = STALLED;
			_children.update_from_node(state,

				/* create */
				[&] (Node const &node) -> Child & {
					result = PROGRESSED;
					return *new (_alloc)
						Child(node.attribute_value("name", Start_name())); },

				/* destroy */
				[&] (Child &child) {
					result = PROGRESSED;
					destroy(_alloc, &child); },

				/* update */
				[&] (Child &child, Node const &node) {
					if (child.update_from_node(node).progressed)
						result = PROGRESSED; }
			);
			return result;
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

			return result;
		}

		Info info(Start_name const &name) const
		{
			Info result { };
			_children.for_each([&] (Child const &child) {
				if (child.name == name)
					result = child.info; });
			return result;
		}

		void for_each_resource_request(auto const &fn) const
		{
			_children.for_each([&] (Child const &child) {
				if (child.info.ram.requested || child.info.caps.requested)
					fn(child.name,
					   Resource_request { child.info.ram, child.info.caps }); });
		}

		Start_name new_construction(Component::Path const  pkg,
		                            Verify          const  verify,
		                            Component::Info const &info,
		                            Affinity::Space const  space)
		{
			_emerging.destruct();

			return Depot::Archive::name(pkg).convert<Start_name>(
				[&] (Depot::Archive::Name const &archive_name) {

					/* determine unique name for new child */
					Start_name unique_name = archive_name;
					unsigned cnt = 1;
					while (present_in_runtime(unique_name))
						unique_name = Start_name(archive_name, ".", ++cnt);

					_emerging.construct(_alloc, unique_name, pkg, verify, info, space);

					return unique_name;
				},
				[&] (Depot::Archive::Unknown) {
					warning("pkg has unknown name: ", pkg);
					return Start_name { };
				});
		}

		void reset_construction()
		{
			_emerging.destruct();
		}

		void apply_to_construction(auto const &fn)
		{
			if (_construction_in_progress())
				fn(*_emerging->construction);
		}

		void with_construction(auto const &fn) const
		{
			if (_construction_in_progress())
				fn(*_emerging->construction);
		}

		void gen_construction(Generator &g) const
		{
			if (_emerging.constructed())
				_emerging->gen_deploy_child_node(g);
		}
};

#endif /* _MODEL__RUNTIME_STATE_H_ */
