/*
 * \brief  Vancouver main program for Genode
 * \author Norman Feske
 * \author Markus Partheymueller
 * \date   2011-11-18
 *
 * Important remark about debugging output:
 *
 * Most of the code within this file is called during virtualization event
 * handling. NOVA's virtualization fault mechanism carries information about
 * the fault cause and fault resolution in the UTCB of the VCPU handler EC.
 * Consequently, the code involved in the fault handling is expected to
 * preserve the UTCB content. I.e., it must not involve the use of IPC, which
 * employs the UTCB to carry IPC payload. Because Genode's debug-output macros
 * use the remote LOG service via IPC as back end, those macros must not be
 * used directly. Instead, the 'Logging::printf' function should be used, which
 * takes care about saving and restoring the UTCB.
 */

/*
 * Copyright (C) 2011-2013 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * The code is partially based on the Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/touch.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/cap_sel_alloc.h>
#include <base/thread.h>
#include <base/rpc_server.h>
#include <base/native_types.h>
#include <util/misc_math.h>
#include <rom_session/connection.h>
#include <rm_session/connection.h>
#include <cap_session/connection.h>
#include <base/allocator_avl.h>
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <os/config.h>
#include <os/alarm.h>
#include <os/synced_interface.h>
#include <timer_session/connection.h>
#include <rtc_session/connection.h>

/* NOVA includes that come with Genode */
#include <nova/syscalls.h>

/* NOVA userland includes */
#include <nul/vcpu.h>
#include <nul/motherboard.h>
#include <sys/hip.h>

/* local includes */
#include <synced_motherboard.h>
#include <device_model_registry.h>
#include <boot_module_provider.h>
#include <console.h>
#include <network.h>
#include <disk.h>

enum {
	PAGE_SIZE_LOG2 = 12UL,
	PAGE_SIZE      = 1UL << PAGE_SIZE_LOG2,
	STACK_SIZE     = 1024*sizeof(Genode::addr_t),
};


enum { verbose_debug = false };
enum { verbose_npt   = false };
enum { verbose_io    = false };

static Genode::Native_utcb utcb_backup;
Genode::Lock *utcb_lock()
{
	static Genode::Lock inst;
	return &inst;
}


/* timer service */
using Genode::Thread;
using Genode::Alarm_scheduler;
using Genode::Alarm;

typedef Genode::Synced_interface<TimeoutList<32, void> > Synced_timeout_list;

class Alarm_thread : Thread<4096>, public Alarm_scheduler
{
	private:

		Timer::Connection    _timer;
		Alarm::Time          _curr_time;   /* jiffies value */

		Synced_motherboard  &_motherboard;
		Synced_timeout_list &_timeouts;

		/**
		 * Thread entry function
		 */
		void entry()
		{
			while (true) {
				unsigned long long now = _motherboard()->clock()->time();
				unsigned nr;

				while ((nr = _timeouts()->trigger(now))) {

					MessageTimeout msg(nr, _timeouts()->timeout());

					if (_timeouts()->cancel(nr) < 0)
						Logging::printf("Timeout not cancelled.\n");

					_motherboard()->bus_timeout.send(msg);
				}

				_timer.usleep(1000);
			}
		}

	public:

		/**
		 * Constructor
		 */
		Alarm_thread(Synced_motherboard &mb, Synced_timeout_list &timeouts)
		: Thread("alarm"), _curr_time(0), _motherboard(mb), _timeouts(timeouts)
		{ start(); }

		Alarm::Time curr_time() { return _curr_time; }
		unsigned long long curr_time_long() { return _motherboard()->clock()->time(); }
};


/**
 * Representation of guest memory
 *
 * The VMM and the guest share the same PD. However, the guest's view on the PD
 * is restricted to the guest-physical-to-VMM-local mappings installed by the
 * VMM for the VCPU's EC.
 *
 * The guest memory is shadowed at the lower portion of the VMM's address
 * space. If the guest (the VCPU EC) tries to access a page that has no mapping
 * in the VMM's PD, NOVA does not generate a page-fault (which would be
 * delivered to the pager of the VMM, i.e., core) but it produces a NPT
 * virtualization event handled locally by the VMM. The NPT event handler is
 * the '_svm_npt' function.
 */
class Guest_memory
{
	private:

		/*
		 * The '_reservation' is a managed dataspace that occupies the lower
		 * part of the address space, which contains the shadow of the VCPU's
		 * physical memory.
		 */
		Genode::Rm_connection _reservation;

		Genode::Ram_dataspace_capability _ds;
		Genode::Ram_dataspace_capability _fb_ds;

		Genode::size_t _fb_size;

		char *_local_addr;
		char *_fb_addr;

	public:

		/**
		 * Number of bytes that are available to the guest
		 *
		 * At startup time, some device models (i.e., the VGA controller) claim
		 * a bit of guest-physical memory for their respective devices (i.e.,
		 * the virtual frame buffer) by calling 'OP_ALLOC_FROM_GUEST'. This
		 * function allocates such blocks from the end of the backing store.
		 * The 'remaining_size' contains the number of bytes left at the lower
		 * part of the backing store for the use as  normal guest-physical RAM.
		 * It is initialized with the actual backing store size and then
		 * managed by the 'OP_ALLOC_FROM_GUEST' handler.
		 */
		Genode::size_t remaining_size;

		/**
		 * Constructor
		 *
		 * \param backing_store_size  number of bytes of physical RAM to be
		 *                            used as guest-physical and device memory,
		 *                            allocated from core's RAM service
		 */
		Guest_memory(Genode::size_t backing_store_size, Genode::size_t fb_size)
		:
			_fb_size(fb_size),
			_reservation(0, backing_store_size),
			_ds(Genode::env()->ram_session()->alloc(backing_store_size-fb_size)),
			_fb_ds(Genode::env()->ram_session()->alloc(fb_size)),
			_local_addr(0),
			_fb_addr(0),
			remaining_size(backing_store_size-fb_size)
		{
			try {
				/* free up preliminary mapping to reserve lower address space */
				Genode::env()->rm_session()->detach(PAGE_SIZE);

				/*
				 * Attach reservation to the beginning of the local address space.
				 * We leave out the very first page because core denies the
				 * attachment of anything at the zero page.
				 */
				Genode::env()->rm_session()->attach_at(_reservation.dataspace(),
				                               PAGE_SIZE, 0, PAGE_SIZE);

				/*
				 * RAM used as backing store for guest-physical memory
				 */
				_local_addr = Genode::env()->rm_session()->attach(_ds);
				_fb_addr = Genode::env()->rm_session()->attach_at(_fb_ds,
				        ((Genode::addr_t) _local_addr)+backing_store_size-fb_size);

			} catch (Genode::Rm_session::Region_conflict) {
				PERR("region conflict");
			}
		}

