/*
 * \brief  Genode-specific shared-object backend
 * \author Norman Feske
 * \date   2013-03-29
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

extern "C" {

#include <dlfcn.h>

#include "SDL_config.h"
#include "SDL_loadso.h"

void *SDL_LoadObject(const char *sofile)
{
	return dlopen(sofile, 0);
}


void *SDL_LoadFunction(void *handle, const char *name)
{
	return dlsym(handle, name);
}


void SDL_UnloadObject(void* handle)
{
	dlclose(handle);
}

}
