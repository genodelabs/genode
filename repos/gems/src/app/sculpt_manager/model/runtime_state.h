/*
 * \brief  State of the components hosted in the runtime subsystem
 * \author Norman Feske
 * \date   2018-08-22
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _MODEL__RUNTIME_STATE_H_
#define _MODEL__RUNTIME_STATE_H_

/* Genode includes */
#include <util/xml_node.h>
#include <util/list_model.h>

/* local includes */
#include <types.h>
#include <runtime.h>

namespace Sculpt { class Runtime_state; }

class Sculpt::Runtime_state : public Runtime_info
{
	private:

		Allocator &_alloc;

		struct Child : List_model<Child>::Element
		{
			Start_name const name;

			Child(Start_name const &name) : name(name) { }
		};

		List_model<Child> _children { };

		struct Update_policy : List_model<Child>::Update_policy
		{
			Allocator &_alloc;

			Update_policy(Allocator &alloc) : _alloc(alloc) { }

			void destroy_element(Child &elem) { destroy(_alloc, &elem); }

			Child &create_element(Xml_node node)
			{
				return *new (_alloc)
					Child(node.attribute_value("name", Start_name()));
			}

			void update_element(Child &, Xml_node) { }

			static bool element_matches_xml_node(Child const &elem, Xml_node node)
			{
				return node.attribute_value("name", Start_name()) == elem.name;
			}
		};

	public:

		Runtime_state(Allocator &alloc) : _alloc(alloc) { }

		void update_from_state_report(Xml_node state)
		{
			Update_policy policy(_alloc);
			_children.update_from_xml(policy, state);
		}

		/**
		 * Runtime_info interface
		 */
		bool present_in_runtime(Start_name const &name) const override
		{
			bool result = false;
			_children.for_each([&] (Child const &child) {
				if (!result && child.name == name)
					result = true; });
			return result;
		}
};

#endif /* _MODEL__RUNTIME_STATE_H_ */
