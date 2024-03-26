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
			bool matches(Xml_node const &node) const
			{
				return node.attribute_value("name", Path()) == name;
			}

			static bool type_matches(Xml_node const &node)
			{
				return node.has_type("file");
			}

			Launcher(Dict &dict, Path const &path) : Dict::Element(dict, path) { }
		};

		Dict _sorted { };

		List_model<Launcher> _launchers { };

	public:

		Launchers(Allocator &alloc) : _alloc(alloc) { }

		void update_from_xml(Xml_node const &launchers)
		{
			_launchers.update_from_xml(launchers,

				/* create */
				[&] (Xml_node const &node) -> Launcher & {
					return *new (_alloc)
						Launcher(_sorted,
						         node.attribute_value("name", Path())); },

				/* destroy */
				[&] (Launcher &e) { destroy(_alloc, &e); },

				/* update */
				[&] (Launcher &, Xml_node) { }
			);
		}

		void for_each(auto const &fn) const
		{
			_sorted.for_each([&] (Dict::Element const &e) { fn(Info(e.name)); });
		}
};

#endif /* _MODEL__LAUNCHERS_H_ */
