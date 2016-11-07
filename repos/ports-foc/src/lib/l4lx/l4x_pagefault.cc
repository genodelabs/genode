#include <env.h>
#include <rm.h>

#include <vcpu.h>
#include <linux.h>

namespace Fiasco {
#include <genode/net.h>
#include <l4/sys/irq.h>
#include <l4/util/util.h>
#include <l4/sys/linkage.h>
}

static bool ballooning = false;
static Genode::Lock balloon_lock;

namespace {

	class Signal_thread : public Genode::Thread_deprecated<8192>
	{
		private:

			Fiasco::l4_cap_idx_t _cap;
			Genode::Lock        *_sync;

		protected:

			void entry()
			{
				using namespace Fiasco;
				using namespace Genode;

				Signal_receiver           receiver;
				Signal_context            rx;
				Signal_context_capability cap(receiver.manage(&rx));
				Genode::env()->parent()->yield_sigh(cap);
				_sync->unlock();

				while (true) {
					receiver.wait_for_signal();
					Genode::env()->parent()->yield_request();
					{
						Genode::Lock::Guard guard(balloon_lock);
						ballooning = true;
						if (l4_error(l4_irq_trigger(_cap)) != -1)
							PWRN("IRQ net trigger failed\n");
					}
				}
			}

		public:

			Signal_thread(Fiasco::l4_cap_idx_t cap, Genode::Lock *sync)
			: Genode::Thread_deprecated<8192>("net-signal-thread"), _cap(cap), _sync(sync) {
				start(); }
	};
}


extern "C" {

L4_CV int l4x_forward_pf(Fiasco::l4_umword_t addr,
						 Fiasco::l4_umword_t pc, int extra_write)
{
	using namespace Fiasco;

	Genode::size_t size          = L4_PAGESIZE;
	Genode::addr_t ds_start_addr = addr;
	L4lx::Region *r = L4lx::Env::env()->rm()->find_region(&ds_start_addr, &size);
	L4lx::Dataspace *ds = r ? r->ds() : 0;

	while (ds) {
		try {
			ds->map(addr - r->addr(), !ballooning);
			break;
		} catch(Genode::Rm_session::Attach_failed) {
			PWRN("Attach of chunk dataspace of failed");
			return 0;
		}
	}

	if (!extra_write)
		l4_touch_ro((void*)l4_trunc_page(addr), L4_LOG2_PAGESIZE);
	else
		l4_touch_rw((void*)l4_trunc_page(addr), L4_LOG2_PAGESIZE);
	return 1;
}


Fiasco::l4_cap_idx_t genode_balloon_irq_cap()
{
	Linux::Irq_guard guard;

	static Genode::Foc_native_cpu_client native_cpu(L4lx::cpu_connection()->native_cpu());
	static Genode::Native_capability cap = native_cpu.alloc_irq();
	static Genode::Lock lock(Genode::Lock::LOCKED);
	static Fiasco::l4_cap_idx_t kcap = Genode::Capability_space::kcap(cap);
	static Signal_thread th(kcap, &lock);
	lock.lock();
	return kcap;
}


bool genode_balloon_free_chunk(unsigned long addr)
{
	Linux::Irq_guard guard;

	Genode::addr_t ds_start_addr = addr;
	Genode::size_t size          = L4_PAGESIZE;
	L4lx::Region *r = L4lx::Env::env()->rm()->find_region(&ds_start_addr, &size);
	L4lx::Dataspace *ds = r ? r->ds() : 0;
	return ds ? ds->free(addr - r->addr()) : false;
}


void genode_balloon_free_done()
{
	Linux::Irq_guard ig;
	Genode::Lock::Guard guard(balloon_lock);
	ballooning = false;
	Genode::env()->parent()->yield_response();
}

}
