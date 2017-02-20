/*
 * \brief  User information
 * \author Josef Soentgen
 * \author Norman Feske
 * \date   2012-07-23
 */

/*
 * Copyright (C) 2011-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _NOUX__USER_INFO_H_
#define _NOUX__USER_INFO_H_

/* Genode includes */
#include <util/string.h>
#include <util/xml_node.h>

/* Noux includes */
#include <noux_session/sysio.h>

namespace Noux {
	class User_info;
	using namespace Genode;
}


class Noux::User_info : Noncopyable
{
	public:

		typedef String<Sysio::MAX_USERNAME_LEN> Name;
		typedef String<Sysio::MAX_SHELL_LEN>    Shell;
		typedef String<Sysio::MAX_HOME_LEN>     Home;

	private:

		unsigned const _uid;
		unsigned const _gid;

		Name  const _name;
		Shell const _shell;
		Home  const _home;

		template <typename S>
		S _sub_node_name(Xml_node node, char const *sub_node, S const &default_name)
		{
			if (!node.has_sub_node(sub_node))
				return default_name;

			return node.sub_node(sub_node).attribute_value("name", default_name);
		}

	public:

		User_info(Xml_node node)
		:
			_uid (node.attribute_value("uid", 0UL)),
			_gid (node.attribute_value("gid", 0UL)),
			_name(node.attribute_value("name", Name("root"))),
			_shell(_sub_node_name(node, "shell", Shell("/bin/bash"))),
			_home (_sub_node_name(node, "home",  Home("name")))
		{ }

		unsigned uid() const { return _uid; }
		unsigned gid() const { return _gid; }

		Name  name()  const { return _name;  }
		Shell shell() const { return _shell; }
		Home  home()  const { return _home;  }
};

#endif /* _NOUX__USER_INFO_H_ */