		~Guest_memory()
		{
			/* detach reservation */
			Genode::env()->rm_session()->detach((void *)PAGE_SIZE);

			/* detach and free backing store */
			Genode::env()->rm_session()->detach((void *)_local_addr);
			Genode::env()->ram_session()->free(_ds);

			Genode::env()->rm_session()->detach((void *)_fb_addr);
			Genode::env()->ram_session()->free(_fb_ds);
		}

		/**
		 * Return pointer to locally mapped backing store
		 */
		char *backing_store_local_base()
		{
			return _local_addr;
		}

		/**
		 * Return pointer to lo locally mapped fb backing store
		 */
		char *backing_store_fb_local_base()
		{
			return _fb_addr;
		}

		Genode::size_t fb_size() { return _fb_size; }

		Genode::Dataspace_capability fb_ds() { return _fb_ds; }
};


class Vcpu_thread : Genode::Thread<STACK_SIZE>
{
	private:

		/**
		 * Log2 size of portal window used for virtualization events
		 */
		enum { VCPU_EXC_BASE_LOG2 = 8 };

	public:

		Vcpu_thread(char const * name) : Thread(name)
		{
			/* release pre-allocated selectors of Thread */
			Genode::cap_selector_allocator()->
			        free(tid().exc_pt_sel,
				     Nova::NUM_INITIAL_PT_LOG2);

			/* allocate correct number of selectors */
			tid().exc_pt_sel = Genode::cap_selector_allocator()->
			        alloc(VCPU_EXC_BASE_LOG2);

			/* tell generic thread code that this becomes a vCPU */
			tid().is_vcpu = true;

		}

		~Vcpu_thread()
		{
			using namespace Nova;

			revoke(Obj_crd(tid().exc_pt_sel, VCPU_EXC_BASE_LOG2));
			Genode::cap_selector_allocator()->
			        free(tid().exc_pt_sel, VCPU_EXC_BASE_LOG2);

			/* allocate selectors for ~Thread */
			tid().exc_pt_sel = Genode::cap_selector_allocator()->
			        alloc(NUM_INITIAL_PT_LOG2);
		}

		Genode::addr_t exc_base() { return tid().exc_pt_sel; }

		void start(Genode::addr_t sel_ec)
		{
			this->Thread_base::start();

			using namespace Genode;

			/*
			 * Request native EC thread cap and put it next to the
			 * SM cap - see Vcpu_dispatcher->sel_sm_ec description
			 */
			request_native_ec_cap(_pager_cap, sel_ec);
		}

		void entry() { }
};


