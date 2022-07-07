/*
 * \brief  DRM ioctl back end dispatcher
 * \author Josef Soentgen
 * \date   2022-07-15
 */

/*
 * Copyright (C) 2022 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/log.h>
#include <libdrm/ioctl_dispatch.h>

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
}


/* etnaviv driver */

extern void  etnaviv_drm_init();
extern int   etnaviv_drm_ioctl(unsigned long, void *);
extern void *etnaviv_drm_mmap(off_t, size_t);
extern int   etnaviv_drm_munmap(void *);

/* lima driver */

extern void  lima_drm_init();
extern int   lima_drm_ioctl(unsigned long, void *);
extern void *lima_drm_mmap(off_t, size_t);
extern int   lima_drm_munmap(void *);
extern int   lima_drm_poll(int);


static Libdrm::Driver drm_backend_type = Libdrm::Driver::INVALID;


/**
 * Initialize DRM back end and set type
 */
void drm_init(Libdrm::Driver driver)
{
	switch (driver) {
	case Libdrm::Driver::ETNAVIV:
		etnaviv_drm_init();
		drm_backend_type = Libdrm::Driver::ETNAVIV;
		break;
	case Libdrm::Driver::LIMA:
		lima_drm_init();
		drm_backend_type = Libdrm::Driver::LIMA;
		break;
	default:
		Genode::error(__func__, ": unknown back end, abort");
		abort();
	}
}


/**
 * Perfom I/O control request
 */
extern "C" int genode_ioctl(int /* fd */, unsigned long request, void *arg)
{
	switch (drm_backend_type) {
	case Libdrm::Driver::ETNAVIV: return etnaviv_drm_ioctl(request, arg);
	case Libdrm::Driver::LIMA:    return lima_drm_ioctl(request, arg);
	default:          return -1;
	}
}


/**
 * Map DRM buffer-object
 */
extern "C" void *drm_mmap(void *addr, size_t length, int prot, int flags,
               int fd, off_t offset)
{
	(void)addr;
	(void)prot;
	(void)flags;
	(void)fd;

	switch (drm_backend_type) {
	case Libdrm::Driver::ETNAVIV: return etnaviv_drm_mmap(offset, length);
	case Libdrm::Driver::LIMA:    return lima_drm_mmap(offset, length);
	default:          return NULL;
	}
}


/**
 * Unmap DRM buffer-object
 */
extern "C" int drm_munmap(void *addr, size_t length)
{
	(void)length;

	switch (drm_backend_type) {
	case Libdrm::Driver::ETNAVIV: return etnaviv_drm_munmap(addr);
	case Libdrm::Driver::LIMA:    return lima_drm_munmap(addr);
	default:          return -1;
	}
}


extern "C" int drm_poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	(void)timeout;

	if (nfds > 1) {
		Genode::error(__func__, ": cannot handle more the 1 pollfd");
		return -1;
	}

	switch (drm_backend_type) {
	case Libdrm::Driver::ETNAVIV: return -1;
	case Libdrm::Driver::LIMA:    return lima_drm_poll(fds[0].fd);
	default:          return -1;
	}
}
