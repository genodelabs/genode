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
#include <util/dictionary.h>

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

		struct Launcher;

		using Dict = Dictionary<Launcher, Path>;

		struct Launcher : Dict::Element, List_model<Launcher>::Element
		{
			Launcher(Dict &dict, Path const &path) : Dict::Element(dict, path) { }
		};

		Dict _sorted { };

		List_model<Launcher> _launchers { };

		struct Update_policy : List_model<Launcher>::Update_policy
		{
			Allocator &_alloc;

			Dict &_sorted;

			Update_policy(Allocator &alloc, Dict &sorted)
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
				return node.attribute_value("name", Path()) == elem.name;
			}
		};

	public:

		Launchers(Allocator &alloc) : _alloc(alloc) { }

		void update_from_xml(Xml_node const &node)
		{
			Update_policy policy(_alloc, _sorted);
			_launchers.update_from_xml(policy, node);
		}

		template <typename FN>
		void for_each(FN const &fn) const
		{
			_sorted.for_each([&] (Dict::Element const &e) {
				fn(Info(e.name)); });
		}
};

#endif /* _MODEL__LAUNCHERS_H_ */