class Vcpu_dispatcher : public Genode::Thread<STACK_SIZE>,
                        public StaticReceiver<Vcpu_dispatcher>
{
	private:

		Genode::Cap_connection _cap_session;

		/**
		 * Pointer to corresponding VCPU model
		 */
		Genode::Synced_interface<VCpu> _vcpu;

		Vcpu_thread _vcpu_thread;

		/**
		 * Guest-physical memory
		 */
		Guest_memory &_guest_memory;

		/**
		 * Motherboard representing the inter-connections of all device models
		 */
		Synced_motherboard &_motherboard;


		/***************
		 ** Shortcuts **
		 ***************/

		static Nova::mword_t _alloc_sel(Genode::size_t num_caps_log2 = 0) {
			return Genode::cap_selector_allocator()->alloc(num_caps_log2); }

		static void _free_sel(Nova::mword_t sel, Genode::size_t num_caps_log2 = 0) {
			Genode::cap_selector_allocator()->free(sel, num_caps_log2); }

		static ::Utcb *_utcb_of_myself() {
			return (::Utcb *)Genode::Thread_base::myself()->utcb(); }


		/***********************************
		 ** Virtualization event handlers **
		 ***********************************/

		static void _skip_instruction(CpuMessage &msg)
		{
			/* advance EIP */
			assert(msg.mtr_in & MTD_RIP_LEN);
			msg.cpu->eip += msg.cpu->inst_len;
			msg.mtr_out |= MTD_RIP_LEN;

			/* cancel sti and mov-ss blocking as we emulated an instruction */
			assert(msg.mtr_in & MTD_STATE);
			if (msg.cpu->intr_state & 3) {
				msg.cpu->intr_state &= ~3;
				msg.mtr_out |= MTD_STATE;
			}
		}

		enum Skip { SKIP = true, NO_SKIP = false };

		void _handle_vcpu(Skip skip, CpuMessage::Type type)
		{
			Utcb *utcb = _utcb_of_myself();

			CpuMessage msg(type, static_cast<CpuState *>(utcb), utcb->mtd);

			if (skip == SKIP)
				_skip_instruction(msg);

			/**
			 * Send the message to the VCpu.
			 */
			if (!_vcpu()->executor.send(msg, true))
				Logging::panic("nobody to execute %s at %x:%x\n",
				               __func__, msg.cpu->cs.sel, msg.cpu->eip);

			/**
			 * Check whether we should inject something...
			 */
			if (msg.mtr_in & MTD_INJ && msg.type != CpuMessage::TYPE_CHECK_IRQ) {
				msg.type = CpuMessage::TYPE_CHECK_IRQ;
				if (!_vcpu()->executor.send(msg, true))
					Logging::panic("nobody to execute %s at %x:%x\n",
					               __func__, msg.cpu->cs.sel, msg.cpu->eip);
			}

			/**
			 * If the IRQ injection is performed, recalc the IRQ window.
			 */
			if (msg.mtr_out & MTD_INJ) {
				msg.type = CpuMessage::TYPE_CALC_IRQWINDOW;
				if (!_vcpu()->executor.send(msg, true))
					Logging::panic("nobody to execute %s at %x:%x\n",
					               __func__, msg.cpu->cs.sel, msg.cpu->eip);
			}

			msg.cpu->mtd = msg.mtr_out;
		}

		/**
		 * Get position of the least significant 1 bit.
		 * bsf is undefined for value == 0.
		 */
		Genode::addr_t bsf(Genode::addr_t value) {
		        return __builtin_ctz(value); }

		bool max_map_crd(Nova::Mem_crd &crd, Genode::addr_t vmm_start,
		                 Genode::addr_t vm_start, Genode::addr_t size,
		                 Genode::addr_t vm_fault)
		{
			Nova::Mem_crd crd_save = crd;

			retry:

			/* lookup whether page is mapped and its size */
			Nova::uint8_t ret = Nova::lookup(crd);
			if (ret != Nova::NOVA_OK)
				return false;

			/* page is not mapped, touch it */
			if (crd.is_null()) {
				crd = crd_save;
				Genode::touch_read((unsigned char volatile *)crd.addr());
				goto retry;
			}

			/* cut-set crd region and vmm region */
			Genode::addr_t cut_start = Genode::max(vmm_start, crd.base());
			Genode::addr_t cut_size  = Genode::min(vmm_start + size,
			                           crd.base() + (1UL << crd.order())) -
			                           cut_start;

			/* calculate minimal order of page to be mapped */
			Genode::addr_t map_page  = vmm_start + vm_fault - vm_start;
			Genode::addr_t map_order = bsf(vm_fault | map_page | cut_size);

			Genode::addr_t hotspot = 0;

			/* calculate maximal aligned order of page to be mapped */
			do {
				crd = Nova::Mem_crd(map_page, map_order,
				                    crd_save.rights());

				map_order += 1;
				map_page  &= ~((1UL << map_order) - 1);
				hotspot    = vm_start + map_page - vmm_start;
			}
			while (cut_start <= map_page &&
			      ((map_page + (1UL << map_order)) <= (cut_start + cut_size)) &&
			      !(hotspot & ((1UL << map_order) - 1)));

			return true;
		}

		bool _handle_map_memory(bool need_unmap)
		{
			Utcb *utcb = _utcb_of_myself();
			Genode::addr_t const vm_fault_addr = utcb->qual[1];

			if (verbose_npt)
				Logging::printf("--> request mapping at 0x%lx\n", vm_fault_addr);

			MessageMemRegion mem_region(vm_fault_addr >> PAGE_SIZE_LOG2);

			if (!_motherboard()->bus_memregion.send(mem_region, false) ||
			    !mem_region.ptr)
				return false;

			if (verbose_npt)
				Logging::printf("VM page 0x%lx in [0x%lx:0x%lx),"
				                " VMM area: [0x%lx:0x%lx)\n",
				                mem_region.page, mem_region.start_page,
				                mem_region.start_page + mem_region.count,
				                (Genode::addr_t)mem_region.ptr >> PAGE_SIZE_LOG2,
				                ((Genode::addr_t)mem_region.ptr >> PAGE_SIZE_LOG2)
				                + mem_region.count);

			Genode::addr_t vmm_memory_base =
			        reinterpret_cast<Genode::addr_t>(mem_region.ptr);
			Genode::addr_t vmm_memory_fault = vmm_memory_base +
			        (vm_fault_addr - (mem_region.start_page << PAGE_SIZE_LOG2));

			bool read=true, write=true, execute=true;
                        /* XXX: Not yet supported by Vancouver.
			if (mem_region.attr == (DESC_TYPE_MEM | DESC_RIGHT_R)) {
				if (verbose_npt)
					Logging::printf("Mapping readonly to %p (err:%x, attr:%x)\n",
						vm_fault_addr, utcb->qual[0], mem_region.attr);
				write = execute = false;
			}*/

			Nova::Mem_crd crd(vmm_memory_fault >> PAGE_SIZE_LOG2, 0,
			                  Nova::Rights(read, write, execute));

			if (!max_map_crd(crd, vmm_memory_base >> PAGE_SIZE_LOG2,
			                 mem_region.start_page,
			                 mem_region.count, mem_region.page))
				Logging::panic("mapping failed");

			if (need_unmap)
				Logging::panic("_handle_map_memory: need_unmap not handled, yet\n");

			Genode::addr_t hotspot = (mem_region.start_page << PAGE_SIZE_LOG2)
			                         + crd.addr() - vmm_memory_base;

			if (verbose_npt)
				Logging::printf("NPT mapping (base=0x%lx, order=%d, hotspot=0x%lx)\n",
				                crd.base(), crd.order(), hotspot);

			utcb->mtd = 0;

			/* EPT violation during IDT vectoring? */
			if (utcb->inj_info & 0x80000000) {
				utcb->mtd |= MTD_INJ;
				Logging::printf("EPT violation during IDT vectoring.\n");
				CpuMessage _win(CpuMessage::TYPE_CALC_IRQWINDOW,
				                static_cast<CpuState *>(utcb), utcb->mtd);
				_win.mtr_out = MTD_INJ;
				if (!_vcpu()->executor.send(_win, true))
					Logging::panic("nobody to execute %s at %x:%x\n",
					               __func__, utcb->cs.sel, utcb->eip);
			}

			Nova::Utcb * u = (Nova::Utcb *)utcb;
			u->set_msg_word(0);
			if (!u->append_item(crd, hotspot, false, true))
				Logging::printf("Could not map everything");

			return true;
		}

		void _handle_io(bool is_in, unsigned io_order, unsigned port)
		{
			if (verbose_io)
				Logging::printf("--> I/O is_in=%d, io_order=%d, port=%x\n",
				                is_in, io_order, port);

			Utcb *utcb = _utcb_of_myself();
			CpuMessage msg(is_in, static_cast<CpuState *>(utcb), io_order,
			               port, &utcb->eax, utcb->mtd);
			_skip_instruction(msg);
			{
				if (!_vcpu()->executor.send(msg, true))
					Logging::panic("nobody to execute %s at %x:%x\n",
					               __func__, msg.cpu->cs.sel, msg.cpu->eip);
			}

			utcb->mtd = msg.mtr_out;
		}

		/* SVM portal functions */
		void _svm_startup()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_CHECK_IRQ);
		}

		void _svm_npt()
		{
			Utcb *utcb = _utcb_of_myself();
			MessageMemRegion msg(utcb->qual[1] >> PAGE_SIZE_LOG2);
			if (!_handle_map_memory(utcb->qual[0] & 1))
				_svm_invalid();
		}

		void _svm_invalid()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_SINGLE_STEP);
			Utcb *utcb = _utcb_of_myself();
			utcb->mtd |= MTD_CTRL;
			utcb->ctrl[0] = 1 << 18; /* cpuid */
			utcb->ctrl[1] = 1 << 0;  /* vmrun */
		}

		void _svm_ioio()
		{
			Utcb *utcb = _utcb_of_myself();

			if (utcb->qual[0] & 0x4) {
				Logging::printf("invalid gueststate\n");
				utcb->ctrl[1] = 0;
				utcb->mtd = MTD_CTRL;
			} else {
				unsigned order = ((utcb->qual[0] >> 4) & 7) - 1;

				if (order > 2)
					order = 2;

				utcb->inst_len = utcb->qual[1] - utcb->eip;

				_handle_io(utcb->qual[0] & 1, order, utcb->qual[0] >> 16);
			}
		}

		void _svm_cpuid()
		{
			Utcb *utcb = _utcb_of_myself();
			utcb->inst_len = 2;
			_handle_vcpu(SKIP, CpuMessage::TYPE_CPUID);
		}

		void _svm_hlt()
		{
			Utcb *utcb = _utcb_of_myself();
			utcb->inst_len = 1;
			_vmx_hlt();
		}

		void _svm_msr()
		{
			_svm_invalid();
		}

		void _recall()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_CHECK_IRQ);
		}

		/* VMX portal functions */
		void _vmx_triple()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_TRIPLE);
		}

		void _vmx_init()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_INIT);
		}

		void _vmx_irqwin()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_CHECK_IRQ);
		}

		void _vmx_hlt()
		{
			_handle_vcpu(SKIP, CpuMessage::TYPE_HLT);
		}

		void _vmx_rdtsc()
		{
			_handle_vcpu(SKIP, CpuMessage::TYPE_RDTSC);
		}

		void _vmx_vmcall()
		{
			Utcb *utcb = _utcb_of_myself();
			utcb->eip += utcb->inst_len;
		}

		void _vmx_pause()
		{
			Utcb *utcb = _utcb_of_myself();
			CpuMessage msg(CpuMessage::TYPE_SINGLE_STEP,
			               static_cast<CpuState *>(utcb), utcb->mtd);
			_skip_instruction(msg);
		}

		void _vmx_invalid()
		{
			Utcb *utcb = _utcb_of_myself();
			utcb->efl |= 2;
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_SINGLE_STEP);
			utcb->mtd |= MTD_RFLAGS;
		}

		void _vmx_startup()
		{
			Utcb *utcb = _utcb_of_myself();
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_HLT);
			utcb->mtd |= MTD_CTRL;
			utcb->ctrl[0] = 0;
			utcb->ctrl[1] = 0;
		}

		void _vmx_recall()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_CHECK_IRQ);
		}

		void _vmx_ioio()
		{
			Utcb *utcb = _utcb_of_myself();
			unsigned order;
			if (utcb->qual[0] & 0x10) {
				Logging::printf("invalid gueststate\n");
				assert(utcb->mtd & MTD_RFLAGS);
				utcb->efl &= ~2;
				utcb->mtd = MTD_RFLAGS;
			} else {
				order = utcb->qual[0] & 7;
				if (order > 2) order = 2;
			}

			_handle_io(utcb->qual[0] & 8, order, utcb->qual[0] >> 16);
		}

		void _vmx_mmio()
		{
			Utcb *utcb = _utcb_of_myself();

			if (!_handle_map_memory(utcb->qual[0] & 0x38))
				/* this is an access to MMIO */
				_handle_vcpu(NO_SKIP, CpuMessage::TYPE_SINGLE_STEP);
		}

		void _vmx_cpuid()
		{
			_handle_vcpu(SKIP, CpuMessage::TYPE_CPUID);
		}

		void _vmx_msr_read()
		{
			_handle_vcpu(SKIP, CpuMessage::TYPE_RDMSR);
		}

		void _vmx_msr_write()
		{
			_handle_vcpu(SKIP, CpuMessage::TYPE_WRMSR);
		}

		/**
		 * Portal entry point entered on virtualization events
		 *
		 * For each event type used as argument of the '_register_handler'
		 * function template, the compiler automatically generates a separate
		 * instance of this function. The sole task of this function is to
		 * call the 'Vcpu' member function corresponding to the event type.
		 */
		template <unsigned EV, void (Vcpu_dispatcher::*FUNC)()>
		static void _portal_entry()
		{
			/* obtain this pointer of the event handler */
			Genode::Thread_base *myself = Genode::Thread_base::myself();
			Vcpu_dispatcher *vd = static_cast<Vcpu_dispatcher *>(myself);

			/* call event-specific handler function */
			(vd->*FUNC)();

			/* continue execution of the guest */
			Nova::reply(myself->stack_top());
		}

		/**
		 * Register virtualization event handler
		 */
		template <unsigned EV, void (Vcpu_dispatcher::*FUNC)()>
		void _register_handler(Genode::addr_t exc_base, Nova::Mtd mtd)
		{
			/*
			 * Let the compiler generate an instance of a portal
			 * entry
			 */
			void (*portal_entry)() = &_portal_entry<EV, FUNC>;

			using namespace Genode;

			/* Create the portal at the desired selector index */
			_cap_session.rcv_window(exc_base + EV);

			Native_capability thread(tid().ec_sel);
			Native_capability handler =
				_cap_session.alloc(thread, (Nova::mword_t)portal_entry, mtd.value());

			if (!handler.valid() || exc_base + EV != handler.local_name())
				Logging::panic("Could not get EC cap");
		}

	public:

		Vcpu_dispatcher(Genode::Lock           &vcpu_lock,
		                VCpu                   *unsynchronized_vcpu,
		                Guest_memory           &guest_memory,
		                Synced_motherboard     &motherboard,
		                bool                   has_svm,
		                bool                   has_vmx)
		:
			Thread("vcpu_dispatcher"),
			_vcpu(vcpu_lock, unsynchronized_vcpu),
			_vcpu_thread("vCPU thread"),
			_guest_memory(guest_memory),
			_motherboard(motherboard)
		{
			using namespace Genode;

			/* request creation of a 'local' EC */
			_tid.ec_sel = Native_thread::INVALID_INDEX - 1;
			Thread_base::start();

			using namespace Nova;

			/* shortcuts for common message-transfer descriptors */
			Mtd const mtd_all(Mtd::ALL);
			Mtd const mtd_cpuid(Mtd::EIP | Mtd::ACDB | Mtd::IRQ);
			Mtd const mtd_irq(Mtd::IRQ);

			/*
			 * Register vCPU event handlers
			 */
			typedef Vcpu_dispatcher This;
			if (has_svm) {
				Genode::addr_t exc_base =
					_vcpu_thread.exc_base();

				_register_handler<0x64, &This::_vmx_irqwin>
					(exc_base, MTD_IRQ);
				_register_handler<0x72, &This::_svm_cpuid>
					(exc_base, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_IRQ);
				_register_handler<0x78, &This::_svm_hlt>
					(exc_base, MTD_RIP_LEN | MTD_IRQ);
				_register_handler<0x7b, &This::_svm_ioio>
					(exc_base, MTD_RIP_LEN | MTD_QUAL | MTD_GPR_ACDB | MTD_STATE);
				_register_handler<0x7c, &This::_svm_msr>
					(exc_base, MTD_ALL);
				_register_handler<0x7f, &This::_vmx_triple>
					(exc_base, MTD_ALL);
				_register_handler<0xfc, &This::_svm_npt>
					(exc_base, MTD_ALL);
				_register_handler<0xfd, &This::_svm_invalid>
					(exc_base, MTD_ALL);
				_register_handler<0xfe, &This::_svm_startup>
					(exc_base, MTD_ALL);
				_register_handler<0xff, &This::_recall>
					(exc_base, MTD_IRQ);

			} else if (has_vmx) {
				Genode::addr_t exc_base =
					_vcpu_thread.exc_base();

				_register_handler<2, &This::_vmx_triple>
					(exc_base, MTD_ALL);
				_register_handler<3, &This::_vmx_init>
					(exc_base, MTD_ALL);
				_register_handler<7, &This::_vmx_irqwin>
					(exc_base, MTD_IRQ);
				_register_handler<10, &This::_vmx_cpuid>
					(exc_base, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_STATE);
				_register_handler<12, &This::_vmx_hlt>
					(exc_base, MTD_RIP_LEN | MTD_IRQ);
				_register_handler<16, &This::_vmx_rdtsc>
					(exc_base, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_TSC | MTD_STATE);
				_register_handler<18, &This::_vmx_vmcall>
					(exc_base, MTD_RIP_LEN | MTD_GPR_ACDB);
				_register_handler<30, &This::_vmx_ioio>
					(exc_base, MTD_RIP_LEN | MTD_QUAL | MTD_GPR_ACDB | MTD_STATE | MTD_RFLAGS);
				_register_handler<31, &This::_vmx_msr_read>
					(exc_base, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_TSC | MTD_SYSENTER | MTD_STATE);
				_register_handler<32, &This::_vmx_msr_write>
					(exc_base, MTD_RIP_LEN | MTD_GPR_ACDB | MTD_TSC | MTD_SYSENTER | MTD_STATE);
				_register_handler<33, &This::_vmx_invalid>
					(exc_base, MTD_ALL);
				_register_handler<40, &This::_vmx_pause>
					(exc_base, MTD_RIP_LEN | MTD_STATE);
				_register_handler<48, &This::_vmx_mmio>
					(exc_base, MTD_ALL);
				_register_handler<0xfe, &This::_vmx_startup>
					(exc_base, MTD_IRQ);
				_register_handler<0xff, &This::_vmx_recall>
					(exc_base, MTD_IRQ | MTD_RIP_LEN | MTD_GPR_ACDB | MTD_GPR_BSD);
			} else {

				/*
				 * We need Hardware Virtualization Features.
				 */
				Logging::panic("no SVM/VMX available, sorry");
			}

			/* let vCPU run */
			_vcpu_thread.start(sel_sm_ec() + 1);

			/* handle cpuid overrides */
			unsynchronized_vcpu->executor.add(this, receive_static<CpuMessage>);
		}

		/**
		 * Destructor
		 */
		~Vcpu_dispatcher() { }

		/**
		 * Unused member of the 'Thread_base' interface
		 *
		 * Similarly to how 'Rpc_entrypoints' are handled, a
		 * 'Vcpu_dispatcher' comes with a custom initialization
		 * procedure, which does not call the thread's normal entry
		 * function. Instead, the thread's EC gets associated with
		 * several portals, each for handling a specific virtualization
		 * event.
		 */
		void entry() { }

		/**
		 * Return capability selector of the VCPU's SM and EC
		 *
		 * The returned number corresponds to the VCPU's semaphore
		 * selector. The consecutive number corresponds to the EC. The
		 * number returned by this function is used by the VMM code as
		 * a unique identifier of the VCPU. I.e., it gets passed as
		 * arguments for 'MessageHostOp' operations.
		 */
		Nova::mword_t sel_sm_ec()
		{
			return tid().exc_pt_sel + Nova::SM_SEL_EC;
		}


		/***********************************
		 ** Handlers for 'StaticReceiver' **
		 ***********************************/

		bool receive(CpuMessage &msg)
		{
			if (msg.type != CpuMessage::TYPE_CPUID)
				return false;

			/*
			 * Linux kernels with guest KVM support compiled in, executed
			 * CPUID to query the presence of KVM.
			 */
			enum { CPUID_KVM_SIGNATURE = 0x40000000 };

			switch (msg.cpuid_index) {

			case CPUID_KVM_SIGNATURE:

				msg.cpu->eax = 0;
				msg.cpu->ebx = 0;
				msg.cpu->ecx = 0;
				msg.cpu->edx = 0;
				break;

			default:
				PDBG("CpuMessage::TYPE_CPUID index %x ignored, return true)",
				     msg.cpuid_index);
			}

			return true;
		}
};


