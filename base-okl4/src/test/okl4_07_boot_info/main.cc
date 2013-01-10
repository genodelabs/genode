/*
 * \brief  Test for parsing OKL4 boot information
 * \author Norman Feske
 * \date   2009-03-24
 *
 * This program can be started as roottask replacement directly on
 * the OKL4 kernel. It determines the available memory resources
 * and boot-time data modules.
 */

/*
 * Copyright (C) 2009-2013 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <base/printf.h>

/* local includes */
#include "../mini_env.h"

/* OKL4 includes */
namespace Okl4 { extern "C" {
#include <l4/utcb.h>
#include <l4/schedule.h>
#include <bootinfo/bootinfo.h>
} }

using namespace Okl4;
using namespace Genode;

static int init_mem(uintptr_t virt_base, uintptr_t virt_end,
                    uintptr_t phys_base, uintptr_t phys_end,
                    const bi_user_data_t * data)
{
	printf("init_mem: virt=[%lx,%lx), phys=[%lx,%lx)\n",
	       virt_base, virt_end, phys_base, phys_end);
	return 0;
}


static int add_virt_mem(bi_name_t pool, uintptr_t base, uintptr_t end,
                        const bi_user_data_t * data)
{
	printf("add_virt_mem: pool=%d region=[%lx,%lx]\n", pool, base, end);
	return 0;
}


static int add_phys_mem(bi_name_t pool, uintptr_t base, uintptr_t end,
                        const bi_user_data_t * data)
{
	printf("add_phys_mem: pool=%d region=[%lx,%lx]\n", pool, base, end);
	return 0;
}


static int export_object(bi_name_t pd, bi_name_t obj,
                         bi_export_type_t export_type, char *key,
                         Okl4::size_t key_len,
                         const bi_user_data_t * data)
{
	printf("export_object: pd=%d obj=%d type=%d key=\"%s\"\n",
	       pd, obj, export_type, key);
	return 0;
}


static bi_name_t new_ms(bi_name_t owner, uintptr_t base, uintptr_t size,
                        uintptr_t flags, uintptr_t attr, bi_name_t physpool,
                        bi_name_t virtpool, bi_name_t zone,
                        const bi_user_data_t * data)
{
	printf("new_ms: owner=%d region=[%lx,%lx), flags=%lx, attr=%lx, physpool=%d, virtpool=%d, zone=%d\n",
	       owner, base, base + size - 1, flags, attr, physpool, virtpool, zone);
	return 0;
}


/**
 * Main program
 */
int main()
{
	L4_Word_t boot_info_addr;
	L4_StoreMR(1, &boot_info_addr);
	printf("boot info at 0x%lx\n", boot_info_addr);

	printf("parsing boot info...\n");
	static bi_user_data_t user_data;
	static bi_callbacks_t callbacks;
	callbacks.init_mem      = init_mem;
	callbacks.add_virt_mem  = add_virt_mem;
	callbacks.add_phys_mem  = add_phys_mem;
	callbacks.export_object = export_object;
	callbacks.new_ms        = new_ms;
	int ret = bootinfo_parse((void *)boot_info_addr, &callbacks, &user_data);

	printf("finished parsing of boot info with ret=%d, exiting main()\n", ret);
	return 0;
}
