/*
 * \brief  Widget for showing the system version
 * \author Norman Feske
 * \date   2023-01-23
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SOFTWARE_VERSION_WIDGET_H_
#define _VIEW__SOFTWARE_VERSION_WIDGET_H_

#include <model/build_info.h>
#include <view/dialog.h>

namespace Sculpt { struct Software_version_widget; }


struct Sculpt::Software_version_widget : Widget<Frame>
{
	void view(Scope<Frame> &s, Build_info const &info) const
	{
		using Version = Build_info::Version;

		auto padded = [] (Version const &v) { return Version("  ", v, "  "); };

		s.sub_scope<Vbox>([&] (Scope<Frame, Vbox> &s) {
			s.sub_scope<Label>     (padded(info.image_version()));
			s.sub_scope<Annotation>(padded(info.genode_version()));
		});
	}
};

#endif /* _VIEW__SOFTWARE_VERSION_WIDGET_H_ */
