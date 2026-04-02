/*
 * \brief  Cached information about available options
 * \author Norman Feske
 * \date   2026-03-14
 */

/*
 * Copyright (C) 2026 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__OPTIONS_H_
#define _MODEL__OPTIONS_H_

/* local includes */
#include <model/file_listing.h>
#include <model/managed_children.h>
#include <vfs.h>

namespace Sculpt {

	struct Options : File_listing { using File_listing::File_listing; };

	class Enabled_options;
}


class Sculpt::Enabled_options
{
	private:

		struct Option;

		using Name = Options::Name;
		using Dict = Dictionary<Option, Name>;

		using Managed_dict = Managed_children::Dict;

	public:

		struct Action : Interface
		{
			virtual void deploy_option_changed(Name const &, Node const &) = 0;
		};

	private:

		Allocator &_alloc;

		static Name node_name(Node const &node)
		{
			return node.attribute_value("name", Name());
		}

		struct Option : Dict::Element, List_model<Option>::Element
		{
			using Dict::Element::Element;

			struct Watched_option
			{
				Action &_action;

				Name const &_name;

				Vfs::Handler<Watched_option> _handler;

				void _handle(Node const &option)
				{
					_action.deploy_option_changed(_name, option);
				}

				Watched_option(Vfs &vfs, Name const &name, Action &action)
				:
					_action(action), _name(name),
					_handler(vfs, Path { "/model/option/", name }, *this, &Watched_option::_handle)
				{ }
			};

			Constructible<Watched_option> _watched_option { };

			Managed_children children { };

			void watch(Vfs &vfs, Action &action)
			{
				if (!_watched_option.constructed())
					_watched_option.construct(vfs, name, action);
			}

			bool matches(Node const &node) const { return node_name(node) == name; }

			static bool type_matches(Node const &node)
			{
				return node.has_type("option");
			}
		};

		List_model<Option> _options { };

		Dict _dict { }; /* avoid iterating over '_options' in 'exists' */

		Managed_dict &_managed_dict;

	public:

		Enabled_options(Allocator &alloc, Managed_dict &managed_dict)
		: _alloc(alloc), _managed_dict(managed_dict) { }

		Progress update_from_deploy(Node const &deploy, Managed_dict &dict)
		{
			bool progress = false;
			_options.update_from_node(deploy,

				/* create */
				[&] (Node const &node) -> Option & {
					progress = true;
					return *new (_alloc) Option(_dict, node_name(node)); },

				/* destroy */
				[&] (Option &o) {
					progress = true;
					o.children.update_from_deploy_or_option(_alloc, dict, Node());
					destroy(_alloc, &o); },

				/* update */
				[&] (Option &, Node const &) { }
			);
			return Progress { progress };
		}

		void watch_options(Vfs &vfs, Action &action)
		{
			_options.for_each([&] (Option &option) { option.watch(vfs, action); });
		}

		Progress apply_option(Name const &name, Managed_dict &dict, Node const &node)
		{
			Progress result = STALLED;
			_options.for_each([&] (Option &option) {
				if (option.name == name)
					result = option.children.update_from_deploy_or_option(_alloc, dict, node);
			});
			return result;
		}

		bool exists(Name const &name) const { return _dict.exists(name); }
};

#endif /* _MODEL__OPTIONS_H_ */
