#ifndef _INCLUDE__LIBDRM_MACROS_H_
#define _INCLUDE__LIBDRM_MACROS_H_

#include <sys/mman.h>

/**
 * On Genode the virtual address of MMAP_GTT is stored in the offset
 */
static inline void *drm_mmap(void *addr, size_t length, int prot, int flags,
                             int fd, off_t offset)
{
	return (void *)offset;
}

static inline int drm_munmap(void *addr, size_t length)
{
	return 0;
}

#endif /* _INCLUDE__LIBDRM_MACROS_H_ */
