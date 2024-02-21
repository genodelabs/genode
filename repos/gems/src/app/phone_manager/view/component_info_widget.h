/*
 * \brief  Widget for displaying component information for the add menu
 * \author Norman Feske
 * \date   2023-11-13
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__COMPONENT_INFO_WIDGET_H_
#define _VIEW__COMPONENT_INFO_WIDGET_H_

#include <model/component.h>

namespace Sculpt { struct Component_info_widget; }


struct Sculpt::Component_info_widget : Widget<Vbox>
{
	void view(Scope<Vbox> &s, Component const &component) const
	{
		if (component.info.length() > 1) {
			s.sub_scope<Label>(Component::Info(" ", component.info, " "));
			s.sub_scope<Small_vgap>();
		}
		s.sub_scope<Annotation>(component.path);
	}
};

#endif /* _VIEW__COMPONENT_INFO_WIDGET_H_ */
