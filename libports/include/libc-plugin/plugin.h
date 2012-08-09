/*
 * \brief  plugin interface
 * \author Christian Prochaska
 * \date   2010-01-21
 */

/*
 * Copyright (C) 2010-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _LIBC_PLUGIN__PLUGIN_H_
#define _LIBC_PLUGIN__PLUGIN_H_

#include <util/list.h>

#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mount.h>  /* for 'struct statfs' */

namespace Libc {

	using namespace Genode;
	
	class File_descriptor;
	
	class Plugin : public List<Plugin>::Element
	{
		protected:

			int _priority;

		public:

			Plugin(int priority = 0);
			virtual ~Plugin();

			virtual int priority();

			virtual bool supports_chdir(const char *path);
			virtual bool supports_mkdir(const char *path, mode_t mode);
			virtual bool supports_freeaddrinfo(struct ::addrinfo *res);
			virtual bool supports_getaddrinfo(const char *node, const char *service,
			                                  const struct ::addrinfo *hints,
			                                  struct ::addrinfo **res);
			virtual bool supports_open(const char *pathname, int flags);
			virtual bool supports_pipe();
			virtual bool supports_rename(const char *oldpath, const char *newpath);
			virtual bool supports_select(int nfds,
			                             fd_set *readfds,
			                             fd_set *writefds,
			                             fd_set *exceptfds,
			                             struct timeval *timeout);
			virtual bool supports_socket(int domain, int type, int protocol);
			virtual bool supports_stat(const char *path);
			virtual bool supports_unlink(const char *path);

			virtual File_descriptor *accept(File_descriptor *,
			                                struct ::sockaddr *addr,
			                                socklen_t *addrlen);
			virtual int bind(File_descriptor *,
			                 const struct ::sockaddr *addr,
			                 socklen_t addrlen);
			virtual int chdir(const char *path);
			virtual int close(File_descriptor *fd);
			virtual int connect(File_descriptor *,
			                    const struct ::sockaddr *addr,
			                    socklen_t addrlen);
			virtual int dup2(File_descriptor *, File_descriptor *new_fd);
			virtual int fstatfs(File_descriptor *, struct statfs *buf);
			virtual int fchdir(File_descriptor *);
			virtual int fcntl(File_descriptor *, int cmd, long arg);
			virtual void freeaddrinfo(struct ::addrinfo *res);
			virtual int fstat(File_descriptor *, struct stat *buf);
			virtual int fsync(File_descriptor *);
			virtual int ftruncate(File_descriptor *, ::off_t length);
			virtual int getaddrinfo(const char *node, const char *service,
			                        const struct ::addrinfo *hints,
			                        struct ::addrinfo **res);
			virtual ssize_t getdirentries(File_descriptor *, char *buf,
			                              ::size_t nbytes, ::off_t *basep);
			virtual int getpeername(File_descriptor *,
			                        struct sockaddr *addr,
			                        socklen_t *addrlen);
			virtual int getsockname(File_descriptor *,
			                        struct sockaddr *addr,
			                        socklen_t *addrlen);
			virtual int getsockopt(File_descriptor *, int level,
			                       int optname, void *optval,
			                       socklen_t *optlen);
			virtual int ioctl(File_descriptor *, int request, char *argp);
			virtual int listen(File_descriptor *, int backlog);
			virtual ::off_t lseek(File_descriptor *, ::off_t offset, int whence);
			virtual int mkdir(const char *pathname, mode_t mode);
			virtual void *mmap(void *addr, ::size_t length, int prot, int flags,
			                   File_descriptor *, ::off_t offset);
			virtual File_descriptor *open(const char *pathname, int flags);
			virtual int pipe(File_descriptor *pipefd[2]);
			virtual ssize_t read(File_descriptor *, void *buf, ::size_t count);
			virtual ssize_t recv(File_descriptor *, void *buf, ::size_t len, int flags);
			virtual ssize_t recvfrom(File_descriptor *, void *buf, ::size_t len, int flags,
			                         struct sockaddr *src_addr, socklen_t *addrlen);
			virtual ssize_t recvmsg(File_descriptor *, struct msghdr *msg, int flags);
			virtual int rename(const char *oldpath, const char *newpath);
			virtual int select(int nfds, fd_set *readfds, fd_set *writefds,
			                   fd_set *exceptfds, struct timeval *timeout);
			virtual ssize_t send(File_descriptor *, const void *buf, ::size_t len, int flags);
			virtual ssize_t sendto(File_descriptor *, const void *buf,
			                       ::size_t len, int flags,
			                       const struct sockaddr *dest_addr,
			                       socklen_t addrlen);
			virtual int setsockopt(File_descriptor *, int level,
			                       int optname, const void *optval,
			                       socklen_t optlen);
			virtual int shutdown(File_descriptor *, int how);
			virtual File_descriptor *socket(int domain, int type, int protocol);
			virtual int stat(const char *path, struct stat *buf);
			virtual int unlink(const char *path);
			virtual ssize_t write(File_descriptor *, const void *buf, ::size_t count);
	};
}

#endif /* _LIBC_PLUGIN__PLUGIN_H_ */
