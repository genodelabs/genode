/*
 * \brief  List-based data model created and updated from XML
 * \author Norman Feske
 * \date   2017-08-09
 *
 * The 'List_model' stores a component-internal representation of XML-node
 * content. The XML information is imported according to an 'Update_policy',
 * which specifies how the elements of the data model are created, destroyed,
 * and updated. The elements are ordered according to the order of XML nodes.
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__LIST_MODEL_H_
#define _INCLUDE__UTIL__LIST_MODEL_H_

/* Genode includes */
#include <util/xml_node.h>
#include <util/list.h>

namespace Genode { template <typename> class List_model; }


template <typename ELEM>
class Genode::List_model
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

			public:

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
		 * Update data model according to XML structure 'node'
		 *
		 * \throw Unknown_element_type
		 */
		template <typename POLICY>
		inline void update_from_xml(POLICY &policy, Xml_node node);

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
		 * Remove all elements from the data model
		 *
		 * This method should be called at the destruction time of the
		 * 'List_model'.
		 *
		 * List-model elements are not implicitly destroyed by the destructor
		 * because the 'policy' needed to destruct elements is not kept as
		 * member of the list model (multiple policies may applied to the same
		 * list model).
		 */
		template <typename POLICY>
		void destroy_all_elements(POLICY &policy)
		{
			Element *next = nullptr;
			for (Element *e = _elements.first(); e; e = next) {
				next = e->_next();
				ELEM &elem = static_cast<ELEM &>(*e);
				_elements.remove(&elem);
				policy.destroy_element(elem);
			}
		}
};


template <typename ELEM>
template <typename POLICY>
void Genode::List_model<ELEM>::update_from_xml(POLICY &policy, Xml_node node)
{
	typedef typename POLICY::Element Element;

	List<Element> updated_list;

	Element *last_updated = nullptr; /* used for appending to 'updated_list' */

	node.for_each_sub_node([&] (Xml_node sub_node) {

		/* skip XML nodes that are unrelated to the data model */
		if (!policy.node_is_element(sub_node))
			return;

		/* check for duplicates, which must not exist in the list model */
		for (Element *dup = updated_list.first(); dup; dup = dup->_next()) {

			/* update existing element with information from later node */
			if (policy.element_matches_xml_node(*dup, sub_node)) {
				policy.update_element(*dup, sub_node);
				return;
			}
		}

		/* look up corresponding element in original list */
		Element *curr = _elements.first();
		while (curr && !policy.element_matches_xml_node(*curr, sub_node))
			curr = curr->_next();

		/* consume existing element or create new one */
		if (curr) {
			_elements.remove(curr);
		} else {
			/* \throw Unknown_element_type */
			curr = &policy.create_element(sub_node);
		}

		/* append current element to 'updated_list' */
		updated_list.insert(curr, last_updated);
		last_updated = curr;

		policy.update_element(*curr, sub_node);
	});

	/* remove stale elements */
	Element *next = nullptr;
	for (Element *e = _elements.first(); e; e = next) {
		next = e->_next();
		policy.destroy_element(*e);
	}

	/* use 'updated_list' list new data model */
	_elements = updated_list;
}


/**
 * Policy interface to be supplied to 'List_model::update_from_xml'
 *
 * \param ELEM  element type, must be a list element
 *
 * This class template is merely a blue print of a policy to document the
 * interface.
 */
template <typename ELEM>
struct Genode::List_model<ELEM>::Update_policy
{
	typedef List_model<ELEM>::Unknown_element_type Unknown_element_type;

	/*
	 * Type that needs to be supplied by the policy implementation
	 */
	typedef ELEM Element;

	/**
	 * Destroy element
	 *
	 * When this function is called, the element is no longer contained
	 * in the model's list.
	 */
	void destroy_element(ELEM &elem);

	/**
	 * Create element of the type given in the 'elem_node'
	 *
	 * \throw List_model::Unknown_element_type
	 */
	ELEM &create_element(Xml_node elem_node);

	/**
	 * Import element properties from XML node
	 */
	void update_element(ELEM &elem, Xml_node elem_node);

	/**
	 * Return true if element corresponds to XML node
	 */
	static bool element_matches_xml_node(Element const &, Xml_node);

	/**
	 * Return true if XML node should be imported
	 *
	 * This method allows the policy to disregard certain XML node types from
	 * building the data model.
	 */
	static bool node_is_element(Xml_node) { return true; }
};

#endif /* _INCLUDE__UTIL__LIST_MODEL_H_ */
