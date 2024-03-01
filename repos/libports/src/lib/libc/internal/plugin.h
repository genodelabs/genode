/*
 * \brief  plugin interface
 * \author Christian Prochaska
 * \date   2010-01-21
 */

/*
 * Copyright (C) 2010-2024 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _LIBC__INTERNAL__PLUGIN_H_
#define _LIBC__INTERNAL__PLUGIN_H_

#include <os/path.h>
#include <base/exception.h>
#include <util/list.h>

#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mount.h>  /* for 'struct statfs' */
#include <sys/poll.h>   /* for 'struct pollfd' */

namespace Genode { class Env; }

namespace Libc {

	using namespace Genode;
	
	class File_descriptor;

	typedef Genode::Path<PATH_MAX> Absolute_path;

	class Symlink_resolve_error : Genode::Exception { };

	void resolve_symlinks(char const *path, Absolute_path &resolved_path);

	class Plugin : public List<Plugin>::Element
	{
		protected:

			int _priority;

			typedef Genode::size_t size_t;

			/* Resume all libc threads blocked for I/O */
			void resume_all();

		public:

			struct Pollfd
			{
				File_descriptor *fdo;
				short            events;
				/*
				 * This pointer points to 'revents' of the original
				 * 'struct pollfd' array.
				 */
				short           *revents;
			};

			Plugin(int priority = 0);
			virtual ~Plugin();

			virtual int priority();

			virtual bool supports_access(char const *path, int amode);
			virtual bool supports_mkdir(const char *path, mode_t mode);
			virtual bool supports_open(const char *pathname, int flags);
			virtual bool supports_pipe();
			virtual bool supports_poll();
			virtual bool supports_readlink(const char *path, char *buf, ::size_t bufsiz);
			virtual bool supports_rename(const char *oldpath, const char *newpath);
			virtual bool supports_rmdir(const char *path);
			virtual bool supports_socket(int domain, int type, int protocol);
			virtual bool supports_stat(const char *path);
			virtual bool supports_symlink(const char *oldpath, const char *newpath);
			virtual bool supports_unlink(const char *path);
			virtual bool supports_mmap();

			/*
			 * Should be overwritten for plugins that require the Genode environment
			 */
			virtual void init(Genode::Env &env) { }

			virtual File_descriptor *accept(File_descriptor *,
			                                struct ::sockaddr *addr,
			                                socklen_t *addrlen);
			virtual int access(char const *, int);
			virtual int bind(File_descriptor *,
			                 const struct ::sockaddr *addr,
			                 socklen_t addrlen);
			virtual int close(File_descriptor *fd);
			virtual int connect(File_descriptor *,
			                    const struct ::sockaddr *addr,
			                    socklen_t addrlen);
			virtual File_descriptor *dup(File_descriptor*);
			virtual int dup2(File_descriptor *, File_descriptor *new_fd);
			virtual int fstatfs(File_descriptor *, struct statfs *buf);
			virtual int fcntl(File_descriptor *, int cmd, long arg);
			virtual int fstat(File_descriptor *, struct stat *buf);
			virtual int fsync(File_descriptor *);
			virtual int ftruncate(File_descriptor *, ::off_t length);
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
			virtual int ioctl(File_descriptor *, unsigned long request, char *argp);
			virtual int listen(File_descriptor *, int backlog);
			virtual ::off_t lseek(File_descriptor *, ::off_t offset, int whence);
			virtual int mkdir(const char *pathname, mode_t mode);
			virtual void *mmap(void *addr, ::size_t length, int prot, int flags,
			                   File_descriptor *, ::off_t offset);
			virtual int munmap(void *addr, ::size_t length);
			virtual int msync(void *addr, ::size_t len, int flags);
			virtual File_descriptor *open(const char *pathname, int flags);
			virtual int pipe(File_descriptor *pipefd[2]);
			virtual int poll(Pollfd fds[], int nfds);
			virtual ssize_t read(File_descriptor *, void *buf, ::size_t count);
			virtual ssize_t readlink(const char *path, char *buf, ::size_t bufsiz);
			virtual ssize_t recv(File_descriptor *, void *buf, ::size_t len, int flags);
			virtual ssize_t recvfrom(File_descriptor *, void *buf, ::size_t len, int flags,
			                         struct sockaddr *src_addr, socklen_t *addrlen);
			virtual ssize_t recvmsg(File_descriptor *, struct msghdr *msg, int flags);
			virtual int rename(const char *oldpath, const char *newpath);
			virtual int rmdir(const char *pathname);
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
			virtual int symlink(const char *oldpath, const char *newpath);
			virtual int unlink(const char *path);
			virtual ssize_t write(File_descriptor *, const void *buf, ::size_t count);
	};
}

#endif /* _LIBC__INTERNAL__PLUGIN_H_ */
