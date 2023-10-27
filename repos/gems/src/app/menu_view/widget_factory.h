/*
 * \brief  Interface for creating widgets out of their XML description
 * \author Norman Feske
 * \date   2009-09-11
 */

/*
 * Copyright (C) 2014-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _WIDGET_FACTORY_H_
#define _WIDGET_FACTORY_H_

/* local includes */
#include "style_database.h"

/* gems includes */
#include <gems/animator.h>

namespace Menu_view {

	class Widget;
	class Widget_factory;
}


class Menu_view::Widget_factory
{
	private:

		unsigned _unique_id_cnt = 0;

	public:

		Allocator      &alloc;
		Style_database &styles;
		Animator       &animator;

		Widget_factory(Allocator &alloc, Style_database &styles, Animator &animator)
		:
			alloc(alloc), styles(styles), animator(animator)
		{ }

		Widget &create(Xml_node const &);

		void destroy(Widget *widget) { Genode::destroy(alloc, widget); }

		static bool node_type_known(Xml_node const &);
};

#endif /* _WIDGET_FACTORY_H_ */
