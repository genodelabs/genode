#include <env.h>
#include <rm.h>

namespace Fiasco {
#include <l4/util/util.h>
#include <l4/sys/linkage.h>
}

extern "C" L4_CV int l4x_forward_pf(Fiasco::l4_umword_t addr,
                              Fiasco::l4_umword_t pc, int extra_write)
{
	using namespace Fiasco;

	Genode::size_t size          = L4_PAGESIZE;
	Genode::addr_t ds_start_addr = addr;
	L4lx::Region *r = L4lx::Env::env()->rm()->find_region(&ds_start_addr, &size);
	L4lx::Dataspace *ds = r ? r->ds() : 0;
	if (ds && !ds->map(addr - r->addr()))
		return 0;

	if (!extra_write)
		l4_touch_ro((void*)l4_trunc_page(addr), L4_LOG2_PAGESIZE);
	else
		l4_touch_rw((void*)l4_trunc_page(addr), L4_LOG2_PAGESIZE);
	return 1;
}


