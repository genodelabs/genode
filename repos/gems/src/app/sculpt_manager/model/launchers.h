/*
 * \brief  Cached information about available launchers
 * \author Norman Feske
 * \date   2018-09-13
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__LAUNCHERS_H_
#define _MODEL__LAUNCHERS_H_

/* Genode includes */
#include <util/list_model.h>
#include <util/avl_tree.h>

/* local includes */
#include <types.h>
#include <runtime.h>

namespace Sculpt { class Launchers; }


class Sculpt::Launchers : public Noncopyable
{
	public:

		struct Info : Noncopyable
		{
			Path const path;
			Info(Path const &path) : path(path) { }
		};

	private:

		Allocator &_alloc;

		struct Launcher : Info, Avl_node<Launcher>, List_model<Launcher>::Element
		{
			Avl_tree<Launcher> &_avl_tree;

			Launcher(Avl_tree<Launcher> &avl_tree, Path const &path)
			: Info(path), _avl_tree(avl_tree)
			{ _avl_tree.insert(this); }

			~Launcher() { _avl_tree.remove(this); }

			/**
			 * Avl_node interface
			 */
			bool higher(Launcher *l) {
				return strcmp(l->path.string(), path.string()) > 0; }
		};

		Avl_tree<Launcher> _sorted { };

		List_model<Launcher> _launchers { };

		struct Update_policy : List_model<Launcher>::Update_policy
		{
			Allocator &_alloc;

			Avl_tree<Launcher> &_sorted;

			Update_policy(Allocator &alloc, Avl_tree<Launcher> &sorted)
			: _alloc(alloc), _sorted(sorted) { }

			void destroy_element(Launcher &elem) { destroy(_alloc, &elem); }

			Launcher &create_element(Xml_node node)
			{
				return *new (_alloc)
					Launcher(_sorted, node.attribute_value("name", Path()));
			}

			void update_element(Launcher &, Xml_node) { }

			static bool element_matches_xml_node(Launcher const &elem, Xml_node node)
			{
				return node.attribute_value("name", Path()) == elem.path;
			}
		};

	public:

		Launchers(Allocator &alloc) : _alloc(alloc) { }

		void update_from_xml(Xml_node node)
		{
			Update_policy policy(_alloc, _sorted);
			_launchers.update_from_xml(policy, node);
		}

		template <typename FN>
		void for_each(FN const &fn) const { _sorted.for_each(fn); }
};

#endif /* _MODEL__LAUNCHERS_H_ */
