#include <base/log.h>
#include <os/backtrace.h>

extern "C" {
#include <pwd.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
}

#if 0
#define WARN_NOT_IMPL Genode::warning(__func__, " not implemented (jvm)");
#else
#define WARN_NOT_IMPL
#endif

extern "C" void collector_func_load(char* name,
                                   void* null_argument_1,
                                   void* null_argument_2,
                                   void *vaddr,
                                   int size,
                                   int zero_argument,
                                   void* null_argument_3)
{ }


int getpwuid_r(uid_t uid, struct passwd *pwd,
                      char *buf, size_t buflen, struct passwd **result)
{
	*result = nullptr;
	return -1;
}


int getpwnam_r(const char *name, struct passwd *pwd,
               char *buf, size_t buflen, struct passwd **result)
{
	WARN_NOT_IMPL;
	return -1;
}


int mincore(const void *, size_t, char *)
{
	WARN_NOT_IMPL;
	return -1;
}


int msync(void *addr, size_t length, int flags)
{
	WARN_NOT_IMPL;
	return -1;
}


int lchown(const char *pathname, uid_t owner, gid_t group)
{
	WARN_NOT_IMPL;
	return -1;
}


int futimes(int fd, const struct timeval tv[2])
{
	WARN_NOT_IMPL;
	return -1;
}


ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	WARN_NOT_IMPL;
	return -1;
}


int socketpair(int domain, int type, int protocol, int sv[2])
{
	WARN_NOT_IMPL;
	return 0;
}

int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact)
{
	WARN_NOT_IMPL;
	return 0;
}


extern "C" void backtrace()
{
	Genode::backtrace();
}
