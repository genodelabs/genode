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
#include <model/download_queue.h>

namespace Sculpt {

	static void gen_download_status(Xml_generator &, Xml_node const &, Download_queue const &);
}


void Sculpt::gen_download_status(Xml_generator &xml, Xml_node const &state,
                                 Download_queue const &download_queue)
{
	gen_named_node(xml, "frame", "downloads", [&] () {
		xml.node("vbox", [&] () {

			xml.node("label", [&] () {
				xml.attribute("text", "Download"); });

			using Path = String<40>;
			using Info = String<16>;

			unsigned count = 0;

			auto gen_message = [&] (auto const &path, auto const &info)
			{
				gen_named_node(xml, "hbox", String<10>(count++), [&] () {
					gen_named_node(xml, "float", "left", [&] () {
						xml.attribute("west", "yes");
						xml.node("label", [&] () {
							xml.attribute("text", path);
							xml.attribute("font", "annotation/regular");
						});
					});
					gen_named_node(xml, "float", "right", [&] () {
						xml.attribute("east", "yes");
						xml.node("label", [&] () {
							xml.attribute("text", Info(" ", info));
							xml.attribute("font", "annotation/regular");
						});
					});
				});
			};

			bool const download_in_progress = state.attribute_value("progress", false);

			if (download_in_progress) {
				state.for_each_sub_node("archive", [&] (Xml_node archive) {

					Path   const path  = archive.attribute_value("path",  Path());
					Info         info  = archive.attribute_value("state", Info());
					double const total = archive.attribute_value("total", 0.0d);
					double const now   = archive.attribute_value("now",   0.0d);

					if (info == "download") {
						if (total > 0.0)
							info = Info((unsigned)((100*now)/total), "%");
						else
							info = Info("fetch");
					}

					gen_message(path, info);
				});
			} else {
				download_queue.for_each_failed_download([&] (Path const &path) {
					gen_message(path, "failed"); });
			}
		});
	});
}

#endif /* _VIEW__DOWNLOAD_STATUS_H_ */
