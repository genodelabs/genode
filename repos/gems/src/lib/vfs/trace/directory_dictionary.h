/*
 * \brief  A dictionary of dictionaries that form a directory structure
 * \author Sebastian Sumpf
 * \date   2023-03-09
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DIRECTORY_DICTIONARY_H_
#define _DIRECTORY_DICTIONARY_H_

#include <util/dictionary.h>
#include "session_label.h"

namespace Vfs {
	class Label;
	class Trace_node;
	class Trace_directory;

	using Dictionary = Genode::Dictionary<Vfs::Trace_node, Vfs::Label>;
};


struct Vfs::Label : Genode::String<32>
{
	using String::String;
};


class Vfs::Trace_node : public Dictionary::Element
{
	private:

		Allocator              &_alloc;
		Trace::Subject_id const _id;
		Dictionary              _dict { };

		/* add version to duplicates (like "idle" or "cross" */
		Label _thread_name(Session_label const &thread_name)
		{
			if (!_dict.exists(thread_name)) return Label(thread_name);

			unsigned version { 0 };
			while (true) {
				Label label { thread_name, ".", ++version };
				if (!_dict.exists(label)) return label;
			}
		}

	public:

		Trace_node(Allocator &alloc, Dictionary &dict, Session_label const &label,
		           Trace::Subject_id const id = { })
		:
		  Dictionary::Element(dict, Label(label)),
		  _alloc(alloc), _id(id)
		{ }

		void insert(Session_label     const &label,
		            Session_label     const &thread_name,
		            Trace::Subject_id const id)
		{
			/* leaf node -> thread_name<.version> */
			if (!label.valid()) {
					new (_alloc) Trace_node(_alloc, _dict, _thread_name(thread_name), id);
				return;
			}

			_dict.with_element(label.first_element(),
				[&] (Trace_node &node) {
					node.insert(label.suffix(), thread_name, id); },
				[&]() {
					/* no match add first element of label to this dict */
					Trace_node *node = new (_alloc) Trace_node(_alloc, _dict, label.first_element());
					node->insert(label.suffix(), thread_name, id);
				}
			);
		}

		void xml(Xml_generator &xml) const
		{
			_dict.for_each([&] (Trace_node const &node) {
				if (node.id().id == 0)
					xml.node("dir", [&] () {
						xml.attribute("name", node.name);
						node.xml(xml);
					});
				else
					xml.node("trace_node", [&] () {
						xml.attribute("name", node.name);
						xml.attribute("id", node.id().id);
						node.xml(xml);
					});
			});
		}

		Trace::Subject_id const &id() const { return _id; }
};


class Vfs::Trace_directory
{
	private:

		Allocator &_alloc;
		Dictionary _root_dict { };
		Trace_node _root { _alloc, _root_dict, Session_label() };

	public:

		Trace_directory(Allocator &alloc) : _alloc(alloc) { }

		void insert(Trace::Subject_info const &info, Trace::Subject_id const id)
		{
			_root.insert(info.session_label(), info.thread_name(), id);
		}

		void xml(Xml_generator &xml)
		{
			_root.xml(xml);
		}
};

#endif /* _DIRECTORY_DICTIONARY_H_ */
