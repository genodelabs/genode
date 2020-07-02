/*
 * \brief  Generate seccomp filter policy for base-linux
 * \author Stefan Thoeni
 * \date   2019-12-13
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 * Copyright (C) 2019 gapfruit AG
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <stdio.h>   /* printf */
#include <seccomp.h> /* libseccomp */
#include <asm/signal.h>
#include <linux/sched.h>

class Filter
{
	private:
		scmp_filter_ctx _ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
		uint32_t _arch;


		void _add_allow_rule(int syscall_number)
		{
			int result = seccomp_rule_add(_ctx, SCMP_ACT_ALLOW, syscall_number, 0);
			if (result != 0) {
				fprintf(stderr, "Add rule failed for number %i\n", syscall_number);
				throw -102;
			}
		}


		void _add_allow_rule(int syscall_number, scmp_arg_cmp c1)
		{
			int result = seccomp_rule_add(_ctx, SCMP_ACT_ALLOW, syscall_number, 1, c1);
			if (result != 0) {
				fprintf(stderr, "Add rule failed for number %i\n", syscall_number);
				throw -102;
			}
		}


		void _add_allow_rule(int syscall_number, scmp_arg_cmp c1, scmp_arg_cmp c2)
		{
			int result = seccomp_rule_add(_ctx, SCMP_ACT_ALLOW, syscall_number, 2, c1, c2);
			if (result != 0) {
				fprintf(stderr, "Add rule failed for number %i\n", syscall_number);
				throw -102;
			}
		}

	public:

		Filter(uint32_t arch)
			: _arch(arch)
		{
		}


