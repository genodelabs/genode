/*
 * \brief  Vancouver main program for Genode
 * \author Norman Feske
 * \date   2011-11-18
 *
 * This code is partially based on 'vancouver.cc' that comes with the NOVA
 * userland.
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
 * Copyright (C) 2011-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <util/touch.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <base/cap_sel_alloc.h>
#include <base/thread.h>
#include <rom_session/connection.h>
#include <rm_session/connection.h>
#include <os/config.h>

/* NOVA includes that come with Genode */
#include <nova/syscalls.h>

/* NOVA userland includes */
#include <nul/vcpu.h>
#include <nul/motherboard.h>
#include <sys/semaphore.h>
#include <sys/hip.h>

/* local includes */
#include <device_model_registry.h>
#include <boot_module_provider.h>


enum {
	PAGE_SIZE_LOG2 = 12UL,
	PAGE_SIZE      = 1UL << PAGE_SIZE_LOG2
};


enum { verbose_npt = false };
enum { verbose_io  = false };


/**
 * Semaphore used as global lock
 *
 * Used for startup synchronization and coarse-grained locking.
 */
Semaphore _lock;


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
		enum { RESERVATION_START = 0,
		       RESERVATION_SIZE  = 0x10000000 };

		Genode::Rm_connection _reservation;

		/*
		 * RAM used as backing store for guest-physical memory
		 */
		enum { DS_LOCAL_START = 0x20000000 };

		Genode::Ram_dataspace_capability _ds;
		char *_local_addr;

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
		Guest_memory(Genode::size_t backing_store_size)
		:
			_reservation(RESERVATION_START, RESERVATION_SIZE),
			_ds(Genode::env()->ram_session()->alloc(backing_store_size)),
			_local_addr(Genode::env()->rm_session()->attach_at(_ds, DS_LOCAL_START)),
			remaining_size(backing_store_size)
		{
			/*
			 * Attach reservation to the beginning of the local address space.
			 * We leave out the very first page because core denies the
			 * attachment of anything at the zero page.
			 */
			Genode::env()->rm_session()->attach_at(_reservation.dataspace(),
			                                       PAGE_SIZE, 0, PAGE_SIZE);

			Logging::printf("local base of guest-physical memory is 0x%p\n",
			                backing_store_local_base());
		}

		~Guest_memory()
		{
			/* detach reservation */
			Genode::env()->rm_session()->detach((void *)PAGE_SIZE);

			/* detach and free backing store */
			Genode::env()->rm_session()->detach((void *)DS_LOCAL_START);
			Genode::env()->ram_session()->free(_ds);
		}

		/**
		 * Return pointer to locally mapped backing store
		 */
		char *backing_store_local_base()
		{
			return _local_addr;
		}
};


