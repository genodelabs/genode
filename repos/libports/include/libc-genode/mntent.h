#ifndef _LIBC__INCLUDE__MNTENT_H_
#define _LIBC__INCLUDE__MNTENT_H_

#include <sys/mount.h>

struct statfs;
typedef enum { FIND, REMOVE, CHECKUNIQUE } dowhat;


struct statfs *getmntentry(const char *fromname, const char *onname,
                           fsid_t *fsid, dowhat what);

#endif /* _LIBC__INCLUDE__MNTENT_H_ */
