/*
 * \brief  Dialog for showing the system version
 * \author Norman Feske
 * \date   2023-01-23
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__SOFTWARE_VERSION_DIALOG_H_
#define _VIEW__SOFTWARE_VERSION_DIALOG_H_

#include <model/build_info.h>
#include <view/dialog.h>

namespace Sculpt { struct Software_version_dialog; }


struct Sculpt::Software_version_dialog
{
	Build_info const _build_info;

	Software_version_dialog(Build_info  const &info) : _build_info(info) { }

	void generate(Xml_generator &xml) const
	{
		using Version = Build_info::Version;
		auto padded = [] (Version const &v) { return Version("  ", v, "  "); };

		gen_named_node(xml, "frame", "version", [&] {
			xml.node("vbox", [&] {
				gen_named_node(xml, "label", "image", [&] {
					xml.attribute("text", padded(_build_info.image_version())); });
				gen_named_node(xml, "label", "genode", [&] {
					xml.attribute("text", padded(_build_info.genode_version()));
					xml.attribute("font", "annotation/regular");
				});
			});
		});
	}
};

#endif /* _VIEW__SOFTWARE_VERSION_DIALOG_H_ */