const void * _forward_pkt;


class Machine : public StaticReceiver<Machine>
{
	private:

		Genode::Rom_connection _hip_rom;
		Hip * const            _hip;
		Clock                  _clock;
		Genode::Lock           _motherboard_lock;
		Motherboard            _unsynchronized_motherboard;
		Synced_motherboard     _motherboard;
		Genode::Lock           _timeouts_lock;
		TimeoutList<32, void>  _unsynchronized_timeouts;
		Synced_timeout_list    _timeouts;
		Guest_memory          &_guest_memory;
		Boot_module_provider  &_boot_modules;
		Alarm_thread          *_alarm_thread;

		bool                   _alloc_fb_mem; /* For detecting FB alloc message */

		Nic::Session          *_nic;
		Rtc::Session          *_rtc;

	public:

		/*********************************************
		 ** Callbacks registered at the motherboard **
		 *********************************************/

		bool receive(MessageHostOp &msg)
		{
			switch (msg.type) {

			/**
			 * Request available guest memory starting at specified address
			 */
			case MessageHostOp::OP_GUEST_MEM:

				if (verbose_debug)
					Logging::printf("OP_GUEST_MEM value=0x%lx\n", msg.value);

				if (_alloc_fb_mem) {
					msg.len = _guest_memory.fb_size();
					msg.ptr = _guest_memory.backing_store_local_base();
					_alloc_fb_mem = false;
					Logging::printf(" -> len=0x%lx, ptr=0x%p\n",
					                msg.len, msg.ptr);
					return true;
				}

				if (msg.value >= _guest_memory.remaining_size) {
					msg.value = 0;
				} else {
					msg.len = _guest_memory.remaining_size - msg.value;
					msg.ptr = _guest_memory.backing_store_local_base() + msg.value;
				}

				if (verbose_debug)
					Logging::printf(" -> len=0x%lx, ptr=0x%p\n",
					                msg.len, msg.ptr);
				return true;

			/**
			 * Cut off upper range of guest memory by specified amount
			 */
			case MessageHostOp::OP_ALLOC_FROM_GUEST:

				if (verbose_debug)
					Logging::printf("OP_ALLOC_FROM_GUEST\n");

				if (msg.value == _guest_memory.fb_size()) {
					_alloc_fb_mem = true;
					msg.phys = _guest_memory.remaining_size;
					return true;
				}

				if (msg.value > _guest_memory.remaining_size)
					return false;

				_guest_memory.remaining_size -= msg.value;

				msg.phys = _guest_memory.remaining_size;

				if (verbose_debug)
					Logging::printf("-> allocated from guest %08lx+%lx\n",
					                _guest_memory.remaining_size, msg.value);
				return true;

			case MessageHostOp::OP_VCPU_CREATE_BACKEND:
				{
					if (verbose_debug)
						Logging::printf("OP_VCPU_CREATE_BACKEND\n");

					Vcpu_dispatcher *vcpu_dispatcher =
						new Vcpu_dispatcher(_motherboard_lock, msg.vcpu,
						                    _guest_memory,
						                    _motherboard,
						                    _hip->has_feature_svm(), _hip->has_feature_vmx());

					msg.value = vcpu_dispatcher->sel_sm_ec();
					return true;
				}

			case MessageHostOp::OP_VCPU_RELEASE:

				if (verbose_debug)
					Logging::printf("OP_VCPU_RELEASE\n");

				if (msg.len) {
					if (Nova::sm_ctrl(msg.value, Nova::SEMAPHORE_UP) != 0) {
						Logging::printf("vcpu release: sm_ctrl failed\n");
						return false;
					}
				}
				return (Nova::ec_ctrl(msg.value + 1) == 0);

			case MessageHostOp::OP_VCPU_BLOCK:
				{
					if (verbose_debug)
						Logging::printf("OP_VCPU_BLOCK\n");

					_motherboard_lock.unlock();
					bool res = (Nova::sm_ctrl(msg.value, Nova::SEMAPHORE_DOWN) == 0);
					if (verbose_debug)
						Logging::printf("woke up from vcpu sem, block on global_lock\n");
					_motherboard_lock.lock();
					return res;
				}

			case MessageHostOp::OP_GET_MODULE:
				{
					/*
					 * Module indices start with 1
					 */
					if (msg.module == 0)
						return false;

					/*
					 * Message arguments
					 */
					int            const index    = msg.module - 1;
					char *         const data_dst = msg.start;
					Genode::size_t const dst_len  = msg.size;

					/*
					 * Copy module data to guest RAM
					 */
					Genode::size_t data_len = 0;
					try {
						data_len = _boot_modules.data(index, data_dst, dst_len);
					} catch (Boot_module_provider::Destination_buffer_too_small) {
						Logging::panic("could not load module, destination buffer too small\n");
						return false;
					} catch (Boot_module_provider::Module_loading_failed) {
						Logging::panic("could not load module %d,"
						               " unknown reason\n", index);
						return false;
					}

					/*
					 * Detect end of module list
					 */
					if (data_len == 0)
						return false;

					/*
					 * Determine command line offset relative to the start of
					 * the loaded boot module. The command line resides right
					 * behind the module data, aligned on a page boundary.
					 */
					Genode::addr_t const cmdline_offset =
						Genode::align_addr(data_len, PAGE_SIZE_LOG2);

					if (cmdline_offset >= dst_len) {
						Logging::printf("destination buffer too small for command line\n");
						return false;
					}

					/*
					 * Copy command line to guest RAM
					 */
					Genode::size_t const cmdline_len =
					        _boot_modules.cmdline(index, data_dst + cmdline_offset,
					                          dst_len - cmdline_offset);

					/*
					 * Return module size (w/o the size of the command line,
					 * the 'vbios_multiboot' is aware of the one-page gap
					 * between modules.
					 */
					msg.size    = data_len;
					msg.cmdline = data_dst + cmdline_offset;
					msg.cmdlen  = cmdline_len;

					return true;
				}
			case MessageHostOp::OP_GET_MAC:
				{
					Nic::Packet_allocator *tx_block_alloc =
						new (Genode::env()->heap()) Nic::Packet_allocator(Genode::env()->heap());

					enum {
						PACKET_SIZE = Nic::Packet_allocator::DEFAULT_PACKET_SIZE,
						BUF_SIZE    = Nic::Session::QUEUE_SIZE * PACKET_SIZE,
					};

					try {
						_nic = new Nic::Connection(tx_block_alloc, BUF_SIZE, BUF_SIZE);
					} catch (...) {
						Logging::printf("No NIC connection possible!\n");
						return false;
					}

					PINF("Our mac address is %2x:%2x:%2x:%2x:%2x:%2x\n",
					        _nic->mac_address().addr[0],
					        _nic->mac_address().addr[1],
					        _nic->mac_address().addr[2],
					        _nic->mac_address().addr[3],
					        _nic->mac_address().addr[4],
					        _nic->mac_address().addr[5]
					);
					msg.mac = ((Genode::uint64_t)_nic->mac_address().addr[0] & 0xff) << 40 |
					          ((Genode::uint64_t)_nic->mac_address().addr[1] & 0xff) << 32 |
					          ((Genode::uint64_t)_nic->mac_address().addr[2] & 0xff) << 24 |
					          ((Genode::uint64_t)_nic->mac_address().addr[3] & 0xff) << 16 |
					          ((Genode::uint64_t)_nic->mac_address().addr[4] & 0xff) << 8 |
					          ((Genode::uint64_t)_nic->mac_address().addr[5] & 0xff);

					/* start receiver thread for this MAC */
					Vancouver_network * netreceiver = new Vancouver_network(_motherboard, _nic);

					return true;
				}
			default:

				PWRN("HostOp %d not implemented", msg.type);
				return false;
			}
		}

