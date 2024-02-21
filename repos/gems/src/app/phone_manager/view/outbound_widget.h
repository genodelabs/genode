/*
 * \brief  Widget for initiating calls
 * \author Norman Feske
 * \date   2022-06-29
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__OUTBOUND_WIDGET_H_
#define _VIEW__OUTBOUND_WIDGET_H_

#include <view/dialog.h>

namespace Sculpt { struct Outbound_widget; }


struct Sculpt::Outbound_widget : Widget<Frame>
{
	void view(Scope<Frame> &s) const
	{
		s.sub_scope<Hbox>([&] (Scope<Frame, Hbox> &s) {

			s.sub_scope<Button>([&] (Scope<Frame, Hbox, Button> &s) {
				s.attribute("style", "unimportant");
				s.sub_scope<Label>("History");
			});

			s.sub_scope<Button>([&] (Scope<Frame, Hbox, Button> &s) {
				s.attribute("style", "unimportant");
				s.sub_scope<Label>("Contacts");
			});

			s.sub_scope<Button>([&] (Scope<Frame, Hbox, Button> &s) {
				s.attribute("selected", "yes");
				s.sub_scope<Label>("Dial");
			});
		});
	}
};

#endif /* _VIEW__OUTBOUND_WIDGET_H_ */