		int create()
		{
			/* Kill the process if the filter architecture does not fit. */
			if (seccomp_attr_set(_ctx, SCMP_FLTATR_ACT_BADARCH, SCMP_ACT_KILL_PROCESS) != 0) {
				fprintf(stderr, "Failed to set bad architecture action\n");
				throw -103;
			}

			/* Remove the default architecture (e.g. native architecture) from the filter.*/
			if (seccomp_arch_remove(_ctx, SCMP_ARCH_NATIVE) != 0) {
				fprintf(stderr, "Failed to remove default architecture\n");
				throw -103;
			}

			/* Add the desired architecture to the filter.*/
			if (seccomp_arch_add(_ctx, _arch) != 0) {
				fprintf(stderr, "Failed to add architecture\n");
				throw -103;
			}

			/* This syscall is safe as it create a socket pair in the
			 * process */
			_add_allow_rule(SCMP_SYS(socketpair));

			/* These syscalls should be safe as they only access already open sockets. */
			_add_allow_rule(SCMP_SYS(sendmsg));
			_add_allow_rule(SCMP_SYS(recvmsg));
			_add_allow_rule(SCMP_SYS(write));
			_add_allow_rule(SCMP_SYS(poll));
			_add_allow_rule(SCMP_SYS(epoll_create));
			_add_allow_rule(SCMP_SYS(epoll_ctl));
			_add_allow_rule(SCMP_SYS(epoll_wait));
			_add_allow_rule(SCMP_SYS(close));
			_add_allow_rule(SCMP_SYS(munmap));
			_add_allow_rule(SCMP_SYS(dup));
			_add_allow_rule(SCMP_SYS(fstat));
			_add_allow_rule(SCMP_SYS(fstat64));

			/* This syscall is used to wait for a condition. This should be safe. */
			_add_allow_rule(SCMP_SYS(futex));

			/* This syscall ends the program. This should be safe */
			_add_allow_rule(SCMP_SYS(exit));

			/* These syscalls are used to react to signals. They should be safe */
			_add_allow_rule(SCMP_SYS(sigaltstack));
			_add_allow_rule(SCMP_SYS(rt_sigaction));

			/* This syscall is used to sleep. This should be safe */
			_add_allow_rule(SCMP_SYS(nanosleep));

			/* These syscall allow access to global information. We would like
			 * to reduced this. */
			_add_allow_rule(SCMP_SYS(getpid));
			_add_allow_rule(SCMP_SYS(gettid));
			_add_allow_rule(SCMP_SYS(gettimeofday));
			_add_allow_rule(SCMP_SYS(getpeername));

			int clone_flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND
			                | CLONE_THREAD | CLONE_SYSVSEM;

			switch (_arch)
			{
				case SCMP_ARCH_X86:
					{
						/* The tgkill syscall must be made safe by restricting parameters.
						 * Genode uses LX_SIGCANCEL alias SIGRTMIN to cancel threads
						 * the 0xCAFEAFFE will be replaced with the process ID to restrict
						 * tgkill to the process (= thread group). */
						_add_allow_rule(SCMP_SYS(tgkill), SCMP_CMP32(0, SCMP_CMP_EQ, 0xCAFEAFFE),
						                                  SCMP_CMP32(2, SCMP_CMP_EQ, SIGRTMIN));

						/* The clone syscall must be made safe by restricting parameters
						 * The specified flags only allow creation of new threads. */
						_add_allow_rule(SCMP_SYS(clone), SCMP_CMP32(0, SCMP_CMP_EQ, clone_flags));

						/* The nmap syscall has a different name on different architectures
						 * but it slould be save as it only uses an already open socket. */
						_add_allow_rule(SCMP_SYS(mmap2));

						/* returning from signal handlers is safe */
						_add_allow_rule(SCMP_SYS(sigreturn));
					}
					break;
				case SCMP_ARCH_X86_64:
					{
						/* The tgkill syscall must be made safe by restricting parameters.
						 * Genode uses LX_SIGCANCEL alias SIGRTMIN to cancel threads
						 * the 0xCAFEAFFE will be replaced with the process ID to restrict
						 * tgkill to the process (= thread group). */
						_add_allow_rule(SCMP_SYS(tgkill), SCMP_CMP64(0, SCMP_CMP_EQ, 0xCAFEAFFE),
						                                  SCMP_CMP64(2, SCMP_CMP_EQ, SIGRTMIN));

						/* The clone syscall must be made safe by restricting parameters
						 * The specified flags only allow creation of new threads. */
						_add_allow_rule(SCMP_SYS(clone), SCMP_CMP64(0, SCMP_CMP_EQ, clone_flags));

						/* The nmap syscall has a different name on different architectures
						 * but it slould be save as it only uses an already open socket. */
						_add_allow_rule(SCMP_SYS(mmap));

						/* returning from signal handlers is safe */
						_add_allow_rule(SCMP_SYS(rt_sigreturn));
					}
					break;
				case SCMP_ARCH_ARM:
					 {
						/* The tgkill syscall must be made safe by restricting parameters.
						 * Genode uses LX_SIGCANCEL alias SIGRTMIN to cancel threads
						 * the 0xCAFEAFFE will be replaced with the process ID to restrict
						 * tgkill to the process (= thread group). */
						_add_allow_rule(SCMP_SYS(tgkill), SCMP_CMP32(0, SCMP_CMP_EQ, 0xCAFEAFFE),
						                                  SCMP_CMP32(2, SCMP_CMP_EQ, SIGRTMIN));

						/* The clone syscall must be made safe by restricting parameters
						 * The specified flags only allow creation of new threads. */
						_add_allow_rule(SCMP_SYS(clone), SCMP_CMP32(0, SCMP_CMP_EQ, clone_flags));

						/* The nmap2 syscall has a different name on different architectures
						 * but it slould be save as it only uses an already open socket. */
						_add_allow_rule(SCMP_SYS(mmap2));

						/* This syscall is only used on ARM. */
						_add_allow_rule(SCMP_SYS(cacheflush));

						/* returning from signal handlers is safe */
						_add_allow_rule(SCMP_SYS(sigreturn));
					}
					break;
				default:
					fprintf(stderr, "Unsupported architecture\n");
					throw -104;
			}

			/* build and export */
			seccomp_export_bpf(_ctx, 1);

			return 0;
		}
};

