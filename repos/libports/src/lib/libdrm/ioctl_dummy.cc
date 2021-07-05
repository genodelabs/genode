/*
 * \brief  DRM ioctl backend dummy
 * \author Josef Soentgen
 * \date   2021-07-05
 */

/*
 * Copyright (C) 2021 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>

extern "C" {
#include <drm.h>
#include <libdrm_macros.h>
}


extern "C" int genode_ioctl(int fd, unsigned long request, void *arg)
{
	(void)fd;
	(void)request;
	(void)arg;

	Genode::warning(__func__, ": not implemented yet");
	return -1;
}


void *drm_mmap(void *addr, size_t length, int prot, int flags,
               int fd, off_t offset)
{
	(void)addr;
	(void)length;
	(void)prot;
	(void)flags;
	(void)fd;
	(void)offset;

	Genode::warning(__func__, ": not implemented yet");
	return (void*)0;
}


int drm_munmap(void *addr, size_t length)
{
	(void)addr;
	(void)length;

	Genode::warning(__func__, ": not implemented yet");
	return -1;
}
