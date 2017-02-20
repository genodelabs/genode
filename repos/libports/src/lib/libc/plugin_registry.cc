/*
 * \brief  Plugin registry implementation
 * \author Christian Prochaska 
 * \date   2010-01-21
 */

/*
 * Copyright (C) 2010-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <libc-plugin/plugin_registry.h>

namespace Libc {

	Plugin_registry *plugin_registry()
	{
		static Plugin_registry _plugin_registry;
		return &_plugin_registry;
	}
}

using namespace Libc;


#define GET_PLUGIN_FOR(func_name, ...) \
	int highest_priority_found = -1; \
	Plugin *result = 0; \
	for (Plugin *plugin = first(); plugin != 0; plugin = plugin->next()) { \
		if (plugin->supports_##func_name(__VA_ARGS__) && \
		    (plugin->priority() > highest_priority_found)) { \
			result = plugin; \
			highest_priority_found = plugin->priority(); \
		} \
	} \
	return result;

Plugin *Plugin_registry::get_plugin_for_access(char const *path, int amode) {
	GET_PLUGIN_FOR(access, path, amode) }

Plugin *Plugin_registry::get_plugin_for_execve(char const *filename, char *const argv[],
        char *const envp[]) {
	GET_PLUGIN_FOR(execve, filename, argv, envp) }


Plugin *Plugin_registry::get_plugin_for_freeaddrinfo(struct addrinfo *res) {
	GET_PLUGIN_FOR(freeaddrinfo, res) }


Plugin *Plugin_registry::get_plugin_for_getaddrinfo(const char *node, const char *service,
                                                    const struct addrinfo *hints,
                                                    struct addrinfo **res) {
	GET_PLUGIN_FOR(getaddrinfo, node, service, hints, res) }


Plugin *Plugin_registry::get_plugin_for_mkdir(const char *path, mode_t mode) {
	GET_PLUGIN_FOR(mkdir, path, mode) }


Plugin *Plugin_registry::get_plugin_for_open(const char *pathname, int flags) {
	GET_PLUGIN_FOR(open, pathname, flags) }


Plugin *Plugin_registry::get_plugin_for_pipe() {
	GET_PLUGIN_FOR(pipe) }


Plugin *Plugin_registry::get_plugin_for_readlink(const char *path, char *buf, ::size_t bufsiz) {
	GET_PLUGIN_FOR(readlink, path, buf, bufsiz) }


Plugin *Plugin_registry::get_plugin_for_rename(const char *oldpath, const char *newpath) {
	GET_PLUGIN_FOR(rename, oldpath, newpath) }

Plugin *Plugin_registry::get_plugin_for_rmdir(const char *path) {
	GET_PLUGIN_FOR(rmdir, path) }

Plugin *Plugin_registry::get_plugin_for_socket(int domain, int type, int protocol) {
	GET_PLUGIN_FOR(socket, domain, type, protocol) }


Plugin *Plugin_registry::get_plugin_for_stat(const char *path, struct stat *) {
	GET_PLUGIN_FOR(stat, path) }


Plugin *Plugin_registry::get_plugin_for_symlink(const char *oldpath, const char *newpath) {
	GET_PLUGIN_FOR(symlink, oldpath, newpath) }


Plugin *Plugin_registry::get_plugin_for_unlink(const char *path) {
	GET_PLUGIN_FOR(unlink, path) }
