/*
 * \brief  User information
 * \author Josef Soentgen
 * \date   2012-07-23
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _NOUX__USER_INFO_H_
#define _NOUX__USER_INFO_H_

/* Genode includes */
#include <util/string.h>
#include <util/xml_node.h>

/* Noux includes */
#include <noux_session/sysio.h>

namespace Noux {

	struct User_info {

		public:

			char name[Sysio::MAX_USERNAME_LEN];
			char shell[Sysio::MAX_SHELL_LEN];
			char home[Sysio::MAX_HOME_LEN];

			unsigned int uid;
			unsigned int gid;

		private:

			void _dup_str(char *dest, const char *src)
			{
				Genode::size_t len = Genode::strlen(src);
				Genode::memcpy(dest, src, len);
				dest[len] = '\0';
			}

		public:

			User_info() : uid(0), gid(0)
			{
				_dup_str(name,  "root");
				_dup_str(home,  "/");
				_dup_str(shell, "/bin/bash");
			}

			void set_info(Genode::Xml_node user_info_node)
			{
				try {
					user_info_node.attribute("name").value(name, sizeof(name));
					user_info_node.attribute("uid").value(&uid);
					user_info_node.attribute("gid").value(&gid);

					for (unsigned i = 0; i < user_info_node.num_sub_nodes(); i++) {
						Xml_node sub_node = user_info_node.sub_node(i);

						if (sub_node.has_type("shell")) {
							sub_node.attribute("name").value(shell, sizeof(shell));
						}

						if (sub_node.has_type("home")) {
							sub_node.attribute("name").value(home, sizeof(home));
						}
					}
				}
				catch (...) { }
			}
	};
}

#endif /* _NOUX__USER_INFO_H_ */
