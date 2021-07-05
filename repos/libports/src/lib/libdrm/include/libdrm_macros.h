#ifndef _INCLUDE__LIBDRM_MACROS_H_
#define _INCLUDE__LIBDRM_MACROS_H_

#if HAVE_VISIBILITY
#  define drm_private __attribute__((visibility("hidden")))
#  define drm_public  __attribute__((visibility("default")))
#else
#  define drm_private
#  define drm_public
#endif

#include <sys/mman.h>

void *drm_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int drm_munmap(void *addr, size_t length);


#endif /* _INCLUDE__LIBDRM_MACROS_H_ */