class Vcpu_dispatcher : Genode::Thread_base,
                        public StaticReceiver<Vcpu_dispatcher>
{
	private:

		/**
		 * Stack size of vCPU event handler
		 */
		enum { STACK_SIZE = 4096 };

		/**
		 * Pointer to corresponding VCPU model
		 */
		VCpu * const _vcpu;

		/**
		 * Guest-physical memory
		 */
		Guest_memory &_guest_memory;

		/**
		 * Motherboard representing the inter-connections of all device models
		 */
		Motherboard &_motherboard;

		/**
		 * Base of portal window where the handlers for virtualization events
		 * will be registered. This window will be passed to the vCPU EC at
		 * creation time.
		 */
		Nova::mword_t const _ev_pt_sel_base;

		/**
		 * Size of portal window used for virtualization events, in log2
		 */
		enum { EV_PT_WINDOW_SIZE_LOG2 = 8 };

		/**
		 * Capability selector used for the VCPU's execution context
		 */
		Genode::addr_t const _sm_sel;
		Genode::addr_t const _ec_sel;

		/**
		 * Capability selector used for the VCPU's scheduling context
		 */
		Genode::addr_t const _sc_sel;


		/***************
		 ** Shortcuts **
		 ***************/

		static Nova::mword_t _alloc_sel(Genode::size_t num_caps_log2 = 0) {
			return Genode::cap_selector_allocator()->alloc(num_caps_log2); }

		static void _free_sel(Nova::mword_t sel, Genode::size_t num_caps_log2 = 0) {
			Genode::cap_selector_allocator()->free(sel, num_caps_log2); }

		static Nova::mword_t _pd_sel() {
			return Genode::cap_selector_allocator()->pd_sel(); }

		static ::Utcb *_utcb_of_myself() {
			return (::Utcb *)Thread_base::myself()->utcb(); }


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

			SemaphoreGuard guard(_lock);

			/**
			 * Send the message to the VCpu.
			 */
			if (!_vcpu->executor.send(msg, true))
				Logging::panic("nobody to execute %s at %x:%x\n",
				               __func__, msg.cpu->cs.sel, msg.cpu->eip);

			/**
			 * Check whether we should inject something...
			 */
			if (msg.mtr_in & MTD_INJ && msg.type != CpuMessage::TYPE_CHECK_IRQ) {
				msg.type = CpuMessage::TYPE_CHECK_IRQ;
				if (!_vcpu->executor.send(msg, true))
					Logging::panic("nobody to execute %s at %x:%x\n",
					               __func__, msg.cpu->cs.sel, msg.cpu->eip);
			}

			/**
			 * If the IRQ injection is performed, recalc the IRQ window.
			 */
			if (msg.mtr_out & MTD_INJ) {
				msg.type = CpuMessage::TYPE_CALC_IRQWINDOW;
				if (!_vcpu->executor.send(msg, true))
					Logging::panic("nobody to execute %s at %x:%x\n",
					               __func__, msg.cpu->cs.sel, msg.cpu->eip);
			}

			msg.cpu->mtd = msg.mtr_out;
		}

		bool _handle_map_memory(bool need_unmap)
		{
			Utcb *utcb = _utcb_of_myself();
			Genode::addr_t const fault_addr = utcb->qual[1];

			MessageMemRegion mem_region(fault_addr >> 12);

			if (verbose_npt)
				Logging::printf("--> request mapping at 0x%lx\n", fault_addr);

			if (!_motherboard.bus_memregion.send(mem_region, true) || !mem_region.ptr)
				return false;

			if (verbose_npt)
				Logging::printf("MemRegion: page=%lx, start_page=%lx, count=%lx, ptr=%p\n",
				                mem_region.page, mem_region.start_page, mem_region.count, mem_region.ptr);

			Genode::addr_t src = (Genode::addr_t)_guest_memory.backing_store_local_base() +
			                    (mem_region.page << PAGE_SIZE_LOG2);
			Genode::touch_read((unsigned char volatile *)src);

			Nova::Mem_crd crd(src >> PAGE_SIZE_LOG2, 32 - PAGE_SIZE_LOG2,
			                  Nova::Rights(true, true, true));

			Nova::uint8_t ret = Nova::lookup(crd);

			if (verbose_npt)
				Logging::printf("looked up crd (base=0x%lx, order=%ld)\n",
				                crd.base(), crd.order());

			if (need_unmap)
				Logging::panic("_handle_map_memory: need_unmap not handled, yet\n");

			Nova::mword_t hotspot = mem_region.page << 12;

			if (verbose_npt)
				Logging::printf("NPT mapping (base=0x%lx, order=%d hotspot=0x%lx)\n",
				                crd.base(), crd.order(), hotspot);

			Nova::Utcb * u = (Nova::Utcb *)utcb;
			u->set_msg_word(0);
			u->append_item(crd, hotspot, false, true);

			/* EPT violation during IDT vectoring? */
			if (utcb->inj_info & 0x80000000)
				Logging::panic("EPT violation during IDT vectoring - not handled\n");

			return true;
		}

		void _handle_io(bool is_in, unsigned io_order, unsigned port)
		{
			if (verbose_io)
				Logging::printf("--> I/O is_in=%d, io_order=%d, port=%x\n",
				                is_in, io_order, port);

			Utcb *utcb = _utcb_of_myself();
			CpuMessage msg(is_in, static_cast<CpuState *>(utcb), io_order, port, &utcb->eax, utcb->mtd);
			_skip_instruction(msg);
			{
				SemaphoreGuard l(_lock);
				if (!_vcpu->executor.send(msg, true))
					Logging::panic("nobody to execute %s at %x:%x\n", __func__, msg.cpu->cs.sel, msg.cpu->eip);
			}
		}

		void _svm_startup()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_CHECK_IRQ);
		}

		void _svm_npt()
		{
			Utcb *utcb = _utcb_of_myself();
			MessageMemRegion msg(utcb->qual[1] >> 12);
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

			if (utcb->qual[0] & 0x4)
				Logging::panic("force_invalid_gueststate_amd(utcb) - not implemented\n");

			else {
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

		void _svm_msr()
		{
			_svm_invalid();
		}

		void _recall()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_CHECK_IRQ);
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
			Thread_base *myself = Thread_base::myself();
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
		void _register_handler(Nova::Mtd mtd)
		{
			/* let the compiler generate an instance of a portal entry */
			void (*portal_entry)() = &_portal_entry<EV, FUNC>;

			Nova::create_pt(_ev_pt_sel_base + EV, _pd_sel(),
			                _tid.ec_sel, mtd, (Nova::mword_t)portal_entry);
		}

	public:

		Vcpu_dispatcher(VCpu         *vcpu,
		                Guest_memory &guest_memory,
		                Motherboard  &motherboard,
		                bool          has_svm,
		                unsigned      cpu_number)
		:
			Genode::Thread_base("vcpu_handler", STACK_SIZE),
			_vcpu(vcpu),
			_guest_memory(guest_memory),
			_motherboard(motherboard),
			_ev_pt_sel_base(_alloc_sel(EV_PT_WINDOW_SIZE_LOG2)),
			_sm_sel(_alloc_sel(1)),
			_ec_sel(_sm_sel + 1),
			_sc_sel(_alloc_sel())
		{
			using namespace Nova;

			/*
			 * Stack and UTCB of local EC used as VCPU event handler
			 */
			mword_t *const sp   = (mword_t * const)_context->stack;
			mword_t  const utcb = (mword_t) &_context->utcb;

			/*
			 * XXX: Propagate the specified CPU number. For now, we ignore
			 *      the 'cpu_number' argument.
			 */

			/*
			 * Create local EC used handling virtualization events. This EC has
			 * is executed with the CPU time donated by the guest OS.
			 */
			enum { CPU_NUMBER = 0, GLOBAL_NO = false };
			if (create_ec(_tid.ec_sel, _pd_sel(),
			              CPU_NUMBER, utcb, (mword_t)sp,
			              _tid.exc_pt_sel, GLOBAL_NO))
				PERR("create_ec failed while creating the handler EC");


			/* shortcuts for common message-transfer descriptors */
			Mtd const mtd_all(Mtd::ALL);
			Mtd const mtd_cpuid(Mtd::EIP | Mtd::ACDB | Mtd::IRQ);
			Mtd const mtd_irq(Mtd::IRQ);
			/*
			 * Register VCPU SVM event handlers
			 */
			typedef Vcpu_dispatcher This;
			if (has_svm) {

				_register_handler<0x72, &This::_svm_cpuid>   (mtd_cpuid);
				_register_handler<0x7b, &This::_svm_ioio>    (mtd_all);
				_register_handler<0x7c, &This::_svm_msr>     (mtd_all);
				_register_handler<0xfc, &This::_svm_npt>     (mtd_all);
				_register_handler<0xfd, &This::_svm_invalid> (mtd_all);
				_register_handler<0xfe, &This::_svm_startup> (mtd_all);
				_register_handler<0xff, &This::_recall>      (mtd_irq);

			} else {

				/*
				 * XXX: to be done, for now, we only support SVM
				 */
				Logging::panic("no SVM available, sorry");
			}

			/*
			 * Create semaphore used as mechanism for blocking the VCPU
			 */
			if (create_sm(_sm_sel, _pd_sel(), 0))
				PERR("create_rm returned failed while creating VCPU");

			/*
			 * Create EC for virtual CPU. The virtual CPU is a global EC
			 * because it executes on an own budget of CPU time.
			 */
			enum { GLOBAL_YES = true, UTCB_VCPU = 0 };
			if (create_ec(_ec_sel, _pd_sel(), CPU_NUMBER, UTCB_VCPU,
			              0, _ev_pt_sel_base, GLOBAL_YES))
				PERR("create_ec failed while creating VCPU");

			/*
			 * Fuel the VCPU with CPU time by attaching a scheduling context to
			 * it.
			 */
			if (create_sc(_sc_sel, _pd_sel(), _ec_sel, Nova::Qpd()))
				PERR("create_sc failed while creating VCPU");

			/* handle cpuid overrides */
			vcpu->executor.add(this, receive_static<CpuMessage>);
		}

		/**
		 * Destructor
		 */
		~Vcpu_dispatcher()
		{
			_free_sel(_ev_pt_sel_base, EV_PT_WINDOW_SIZE_LOG2);
			_free_sel(_ec_sel, 0);
			_free_sel(_sc_sel, 0);
		}

		/**
		 * Unused member of the 'Thread_base' interface
		 *
		 * Similarly to how 'Rpc_entrypoints' are handled, a 'Vcpu_dispatcher'
		 * comes with a custom initialization procedure, which does not call
		 * the thread's normal entry function. Instread, the thread's EC gets
		 * associated with several portals, each for handling a specific
		 * virtualization event.
		 */
		void entry() { }

		/**
		 * Return capability selector of the VCPU's SM and EC
		 *
		 * The returned number corresponds to the VCPU's semaphore selector.
		 * The consecutive number corresponds to the EC. The number returned by
		 * this function is used by the VMM code as a unique identifyer of the
		 * VCPU. I.e., it gets passed as arguments for 'MessageHostOp'
		 * operations.
		 *
		 * XXX: Couldn't we just use the 'Vcpu_dispatcher' as unique
		 *      identifier instead?
		 */
		Nova::mword_t sm_ec_sel_base() const { return _sm_sel; }


		/***********************************
		 ** Handlers for 'StaticReceiver' **
		 ***********************************/

		bool receive(CpuMessage &msg)
		{
			if (msg.type != CpuMessage::TYPE_CPUID)
				return false;

			PDBG("CpuMessage::TYPE_CPUID - not implemented");
			for (;;);
			return false;
		}
};


