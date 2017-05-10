#ifndef _INCLUDE__LIBDRM_MACROS_H_
#define _INCLUDE__LIBDRM_MACROS_H_

#include <sys/mman.h>

void *drm_mmap(void *addr, size_t length, int prot, int flags,
                             int fd, off_t offset);
int drm_munmap(void *addr, size_t length);

#endif /* _INCLUDE__LIBDRM_MACROS_H_ */
