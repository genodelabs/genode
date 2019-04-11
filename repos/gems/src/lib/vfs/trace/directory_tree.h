/*
 * \brief  A tree of AVL trees that forms a directory structure
 * \author Sebastian Sumpf
 * \date   2019-06-14
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _DIRECTORY_TREE_H_
#define _DIRECTORY_TREE_H_

#include <util/avl_string.h>
#include "session_label.h"


namespace Vfs {
	class  Directory_tree;
	class  Trace_node;
	struct Label;
}


namespace Genode {
	template <typename> class Avl_node_tree;
}


template <typename NT>
class Genode::Avl_node_tree : public NT
{
	protected:

		using Tree = Avl_tree<NT>;
		using Node = Avl_node<NT>;

		Tree _tree { };

	public:

		using NT::NT;

		void insert(Node *node) { _tree.insert(node); }

		Tree &tree() { return _tree; }

		Node *find_by_name(char const *name)
		{
			if (!_tree.first()) return nullptr;

			Node *node = _tree.first()->find_by_name(name);
			return node;
		}
};


struct Vfs::Label : Genode::String<32>
{
	using String::String;
};


class Vfs::Trace_node : public Vfs::Label,
                        public Avl_node_tree<Genode::Avl_string_base>
{
	private:

		Allocator              &_alloc;
		Trace::Subject_id const _id;

		Trace_node *_find_by_name(char const *name)
		{
			Node *node = find_by_name(name);
			return node ? static_cast<Trace_node *>(node) : nullptr;
		}

	public:

		Trace_node(Genode::Allocator &alloc, Session_label const &label,
		            Trace::Subject_id const id = 0)
		: Vfs::Label(label), Avl_node_tree(string()),
		  _alloc(alloc), _id(id)
		{ }

		Trace_node &insert(Session_label const &label)
		{
			if (!label.valid()) return *this;

			Trace_node *node = _find_by_name(label.first_element().string());
			if (!node) {
				node = new(_alloc) Trace_node(_alloc, label.first_element());
				Avl_node_tree::insert(node);
			}

			return node->insert(label.suffix());
		}

		void xml(Genode::Xml_generator &xml) const
		{
			_tree.for_each([&] (Genode::Avl_string_base const &name) {
				Trace_node const &node = static_cast<Trace_node const &>(name);

				if (node.id() == 0)
					xml.node("dir", [&] () {
						xml.attribute("name", node.name());
						node.xml(xml);
					});
				else
					xml.node("trace_node", [&] () {
						xml.attribute("name", node.name());
						xml.attribute("id", node.id().id);
						node.xml(xml);
					});
				});
		}

		Trace::Subject_id const &id() const { return _id; }
};


class Vfs::Directory_tree : public Genode::Avl_tree<Trace_node>
{
	private:

		Allocator &_alloc;
		Trace_node _root { _alloc, Session_label() };

	public:

		Directory_tree(Genode::Allocator &alloc)
		: _alloc(alloc) { }

		void insert(Trace::Subject_info const &info, Trace::Subject_id const id)
		{
			Trace_node &leaf = _root.insert(info.session_label());
			Trace_node *node  = new (_alloc) Trace_node(_alloc, info.thread_name(), id);
			leaf.Avl_node_tree::insert(node);
		}

		void xml(Genode::Xml_generator &xml)
		{
			_root.xml(xml);
		}
};

#endif /* _DIRECTORY_TREE_H_ */
