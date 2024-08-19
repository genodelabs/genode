/*
 * \brief  QPA plugin initialization
 * \author Christian Prochaska
 * \date   2020-06-28
 */

/*
 * Copyright (C) 2020 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__QPA_INIT_H_
#define _INCLUDE__QPA_INIT_H_

/* Genode includes */
#include <base/env.h>

/* libc includes */
#include <dlfcn.h>  /* 'dlopen'  */
#include <stdio.h>  /* 'fprintf' */
#include <stdlib.h> /* 'exit'    */
#include <unistd.h> /* 'access'  */

/**
 * Initialize the QPA plugin
 *
 * When Qt loads the plugin again, it will get the same handle.
 */
static inline void qpa_init(Genode::Env &env)
{
	char const *qpa_plugin = "/qt/plugins/platforms/libqgenode.lib.so";

	void *qpa_plugin_handle = nullptr;

	/* check existence with 'access()' first to avoid ld error messages */
	if (access(qpa_plugin, F_OK) == 0)
		qpa_plugin_handle = dlopen(qpa_plugin, RTLD_LAZY);

	if (qpa_plugin_handle) {

		typedef void (*initialize_qpa_plugin_t)(Genode::Env &);

		initialize_qpa_plugin_t initialize_qpa_plugin =
			(initialize_qpa_plugin_t) dlsym(qpa_plugin_handle,
			                                "initialize_qpa_plugin");

		if (!initialize_qpa_plugin) {
			fprintf(stderr, "Could not find 'initialize_qpa_plugin' \
			                 function in QPA plugin\n");
			dlclose(qpa_plugin_handle);
			exit(1);
		}

		initialize_qpa_plugin(env);
	}
}

#endif /* _INCLUDE__QPA_INIT_H_ */