		bool receive(MessageDisk &msg)
		{
			if (verbose_debug)
				PDBG("MessageDisk");
			return false;
		}

		bool receive(MessageTimer &msg)
		{
			switch (msg.type) {
			case MessageTimer::TIMER_NEW:

				if (verbose_debug)
					Logging::printf("TIMER_NEW\n");

				if (_alarm_thread == NULL) {
					Logging::printf("Creating alarm thread\n");
					_alarm_thread = new Alarm_thread(_motherboard, _timeouts);
				}

				msg.nr = _timeouts()->alloc();

				return true;

			case MessageTimer::TIMER_REQUEST_TIMEOUT:

				if (_timeouts()->request(msg.nr, msg.abstime) < 0)
					Logging::printf("Could not program timeout.\n");

				return true;

			default:
				return false;
			};
		}

		bool receive(MessageTime &msg)
		{
			Genode::Lock::Guard guard(*utcb_lock());
			utcb_backup = *Genode::Thread_base::myself()->utcb();

			if (!_rtc) {
				try {
					_rtc = new Rtc::Connection;
				} catch (...) {
					Logging::printf("No RTC present, returning dummy time.\n");
					msg.wallclocktime = msg.timestamp = 0;

					*Genode::Thread_base::myself()->utcb() = utcb_backup;

					return true;
				}
			}
			msg.wallclocktime = _rtc->get_current_time();
			Logging::printf("Got time %llx\n", msg.wallclocktime);
			msg.timestamp = _unsynchronized_motherboard.clock()->clock(1000000U);

			*Genode::Thread_base::myself()->utcb() = utcb_backup;

			return true;
		}

