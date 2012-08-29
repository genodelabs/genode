/*
 * \brief  Startup code for L4Linux.
 * \author Stefan Kalkowski
 * \date   2011-04-11
 */

/*
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/elf.h>
#include <base/env.h>
#include <base/thread.h>
#include <base/native_types.h>
#include <dataspace/client.h>
#include <rom_session/connection.h>
#include <foc_cpu_session/connection.h>
#include <util/misc_math.h>
#include <os/config.h>

/* L4lx includes */
#include <env.h>

namespace Fiasco {
#include <l4/re/env.h>
#include <l4/sys/consts.h>
#include <l4/sys/utcb.h>
#include <l4/sys/types.h>
#include <l4/util/util.h>
}

extern void* _prog_img_end; /* binary end in virtual memory */
extern void* _prog_img_beg; /* binary start in virtual memory */
extern void* l4lx_kinfo;    /* pointer to the KIP */

extern "C" int linux_main(int argc, char **argv); /* l4linux entry function */

static const bool DEBUG = false;

static void parse_cmdline(char*** cmd, int *num)
{
	using namespace Genode;

	enum { MAX_CMDLINE_LEN = 256, MAX_ARGS = 128 };

	static char  arg_str[MAX_CMDLINE_LEN];
	static char* words[MAX_ARGS];

	try {
		config()->xml_node().attribute("args").value(arg_str, sizeof(arg_str));
	} catch(...) {
		PWRN("Couldn't parse commandline from config!");
		arg_str[0] = 0;
	}

	if (DEBUG)
		PDBG("Read the following commandline from config: %s", arg_str);

	unsigned i = 1;
	words[0] = (char*) "vmlinux";
	for (char* start = &arg_str[0], *ptr = start; i < MAX_ARGS; ptr++) {

		if (*ptr != 0 && *ptr != ' ')
			continue;

		words[i]   = start;
		*ptr++     = 0;
		start      = ptr;
		words[++i] = 0;

		if (*ptr == 0)
			break;

		for (; *ptr == ' '; ptr++) ;
	}

	*cmd = words;
	*num = i;
}


static void map_kip()
{
	using namespace Genode;

	/* Open the KIP special file and keep it open */
	Genode::Rom_connection kip_rom("kip");
	kip_rom.on_destruction(Genode::Rom_connection::KEEP_OPEN);

	/* Attach and register dataspace */
	l4lx_kinfo = L4lx::Env::env()->rm()->attach(kip_rom.dataspace(), "KIP");

	/* map it right now */
	Fiasco::l4_touch_ro(l4lx_kinfo, L4_PAGESIZE);
}


static void prepare_l4re_env()
{
	using namespace Fiasco;

	Genode::Foc_cpu_connection cpu;
	static Genode::Native_capability main_thread_cap
		= cpu.native_cap(Genode::env()->cpu_session()->first());
	l4re_env_t *env = l4re_env();
	env->first_free_utcb = (l4_addr_t)l4_utcb() + L4_UTCB_OFFSET;
	env->utcb_area       = l4_fpage((l4_addr_t)l4_utcb(),
	                                Genode::log2(L4_UTCB_OFFSET * L4lx::THREAD_MAX),
	                                L4_CAP_FPAGE_RW);
	env->factory         = L4_INVALID_CAP;
	env->scheduler       = L4_BASE_SCHEDULER_CAP;
	env->mem_alloc       = L4_INVALID_CAP;
	env->log             = L4_INVALID_CAP;
	env->main_thread     = main_thread_cap.dst();
	env->rm              = Fiasco::THREADS_BASE_CAP + Fiasco::THREAD_PAGER_CAP;
}


static void register_reserved_areas()
{
	using namespace Genode;

	size_t bin_sz = (addr_t)&_prog_img_end - (addr_t)&_prog_img_beg;
	L4lx::Env::env()->rm()->reserve_range((addr_t)&_prog_img_beg, bin_sz, "Binary");
	L4lx::Env::env()->rm()->reserve_range(Native_config::context_area_virtual_base(),
	                                      Native_config::context_area_virtual_size(),
	                                      "Thread Context Area");
}


int main(int, char**)
{
	int    cmd_num = 0;
	char** cmdline = 0;

	PINF("Booting L4Linux ...");

	register_reserved_areas();
	map_kip();
	prepare_l4re_env();
	parse_cmdline(&cmdline, &cmd_num);

	if (DEBUG)
		L4lx::Env::env()->rm()->dump();

	return linux_main(cmd_num, cmdline);
}
