/*
 * \brief  Utility for updating an internal data model from an XML structure
 * \author Norman Feske
 * \date   2017-08-09
 */

/*
 * Copyright (C) 2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__UTIL__LIST_MODEL_FROM_XML_H_
#define _INCLUDE__UTIL__LIST_MODEL_FROM_XML_H_

#include <util/xml_node.h>

namespace Genode {

	template <typename POLICY>
	static inline void
	update_list_model_from_xml(POLICY                         &policy,
	                           List<typename POLICY::Element> &list,
	                           Xml_node                        node);

	template <typename> struct List_model_update_policy;
}


/**
 * Policy interface to be supplied to 'update_list_model_from_xml'
 *
 * \param ELEM  element type, must be a list element
 *
 * This class template is merely a blue print of a policy to document the
 * interface.
 */
template <typename ELEM>
struct Genode::List_model_update_policy
{
	/*
	 * Types that need to be supplied by the policy implementation
	 */
	typedef ELEM Element;

	struct Unknown_element_type : Exception { };

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
	 * \throw Unknown_element_type
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
	static bool node_is_element(Xml_node node) { return true; }
};


/**
 * Update 'list' data model according to XML structure 'node'
 *
 * \throw Unknown_element_type
 */
template <typename POLICY>
static inline void
Genode::update_list_model_from_xml(POLICY                         &policy,
                                   List<typename POLICY::Element> &list,
                                   Xml_node                        node)
{
	typedef typename POLICY::Element Element;

	List<Element> updated_list;

	Element *last_updated = nullptr; /* used for appending to 'updated_list' */

	node.for_each_sub_node([&] (Xml_node sub_node) {

		/* skip XML nodes that are unrelated to the data model */
		if (!policy.node_is_element(sub_node))
			return;

		/* look up corresponding element in original list */
		Element *curr = list.first();
		while (curr && !policy.element_matches_xml_node(*curr, sub_node))
			curr = curr->next();

		/* consume existing element or create new one */
		if (curr) {
			list.remove(curr);
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
	for (Element *e = list.first(); e; e = next) {
		next = e->next();
		policy.destroy_element(*e);
	}

	/* use 'updated_list' list new data model */
	list = updated_list;
}

#endif /* _INCLUDE__UTIL__LIST_MODEL_FROM_XML_H_ */