		bool receive(MessageNetwork &msg)
		{
			if (msg.type != MessageNetwork::PACKET) return false;

			Genode::Lock::Guard guard(*utcb_lock());
			utcb_backup = *Genode::Thread_base::myself()->utcb();

			if (msg.buffer == _forward_pkt) {
				/* don't end in an endless forwarding loop */
				return false;
			}

			/* allocate transmit packet */
			Packet_descriptor tx_packet;
			try {
				tx_packet = _nic->tx()->alloc_packet(msg.len);
			} catch (Nic::Session::Tx::Source::Packet_alloc_failed) {
				PERR("tx packet alloc failed");
				return false;
			}

			/* fill packet with content */
			char *tx_content = _nic->tx()->packet_content(tx_packet);
			_forward_pkt = tx_content;
			for (unsigned i = 0; i < msg.len; i++) {
				tx_content[i] = msg.buffer[i];
			}
			_nic->tx()->submit_packet(tx_packet);

			/* wait for acknowledgement */
			Packet_descriptor ack_tx_packet = _nic->tx()->get_acked_packet();

			if (ack_tx_packet.size()   != tx_packet.size()
			 || ack_tx_packet.offset() != tx_packet.offset()) {
				PERR("unexpected acked packet");
			}

			/* release sent packet to free the space in the tx communication buffer */
			_nic->tx()->release_packet(tx_packet);

			*Genode::Thread_base::myself()->utcb() = utcb_backup;

			return true;
		}

