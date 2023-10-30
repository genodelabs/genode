/*
 * \brief  Generate download-status widget
 * \author Norman Feske
 * \date   2018-05-18
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _VIEW__DOWNLOAD_STATUS_WIDGET_H_
#define _VIEW__DOWNLOAD_STATUS_WIDGET_H_

/* local includes */
#include <model/download_queue.h>
#include <view/dialog.h>

namespace Sculpt { struct Download_status_widget; }


struct Sculpt::Download_status_widget : Titled_frame
{
	void view(Scope<Frame> &s, Xml_node const &state, Download_queue const &download_queue) const
	{
		Titled_frame::view(s, [&] {

			using Path = String<40>;
			using Info = String<16>;

			auto gen_message = [&] (auto const &path, auto const &info)
			{
				s.sub_scope<Left_right_annotation>(path, Info(" ", info));
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
	}
};

#endif /* _VIEW__DOWNLOAD_STATUS_WIDGET_H_ */
