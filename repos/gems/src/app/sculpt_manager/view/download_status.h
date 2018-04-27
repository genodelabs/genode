/*
 * \brief  Generate download-status view
 * \author Norman Feske
 * \date   2018-05-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DOWNLOAD_STATUS_H_
#define _VIEW__DOWNLOAD_STATUS_H_

/* local includes */
#include <xml.h>

namespace Sculpt { static void gen_download_status(Xml_generator &, Xml_node); }


void Sculpt::gen_download_status(Xml_generator &xml, Xml_node state)
{
	gen_named_node(xml, "frame", "downloads", [&] () {
		xml.node("vbox", [&] () {

			xml.node("label", [&] () {
				xml.attribute("text", "Download"); });

			unsigned count = 0;
			state.for_each_sub_node("archive", [&] (Xml_node archive) {
				gen_named_node(xml, "hbox", String<10>(count++), [&] () {
					gen_named_node(xml, "float", "left", [&] () {
						xml.attribute("west", "yes");
						typedef String<40> Path;
						xml.node("label", [&] () {
							xml.attribute("text", archive.attribute_value("path", Path()));
							xml.attribute("font", "annotation/regular");
						});
					});

					typedef String<16> Info;

					Info        info  = archive.attribute_value("state", Info());
					float const total = archive.attribute_value("total", 0.0);
					float const now   = archive.attribute_value("now",   0.0);

					if (info == "download") {
						if (total > 0.0)
							info = Info((unsigned)((100*now)/total), "%");
						else
							info = Info("fetch");
					}

					gen_named_node(xml, "float", "right", [&] () {
						xml.attribute("east", "yes");
						xml.node("label", [&] () {
							xml.attribute("text", info);
							xml.attribute("font", "annotation/regular");
						});
					});
				});
			});
		});
	});
}

#endif /* _VIEW__DOWNLOAD_STATUS_H_ */
