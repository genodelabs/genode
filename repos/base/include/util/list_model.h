/*
 * \brief  List-based data model created and updated from XML
 * \author Norman Feske
 * \date   2017-08-09
 *
 * The 'List_model' stores a component-internal representation of XML-node
 * content. The internal representation 'ELEM' carries two methods 'matches'
 * and 'type_matches' that define the relation of the elements to XML nodes.
 * E.g.,
 *
 * ! struct Item : List_model<Item>::Element
 * ! {
 * !   static bool type_matches(Xml_node const &);
 * !
 * !   bool matches(Xml_node const &) const;
 * !   ...
 * ! };
 *
 * The class function 'type_matches' returns true if the specified XML node
 * matches the 'Item' type. It can thereby be used to control the creation
 * of 'ELEM' nodes by responding to specific XML tags while ignoring unrelated
 * XML tags.
 *
 * The 'matches' method returns true if the concrete element instance matches
 * the given XML node. It is used to correlate existing 'ELEM' objects with
 * new versions of XML nodes to update the 'ELEM' objects.
 *
 * The functor arguments 'create_fn', 'destroy_fn', and 'update_fn' for the
 * 'update_from_xml' method define how objects are created, destructed, and
 * updated. E.g.,
 *
 * ! _list_model.update_from_xml(node,
 * !
 * !   [&] (Xml_node const &node) -> Item & {
 * !     return *new (alloc) Item(node); },
 * !
 * !   [&] (Item &item) { destroy(alloc, &item); },
 * !
 * !   [&] (Item &item, Xml_node const &node) { item.update(node); }
 * ! );
 *
 * The elements are ordered according to the order of XML nodes.
 *
 * The list model is a container owning the elements. Before destructing a
 * list model, its elements must be removed by calling 'update_from_xml'
 * with an 'Xml_node("<empty/>")' as argument, which results in the call
 * of 'destroy_fn' for each element.
 */

/*
 * Copyright (C) 2017-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__LIST_MODEL_H_
#define _INCLUDE__UTIL__LIST_MODEL_H_

/* Genode includes */
#include <util/noncopyable.h>
#include <util/xml_node.h>
#include <util/list.h>
#include <base/log.h>

namespace Genode { template <typename> class List_model; }


template <typename ELEM>
class Genode::List_model : Noncopyable
{
	private:

		List<ELEM> _elements { };

	public:

		struct Update_policy;

		class Element : private List<ELEM>::Element
		{
			private:

				/* used by 'List_model::update_from_xml' only */
				friend class List_model;
				friend class List<ELEM>;

				ELEM *_next() const { return List<ELEM>::Element::next(); }

				/**
				 * Noncopyable
				 */
				Element(Element const &) = delete;
				Element & operator = (Element const &) = delete;

			public:

				Element() { };

				/**
				 * Return the element's neighbor if present, otherwise nullptr
				 */
				ELEM const *next() const { return List<ELEM>::Element::next(); }
		};

		struct Unknown_element_type : Exception { };

		~List_model()
		{
			if (_elements.first())
				warning("list model not empty at destruction time");
		}

		/**
		 * Update data model according to the given XML node
		 */
		inline void update_from_xml(Xml_node const &,
		                            auto const &create_fn,
		                            auto const &destroy_fn,
		                            auto const &update_fn);

		/**
		 * Call functor 'fn' for each const element
		 */
		template <typename FN>
		void for_each(FN const &fn) const
		{
			for (Element const *e = _elements.first(); e; e = e->next())
				fn(static_cast<ELEM const &>(*e));
		}

		/**
		 * Call functor 'fn' for each non-const element
		 */
		template <typename FN>
		void for_each(FN const &fn)
		{
			Element *next = nullptr;
			for (Element *e = _elements.first(); e; e = next) {
				next = e->_next();
				fn(static_cast<ELEM &>(*e));
			}
		}

		/**
		 * Call functor 'fn' with the first element of the list model
		 *
		 * Using this method combined with 'Element::next', the list model
		 * can be traversed manually. This is handy in situations where the
		 * list-model elements are visited via recursive function calls
		 * instead of a 'for_each' loop.
		 */
		template <typename FN>
		void with_first(FN const &fn) const
		{
			if (Element const *e = _elements.first())
				fn(static_cast<ELEM const &>(*e));
		}
};


template <typename ELEM>
void Genode::List_model<ELEM>::update_from_xml(Xml_node const &node,
                                               auto const &create_fn,
                                               auto const &destroy_fn,
                                               auto const &update_fn)
{
	List<ELEM> updated_list;

	ELEM *last_updated = nullptr; /* used for appending to 'updated_list' */

	node.for_each_sub_node([&] (Xml_node sub_node) {

		/* skip XML nodes that are unrelated to the data model */
		if (!ELEM::type_matches(sub_node))
			return;

		/* check for duplicates, which must not exist in the list model */
		for (ELEM *dup = updated_list.first(); dup; dup = dup->_next()) {

			/* update existing element with information from later node */
			if (dup->matches(sub_node)) {
				update_fn(*dup, sub_node);
				return;
			}
		}

		/* look up corresponding element in original list */
		ELEM *curr = _elements.first();
		while (curr && !curr->matches(sub_node))
			curr = curr->_next();

		/* consume existing element or create new one */
		if (curr)
			_elements.remove(curr);
		else
			curr = &create_fn(sub_node);

		/* append current element to 'updated_list' */
		updated_list.insert(curr, last_updated);
		last_updated = curr;

		update_fn(*curr, sub_node);
	});

	/* remove stale elements */
	ELEM *next = nullptr;
	for (ELEM *e = _elements.first(); e; e = next) {
		next = e->_next();
		destroy_fn(*e);
	}

	/* use 'updated_list' list new data model */
	_elements = updated_list;
}

#endif /* _INCLUDE__UTIL__LIST_MODEL_H_ */