		bool receive(MessagePciConfig &msg)
		{
			if (verbose_debug)
				PDBG("MessagePciConfig");
			return false;
		}

		bool receive(MessageAcpi &msg)
		{
			if (verbose_debug)
				PDBG("MessageAcpi");
			return false;
		}

		bool receive(MessageLegacy &msg)
		{
			if (msg.type == MessageLegacy::RESET) {
				Logging::printf("MessageLegacy::RESET requested\n");
				return true;
			}
			return false;
		}

		/**
		 * Constructor
		 */
		Machine(Boot_module_provider &boot_modules, Guest_memory &guest_memory)
		:
			_hip_rom("hypervisor_info_page"),
			_hip(Genode::env()->rm_session()->attach(_hip_rom.dataspace())),
			_clock(_hip->tsc_freq*1000),
			_motherboard_lock(Genode::Lock::LOCKED),
			_unsynchronized_motherboard(&_clock, _hip),
			_motherboard(_motherboard_lock, &_unsynchronized_motherboard),
			_timeouts(_timeouts_lock, &_unsynchronized_timeouts),
			_guest_memory(guest_memory),
			_boot_modules(boot_modules)
		{
			_timeouts()->init();

			/* register host operations, called back by the VMM */
			_unsynchronized_motherboard.bus_hostop.add  (this, receive_static<MessageHostOp>);
			_unsynchronized_motherboard.bus_disk.add    (this, receive_static<MessageDisk>);
			_unsynchronized_motherboard.bus_timer.add   (this, receive_static<MessageTimer>);
			_unsynchronized_motherboard.bus_time.add    (this, receive_static<MessageTime>);
			_unsynchronized_motherboard.bus_network.add (this, receive_static<MessageNetwork>);
			_unsynchronized_motherboard.bus_hwpcicfg.add(this, receive_static<MessageHwPciConfig>);
			_unsynchronized_motherboard.bus_acpi.add    (this, receive_static<MessageAcpi>);
			_unsynchronized_motherboard.bus_legacy.add  (this, receive_static<MessageLegacy>);
		}


		/**
		 * Exception type thrown on configuration errors
		 */
		class Config_error { };


		/**
		 * Configure virtual machine according to the provided XML description
		 *
		 * \param machine_node  XML node containing device-model sub nodes
		 * \throw               Config_error
		 *
		 * Device models are instantiated in the order of appearance in the XML
		 * configuration.
		 */
		void setup_devices(Genode::Xml_node machine_node)
		{
			using namespace Genode;

			Xml_node node = machine_node.sub_node();
			for (;; node = node.next()) {

				enum { MODEL_NAME_MAX_LEN = 32 };
				char name[MODEL_NAME_MAX_LEN];
				node.type_name(name, sizeof(name));

				PINF("device: %s", name);
				Device_model_info *dmi = device_model_registry()->lookup(name);

				if (!dmi) {
					PERR("configuration error: device model '%s' does not exist", name);
					throw Config_error();
				}

				/*
				 * Read device-model arguments into 'argv' array
				 */
				enum { MAX_ARGS = 8 };
				unsigned long argv[MAX_ARGS];

				for (int i = 0; i < MAX_ARGS; i++)
					argv[i] = ~0UL;

				for (int i = 0; dmi->arg_names[i] && (i < MAX_ARGS); i++) {

					try {
						Xml_node::Attribute arg = node.attribute(dmi->arg_names[i]);
						arg.value(&argv[i]);

						PINF(" arg[%d]: 0x%x", i, (int)argv[i]);
					}
					catch (Xml_node::Nonexistent_attribute) { }
				}

				/*
				 * Initialize new instance of device model
				 *
				 * We never pass any argument string to a device model because
				 * it is not examined by the existing device models.
				 */
				dmi->create(_unsynchronized_motherboard, argv, "", 0);

				if (node.is_last())
					break;
			}
		}

