/*
 * \brief  plugin registry interface
 * \author Christian Prochaska 
 * \date   2010-01-21
 *
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC_PLUGIN__PLUGIN_REGISTRY_H_
#define _LIBC_PLUGIN__PLUGIN_REGISTRY_H_

#include <util/list.h>

#include <libc-plugin/plugin.h>

namespace Libc {

	struct Plugin_registry;

	extern Plugin_registry *plugin_registry();
}


struct Libc::Plugin_registry : List<Plugin>
{
	Plugin *get_plugin_for_access(char const *pathname, int amode);
	Plugin *get_plugin_for_mkdir(const char *path, mode_t mode);
	Plugin *get_plugin_for_open(const char *pathname, int flags);
	Plugin *get_plugin_for_pipe();
	Plugin *get_plugin_for_readlink(const char *path, char *buf, ::size_t bufsiz);
	Plugin *get_plugin_for_rename(const char *oldpath, const char *newpath);
	Plugin *get_plugin_for_rmdir(const char *path);
	Plugin *get_plugin_for_socket(int domain, int type, int protocol);
	Plugin *get_plugin_for_stat(const char *path, struct stat *);
	Plugin *get_plugin_for_symlink(const char *oldpath, const char *newpath);
	Plugin *get_plugin_for_unlink(const char *path);

	template <typename FUNC>
	void for_each_plugin(FUNC const &fn) const
	{
		for (Plugin *plugin = plugin_registry()->first(); plugin;
		     plugin = plugin->next()) {
			fn(*plugin);
		}
	}
};

#endif /* _LIBC_PLUGIN__PLUGIN_REGISTRY_H_ */