class Machine : public StaticReceiver<Machine>
{
	private:

		Genode::Rom_connection _hip_rom;
		Hip * const            _hip;
		Clock                  _clock;
		Motherboard            _motherboard;
		TimeoutList<32, void>  _timeouts;
		Guest_memory           _guest_memory;
		Boot_module_provider  &_boot_modules;

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

				Logging::printf("OP_GUEST_MEM value=0x%lx\n", msg.value);

				if (msg.value >= _guest_memory.remaining_size) {
					msg.value = 0;
				} else {
					msg.len = _guest_memory.remaining_size - msg.value;
					msg.ptr = _guest_memory.backing_store_local_base() + msg.value;
				}
				Logging::printf(" -> len=0x%lx, ptr=0x%p\n",
				                msg.len, msg.ptr);
				return true;

			/**
			 * Cut off upper range of guest memory by specified amount
			 */
			case MessageHostOp::OP_ALLOC_FROM_GUEST:

				Logging::printf("OP_ALLOC_FROM_GUEST\n");

				if (msg.value > _guest_memory.remaining_size)
					return false;

				_guest_memory.remaining_size -= msg.value;

				msg.phys = _guest_memory.remaining_size;

				Logging::printf("-> allocated from guest %08lx+%lx\n",
				                _guest_memory.remaining_size, msg.value);
				return true;