		/**
		 * Reset the machine and unblock the VCPUs
		 */
		void boot()
		{
			/* init VCPUs */
			for (VCpu *vcpu = _unsynchronized_motherboard.last_vcpu; vcpu; vcpu = vcpu->get_last()) {

				/* init CPU strings */
				const char *short_name = "NOVA microHV";
				vcpu->set_cpuid(0, 1, reinterpret_cast<const unsigned *>(short_name)[0]);
				vcpu->set_cpuid(0, 3, reinterpret_cast<const unsigned *>(short_name)[1]);
				vcpu->set_cpuid(0, 2, reinterpret_cast<const unsigned *>(short_name)[2]);
				const char *long_name = "Vancouver VMM proudly presents this VirtualCPU. ";
				for (unsigned i=0; i<12; i++)
					vcpu->set_cpuid(0x80000002 + (i / 4), i % 4, reinterpret_cast<const unsigned *>(long_name)[i]);

				/* propagate feature flags from the host */
				unsigned ebx_1=0, ecx_1=0, edx_1=0;
				Cpu::cpuid(1, ebx_1, ecx_1, edx_1);

				/* clflush size */
				vcpu->set_cpuid(1, 1, ebx_1 & 0xff00, 0xff00ff00);

				/* +SSE3,+SSSE3 */
				vcpu->set_cpuid(1, 2, ecx_1, 0x00000201);

				/* -PAE,-PSE36, -MTRR,+MMX,+SSE,+SSE2,+CLFLUSH,+SEP */
				vcpu->set_cpuid(1, 3, edx_1, 0x0f88a9bf | (1 << 28));
			}

			Logging::printf("RESET device state\n");
			MessageLegacy msg2(MessageLegacy::RESET, 0);
			_unsynchronized_motherboard.bus_legacy.send_fifo(msg2);

			Logging::printf("INIT done\n");

			_motherboard_lock.unlock();
		}

		Synced_motherboard &motherboard() { return _motherboard; }

		Motherboard &unsynchronized_motherboard() { return _unsynchronized_motherboard; }

		Genode::Lock &motherboard_lock() { return _motherboard_lock; }

		~Machine()
		{
			Genode::env()->rm_session()->detach(_hip);
		}
};


extern unsigned long _prog_img_beg;  /* begin of program image (link address) */
extern unsigned long _prog_img_end;  /* end of program image */

namespace Genode {
	Rm_session *env_context_area_rm_session();

	void __attribute__((constructor)) init_context_area_vmm() {
		/**
		 * XXX Invoke env_context_rm_session to make sure the virtual region of
		 *     the context area is reserved at core. Typically this happens
		 *     when the first time a thread is allocated. Unfortunately,
		 *     beforehand the VMM may try to grab the same region for
		 *     large VM sizes.
		 */
		env_context_area_rm_session();
	}
}

int main(int argc, char **argv)
{
	/*
	 * Reserve complete lower address space so that nobody else can take it.
	 * When we found out how much memory we actually should use for the VM,
	 * the reservation is adjusted to the real size.
	 */
	Genode::Rm_connection pre_reservation(0, Genode::Native_config::context_area_virtual_base());
	Genode::env()->rm_session()->attach_at(pre_reservation.dataspace(),
	                                       PAGE_SIZE, 0, PAGE_SIZE);

	Genode::printf("--- Vancouver VMM starting ---\n");

	/* request max available memory */
	Genode::addr_t vm_size = Genode::env()->ram_session()->avail();
	/* reserve some memory for the VMM */
	vm_size -= 8 * 1024 * 1024;
	/* calculate max memory for the VM */
	vm_size = vm_size & ~((1UL << PAGE_SIZE_LOG2) - 1);

	/* Find out framebuffer size (default: 4 MiB) */
	Genode::addr_t fb_size = 4*1024*1024;
	try {
		Genode::Xml_node node = Genode::config()->xml_node().sub_node("vga");
		Genode::Xml_node::Attribute arg = node.attribute("fb_size");
		unsigned long val;
                arg.value(&val);
		fb_size = val*1024;
	} catch (...) { }

	static Guest_memory guest_memory(vm_size, fb_size);
	/* free up temporary rm_session */
	Genode::env()->parent()->close(pre_reservation.cap());

	/* diagnostic messages */
	Genode::printf("[0x%08lx, 0x%08lx) - %lu MiB - guest physical memory\n",
	               0, vm_size, vm_size / 1024 / 1024);

	if (guest_memory.backing_store_local_base())
		Genode::printf("[0x%08p, 0x%08lx) - VMM local base of guest-physical"
		               " memory\n", guest_memory.backing_store_local_base(),
		               (Genode::addr_t)guest_memory.backing_store_local_base() +
		               vm_size);

	Genode::printf("[0x%08lx, 0x%08lx) - Genode thread context area\n",
	                Genode::Native_config::context_area_virtual_base(),
	                Genode::Native_config::context_area_virtual_base() +
	                Genode::Native_config::context_area_virtual_size());

	Genode::printf("[0x%08lx, 0x%08lx) - VMM program image\n",
	               (Genode::addr_t)&_prog_img_beg,
	               (Genode::addr_t)&_prog_img_end);

	if (!guest_memory.backing_store_local_base()) {
		Genode::printf("Not enough space (0x%lx) left for VMM, VM image"
		               " to large\n", vm_size);
		return 1;
	}

	Genode::printf("\n--- Setup VM ---\n");

	static Boot_module_provider
		boot_modules(Genode::config()->xml_node().sub_node("multiboot"));

	static Machine machine(boot_modules, guest_memory);

	Genode::Lock fb_lock;

	/* create console thread */
	Vancouver_console vcon(machine.motherboard(),
	                       fb_lock,
	                       fb_size, guest_memory.fb_ds());

	vcon.register_host_operations(machine.unsynchronized_motherboard());

	/* create disk thread */
	Vancouver_disk vdisk(machine.motherboard(),
	                     guest_memory.backing_store_local_base(),
	                     guest_memory.backing_store_fb_local_base());

	vdisk.register_host_operations(machine.unsynchronized_motherboard());

	machine.setup_devices(Genode::config()->xml_node().sub_node("machine"));

	Genode::printf("\n--- Booting VM ---\n");

	machine.boot();

	Genode::sleep_forever();
	return 0;
}
