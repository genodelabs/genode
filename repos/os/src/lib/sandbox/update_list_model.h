/*
 * \brief  Convenience wrapper around 'List_model'
 * \author Norman Feske
 * \date   2021-04-01
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _UPDATE_LIST_MODEL_H_
#define _UPDATE_LIST_MODEL_H_

/* Genode includes */
#include <util/list_model.h>

/* local includes */
#include <types.h>

namespace Sandbox {

	template <typename NODE, typename CREATE_FN, typename DESTROY_FN, typename UPDATE_FN>
	static inline void update_list_model_from_xml(List_model<NODE> &,
	                                              Xml_node   const &,
	                                              CREATE_FN  const &,
	                                              DESTROY_FN const &,
	                                              UPDATE_FN  const &);
}


template <typename NODE, typename CREATE_FN, typename DESTROY_FN, typename UPDATE_FN>
void Sandbox::update_list_model_from_xml(List_model<NODE> &model,
                                         Xml_node   const &xml,
                                         CREATE_FN  const &create,
                                         DESTROY_FN const &destroy,
                                         UPDATE_FN  const &update)
{
	struct Model_update_policy : List_model<NODE>::Update_policy
	{
		CREATE_FN  const &_create_fn;
		DESTROY_FN const &_destroy_fn;
		UPDATE_FN  const &_update_fn;

		Model_update_policy(CREATE_FN  const &create_fn,
		                    DESTROY_FN const &destroy_fn,
		                    UPDATE_FN  const &update_fn)
		:
			_create_fn(create_fn), _destroy_fn(destroy_fn), _update_fn(update_fn)
		{ }

		void destroy_element(NODE &node) { _destroy_fn(node); }

		NODE &create_element(Xml_node xml) { return _create_fn(xml); }

		void update_element(NODE &node, Xml_node xml) { _update_fn(node, xml); }

		static bool element_matches_xml_node(NODE const &node, Xml_node xml)
		{
			return node.matches(xml);
		}
	} policy(create, destroy, update);

	model.update_from_xml(policy, xml);
}

#endif /* _UPDATE_LIST_MODEL_H_ */
