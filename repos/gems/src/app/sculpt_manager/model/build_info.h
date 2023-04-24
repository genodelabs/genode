/*
 * \brief  Interface to obtain version info about the used system image
 * \author Norman Feske
 * \date   2022-01-24
 */

/*
 * Copyright (C) 2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _BUILD_INFO_H_
#define _BUILD_INFO_H_

#include <types.h>

namespace Sculpt { struct Build_info; }


struct Sculpt::Build_info
{
	using Value   = String<64>;
	using Version = String<64>;

	Value genode_source, date, depot_user, board;

	Version image_version() const
	{
		return Version(depot_user, "/sculpt-", board, "-", date);
	}

	Version genode_version() const
	{
		return Version("Genode ", genode_source);
	}

	static Build_info from_xml(Xml_node const &info)
	{
		return Build_info {
			.genode_source = info.attribute_value("genode_version", Value()),
			.date          = info.attribute_value("date",           Value()),
			.depot_user    = info.attribute_value("depot_user",     Value()),
			.board         = info.attribute_value("board",          Value())
		};
	}
};

#endif /* _BUILD_INFO_H_ */
