/*
 * \brief  Platform dependant hook after binary ready
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

/* Genode includes */
#include <base/log.h>

/* base-internal includes */
#include <base/internal/globals.h>
#include <linux_syscalls.h>
#include <errno.h>
#include <sys/prctl.h>     /* prctl */
#include <linux/seccomp.h> /* seccomp's constants */

using namespace Genode;

extern char _binary_seccomp_bpf_policy_bin_start[];
extern char _binary_seccomp_bpf_policy_bin_end[];

struct bpf_program {
  Genode::uint16_t blk_cnt;
  Genode::uint64_t* blks;
};

void Genode::binary_ready_hook_for_platform()
{
	if (lx_prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
		error("PR_SET_NO_NEW_PRIVS failed");
		throw Exception();
	}

	for (char* i = _binary_seccomp_bpf_policy_bin_start;
	     i < _binary_seccomp_bpf_policy_bin_end - sizeof(Genode::uint32_t); i++) {
		Genode::uint32_t* v = reinterpret_cast<Genode::uint32_t*>(i);
		if (*v == 0xCAFEAFFE) {
			*v = lx_getpid();
		}
	}

	bpf_program program;
	program.blk_cnt = (_binary_seccomp_bpf_policy_bin_end -
	                   _binary_seccomp_bpf_policy_bin_start) /
	                   sizeof(Genode::uint64_t);
	program.blks = (Genode::uint64_t*)_binary_seccomp_bpf_policy_bin_start;
	Genode::uint64_t flags = SECCOMP_FILTER_FLAG_TSYNC;
	auto ret = lx_seccomp(SECCOMP_SET_MODE_FILTER, flags, &program);
	if (ret != 0) {
		error("SECCOMP_SET_MODE_FILTER failed ", ret);
		throw Exception();
	}
}