			case MessageHostOp::OP_VCPU_CREATE_BACKEND:
				{
					Logging::printf("OP_VCPU_CREATE_BACKEND\n");
					/*
					 * XXX determine real CPU number, hardcoded CPU 0 for now
					 */
					enum { CPU_NUMBER = 0 };
					Vcpu_dispatcher *vcpu_dispatcher =
						new Vcpu_dispatcher(msg.vcpu, _guest_memory, _motherboard,
						                    _hip->has_svm(), CPU_NUMBER);

					msg.value = vcpu_dispatcher->sm_ec_sel_base();
					return true;
				}

			case MessageHostOp::OP_VCPU_RELEASE:

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
					Logging::printf("OP_VCPU_BLOCK\n");

					_lock.up();
					Logging::printf("going to block\n");
					bool res = (Nova::sm_ctrl(msg.value, Nova::SEMAPHORE_DOWN) == 0);
					Logging::printf("woke up from vcpu sem, block on _lock\n");
					_lock.down();
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
						Logging::printf("could not load module, destination buffer too small\n");
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

			default:
				
				PWRN("HostOp %d not implemented", msg.type);
				return false;
			}
		}

		bool receive(MessageConsole &msg)
		{
			Logging::printf("MessageConsole\n");
			return true;
		}

		bool receive(MessageDisk &msg)
		{
			PDBG("MessageDisk");
			return false;
		}

		bool receive(MessageTimer &msg)
		{
			switch (msg.type) {
			case MessageTimer::TIMER_NEW:

				Logging::printf("TIMER_NEW\n");
				msg.nr = _timeouts.alloc();
				return true;

			case MessageTimer::TIMER_REQUEST_TIMEOUT:

				Logging::printf("TIMER_REQUEST_TIMEOUT\n");
				_timeouts.request(msg.nr, msg.abstime);
				return true;

			default:
				return false;
			};
		}

		bool receive(MessageTime &msg)
		{
			Logging::printf("MessageTime - not implemented\n");
			return false;
		}

		bool receive(MessageNetwork &msg)
		{
			PDBG("MessageNetwork");
			return false;
		}

		bool receive(MessageVirtualNet &msg)
		{
			PDBG("MessageVirtualNet");
			return false;
		}

		bool receive(MessagePciConfig &msg)
		{
			PDBG("MessagePciConfig");
			return false;
		}

		bool receive(MessageAcpi &msg)
		{
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
		Machine(Boot_module_provider &boot_modules)
		:
			_hip_rom("hypervisor_info_page"),
			_hip(Genode::env()->rm_session()->attach(_hip_rom.dataspace())),
			_clock(_hip->freq_tsc*1000),
			_motherboard(&_clock, _hip),
			_guest_memory(32*1024*1024 /* XXX hard-wired for now */),
			_boot_modules(boot_modules)
		{
			_timeouts.init();

			/* register host operations, called back by the VMM */
			_motherboard.bus_hostop.add  (this, receive_static<MessageHostOp>);
			_motherboard.bus_console.add (this, receive_static<MessageConsole>);
			_motherboard.bus_disk.add    (this, receive_static<MessageDisk>);
			_motherboard.bus_timer.add   (this, receive_static<MessageTimer>);
			_motherboard.bus_time.add    (this, receive_static<MessageTime>);
			_motherboard.bus_network.add (this, receive_static<MessageNetwork>);
			_motherboard.bus_vnet.add    (this, receive_static<MessageVirtualNet>);
			_motherboard.bus_hwpcicfg.add(this, receive_static<MessagePciConfig>);
			_motherboard.bus_acpi.add    (this, receive_static<MessageAcpi>);
			_motherboard.bus_legacy.add  (this, receive_static<MessageLegacy>);
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
				dmi->create(_motherboard, argv, "", 0);

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
			for (VCpu *vcpu = _motherboard.last_vcpu; vcpu; vcpu = vcpu->get_last()) {

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
			_motherboard.bus_legacy.send_fifo(msg2);

			_lock.up();
			Logging::printf("INIT done\n");
		}

		~Machine()
		{
			Genode::env()->rm_session()->detach(_hip);
		}
};


int main(int argc, char **argv)
{
	Genode::printf("--- Vancouver VMM started ---\n");

	_lock = Semaphore(Genode::cap_selector_allocator()->alloc());
	if (Nova::create_sm(_lock.sm(), Genode::cap_selector_allocator()->pd_sel(), 0))
		PWRN("_lock creation failed");

	static Boot_module_provider
		boot_modules(Genode::config()->xml_node().sub_node("multiboot"));

	static Machine machine(boot_modules);

	machine.setup_devices(Genode::config()->xml_node().sub_node("machine"));

	machine.boot();

	Genode::sleep_forever();
	return 0;
}
