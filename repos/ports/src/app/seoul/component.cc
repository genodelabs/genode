/*
 * \brief  Seoul component for Genode
 * \author Alexander Boettcher
 * \author Norman Feske
 * \author Markus Partheymueller
 * \date   2011-11-18
 */

/*
 * Copyright (C) 2011-2019 Genode Labs GmbH
 * Copyright (C) 2012 Intel Corporation
 *
 * This file is distributed under the terms of the GNU General Public License
 * version 2.
 *
 * The code is partially based on the Seoul/Vancouver VMM, which is distributed
 * under the terms of the GNU General Public License version 2.
 *
 * Modifications by Intel Corporation are contributed under the terms and
 * conditions of the GNU General Public License version 2.
 */

/* base includes */
#include <base/allocator_avl.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/rpc_server.h>
#include <base/synced_interface.h>
#include <rm_session/connection.h>
#include <rom_session/connection.h>
#include <util/touch.h>
#include <util/misc_math.h>

#include <vm_session/connection.h>
#include <cpu/vm_state.h>

/* os includes */
#include <nic_session/connection.h>
#include <nic/packet_allocator.h>
#include <rtc_session/connection.h>
#include <timer_session/connection.h>

#include <vmm/types.h>

/* Seoul includes as used by NOVA userland (NUL) */
#include <nul/vcpu.h>
#include <nul/motherboard.h>

/* utilities includes */
#include <service/time.h>

/* local includes */
#include "synced_motherboard.h"
#include "device_model_registry.h"
#include "boot_module_provider.h"
#include "console.h"
#include "network.h"
#include "disk.h"
#include "state.h"
#include "guest_memory.h"
#include "timeout_late.h"


enum { verbose_debug = false };
enum { verbose_npt   = false };
enum { verbose_io    = false };


using Genode::Attached_rom_dataspace;

typedef Genode::Synced_interface<TimeoutList<32, void> > Synced_timeout_list;

class Timeouts
{
	private:

		Timer::Connection                 _timer;
		Synced_motherboard               &_motherboard;
		Synced_timeout_list              &_timeouts;
		Genode::Signal_handler<Timeouts>  _timeout_sigh;
		Late_timeout                      _late { };

		Genode::uint64_t _check_and_wakeup()
		{
			Late_timeout::Remote const timeout_remote = _late.reset();

			timevalue const now = _motherboard()->clock()->time();

			unsigned timer_nr;
			unsigned timeout_count = 0;

			while ((timer_nr = _timeouts()->trigger(now))) {

				if (timeout_count == 0 && _late.apply(timeout_remote,
				                                      timer_nr, now))
				{
					return _motherboard()->clock()->abstime(1, 1000);
				}

				MessageTimeout msg(timer_nr, _timeouts()->timeout());

				if (_timeouts()->cancel(timer_nr) < 0)
					Logging::printf("Timeout not cancelled.\n");

				_motherboard()->bus_timeout.send(msg);

				timeout_count++;
			}

			return _timeouts()->timeout();
		}

		void check_timeouts()
		{
			Genode::uint64_t const next = _check_and_wakeup();

			if (next == ~0ULL)
				return;

			timevalue rel_timeout_us = _motherboard()->clock()->delta(next, 1000 * 1000);
			if (rel_timeout_us == 0)
				rel_timeout_us = 1;

			_timer.trigger_once(rel_timeout_us);
		}

	public:

		void reprogram(Clock &clock, MessageTimer const &msg)
		{
			_late.timeout(clock, msg);
			Genode::Signal_transmitter(_timeout_sigh).submit();
		}

		/**
		 * Constructor
		 */
		Timeouts(Genode::Env &env, Synced_motherboard &mb,
		             Synced_timeout_list &timeouts)
		:
		  _timer(env),
		  _motherboard(mb),
		  _timeouts(timeouts),
		  _timeout_sigh(env.ep(), *this, &Timeouts::check_timeouts)
		{
			_timer.sigh(_timeout_sigh);
		}

};


class Vcpu : public StaticReceiver<Vcpu>
{

	private:

		Genode::Vm_connection              &_vm_con;
		Genode::Vm_handler<Vcpu>            _handler;
		bool const                          _vmx;
		bool const                          _svm;
		bool const                          _map_small;
		bool const                          _rdtsc_exit;
		Genode::Vm_session_client::Vcpu_id  _id;
		Genode::Attached_dataspace          _state_ds;
		Genode::Vm_state                   &_state;

		Seoul::Guest_memory                &_guest_memory;
		Synced_motherboard                 &_motherboard;
		Genode::Synced_interface<VCpu>      _vcpu;

		CpuState                            _seoul_state { };

		Genode::Semaphore                   _block { 0 };

	public:

		Vcpu(Genode::Entrypoint &ep,
		     Genode::Vm_connection &vm_con,
		     Genode::Allocator &alloc, Genode::Env &env,
		     Genode::Mutex &vcpu_mutex, VCpu *unsynchronized_vcpu,
		     Seoul::Guest_memory &guest_memory, Synced_motherboard &motherboard,
		     bool vmx, bool svm, bool map_small, bool rdtsc)
		:
			_vm_con(vm_con),
			_handler(ep, *this, &Vcpu::_handle_vm_exception,
			         vmx ? &Vcpu::exit_config_intel :
			         svm ? &Vcpu::exit_config_amd : nullptr),
			_vmx(vmx), _svm(svm), _map_small(map_small), _rdtsc_exit(rdtsc),
			/* construct vcpu */
			_id(_vm_con.with_upgrade([&]() {
				return _vm_con.create_vcpu(alloc, env, _handler);
			})),
			/* get state of vcpu */
			_state_ds(env.rm(), _vm_con.cpu_state(_id)),
			_state(*_state_ds.local_addr<Genode::Vm_state>()),

			_guest_memory(guest_memory),
			_motherboard(motherboard),
			_vcpu(vcpu_mutex, unsynchronized_vcpu)
		{
			if (!_svm && !_vmx)
				Logging::panic("no SVM/VMX available, sorry");

			_seoul_state.clear();

			/* handle cpuid overrides */
			unsynchronized_vcpu->executor.add(this, receive_static<CpuMessage>);

			/* let vCPU run */
			_vm_con.run(id());
		}

		Genode::Vm_session_client::Vcpu_id id() const { return _id; }

		void block() { _block.down(); }
		void unblock() { _block.up(); }

		void recall() { _vm_con.pause(id()); }

		void _handle_vm_exception()
		{
			unsigned const exit = _state.exit_reason;

			if (_svm) {
				switch (exit) {
				case 0x00 ... 0x1f: _svm_cr(); break;
				case 0x62: _irqwin(); break;
				case 0x64: _irqwin(); break;
				case 0x6e: _svm_rdtsc(); break;
				case 0x72: _svm_cpuid(); break;
				case 0x78: _svm_hlt(); break;
				case 0x7b: _svm_ioio(); break;
				case 0x7c: _svm_msr(); break;
				case 0x7f: _triple(); break;
				case 0xfd: _svm_invalid(); break;
				case 0xfc: _svm_npt(); break;
				case 0xfe: _svm_startup(); break;
				case 0xff: _recall(); break;
				default:
					Genode::error(__func__, " exit=", Genode::Hex(exit));
					/* no resume */
					return;
				}
			}
			if (_vmx) {
				switch (exit) {
				case 0x02: _triple(); break;
				case 0x03: _vmx_init(); break;
				case 0x07: _irqwin(); break;
				case 0x0a: _vmx_cpuid(); break;
				case 0x0c: _vmx_hlt(); break;
				case 0x10: _vmx_rdtsc(); break;
				case 0x12: _vmx_vmcall(); break;
				case 0x1c: _vmx_mov_crx(); break;
				case 0x1e: _vmx_ioio(); break;
				case 0x1f: _vmx_msr_read(); break;
				case 0x20: _vmx_msr_write(); break;
				case 0x21: _vmx_invalid(); break;
				case 0x28: _vmx_pause(); break;
				case 0x30: _vmx_ept(); break;
				case 0xfe: _vmx_startup(); break;
				case 0xff: _recall(); break;
				default:
					Genode::error(__func__, " exit=", Genode::Hex(exit));
					/* no resume */
					return;
				}
			}

			/* resume */
			_vm_con.run(id());

		}

		void exit_config_intel(Genode::Vm_state &state, unsigned exit)
		{
			CpuState dummy_state;
			unsigned mtd = 0;

			/* touch the register state required for the specific vm exit */

			switch (exit) {
			case 0x02: /* _triple */
				mtd = MTD_ALL;
				break;
			case 0x03: /* _vmx_init */
				mtd = MTD_ALL;
				break;
			case 0x07: /* _vmx_irqwin */
				mtd = MTD_IRQ;
				break;
			case 0x0a: /* _vmx_cpuid */
				mtd = MTD_RIP_LEN | MTD_GPR_ACDB | MTD_STATE;
				break;
			case 0x0c: /* _vmx_hlt */
				mtd = MTD_RIP_LEN | MTD_IRQ;
				break;
			case 0x10: /* _vmx_rdtsc */
				mtd = MTD_RIP_LEN | MTD_GPR_ACDB | MTD_TSC | MTD_STATE;
				break;
			case 0x12: /* _vmx_vmcall */
				mtd = MTD_RIP_LEN | MTD_GPR_ACDB;
				break;
			case 0x1c: /* _vmx_mov_crx */
				mtd = MTD_ALL;
				break;
			case 0x1e: /* _vmx_ioio */
				mtd = MTD_RIP_LEN | MTD_QUAL | MTD_GPR_ACDB | MTD_STATE | MTD_RFLAGS;
				break;
			case 0x28: /* _vmx_pause */
				mtd = MTD_RIP_LEN | MTD_STATE;
				break;
			case 0x1f: /* _vmx_msr_read */
			case 0x20: /* _vmx_msr_write */
				mtd = MTD_RIP_LEN | MTD_GPR_ACDB | MTD_TSC | MTD_SYSENTER | MTD_STATE;
				break;
			case 0x21: /* _vmx_invalid */
			case 0x30: /* _vmx_ept */
			case 0xfe: /* _vmx_startup */
				mtd = MTD_ALL;
				break;
			case 0xff: /* _recall */
				mtd = MTD_IRQ | MTD_RIP_LEN | MTD_GPR_ACDB | MTD_GPR_BSD;
				break;
			default:
//				mtd = MTD_RIP_LEN;
				break;
			}

			Seoul::write_vm_state(dummy_state, mtd, state);
		}

		void exit_config_amd(Genode::Vm_state &state, unsigned exit)
		{
			CpuState dummy_state;
			unsigned mtd = 0;

			/* touch the register state required for the specific vm exit */

			switch (exit) {
			case 0x00 ... 0x1f: /* _svm_cr */
				mtd = MTD_RIP_LEN | MTD_CS_SS | MTD_GPR_ACDB | MTD_GPR_BSD |
				      MTD_CR | MTD_IRQ;
				break;
			case 0x72: /* _svm_cpuid */
				mtd = MTD_RIP_LEN | MTD_GPR_ACDB | MTD_IRQ;
				break;
			case 0x78: /* _svm_hlt */
				mtd = MTD_RIP_LEN | MTD_IRQ;
				break;
			case 0xff: /*_recall */
			case 0x62: /* _irqwin - SMI */
			case 0x64: /* _irqwin */
				mtd = MTD_IRQ;
				break;
			case 0x6e: /* _svm_rdtsc */
				mtd = MTD_RIP_LEN | MTD_GPR_ACDB | MTD_TSC | MTD_STATE;
				break;
			case 0x7b: /* _svm_ioio */
				mtd = MTD_RIP_LEN | MTD_QUAL | MTD_GPR_ACDB | MTD_STATE;
				break;
			case 0x7c: /* _svm_msr, MTD_ALL */
			case 0x7f: /* _triple, MTD_ALL */
			case 0xfd: /* _svm_invalid, MTD_ALL */
			case 0xfc: /*_svm_npt, MTD_ALL */
			case 0xfe: /*_svm_startup, MTD_ALL */
				mtd = MTD_ALL;
				break;
			default:
//				mtd = MTD_RIP_LEN;
				break;
			}

			Seoul::write_vm_state(dummy_state, mtd, state);
		}

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
			/* convert Genode VM state to Seoul state */
			unsigned mtd = Seoul::read_vm_state(_state, _seoul_state);

			CpuMessage msg(type, &_seoul_state, mtd);

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

			if (~mtd & msg.mtr_out)
				Genode::error("mtd issue !? exit=", Genode::Hex(_state.exit_reason),
				              " ", Genode::Hex(mtd), "->", Genode::Hex(msg.mtr_out),
				              " ", Genode::Hex(~mtd & msg.mtr_out));

			/* convert Seoul state to Genode VM state */
			Seoul::write_vm_state(_seoul_state, msg.mtr_out, _state);
		}

		bool _handle_map_memory(bool need_unmap)
		{
			Genode::addr_t const vm_fault_addr = _state.qual_secondary.value();

			if (verbose_npt)
				Logging::printf("--> request mapping at 0x%lx\n", vm_fault_addr);

			MessageMemRegion mem_region(vm_fault_addr >> Vmm::PAGE_SIZE_LOG2);

			if (!_motherboard()->bus_memregion.send(mem_region, false) ||
			    !mem_region.ptr)
				return false;

			if (verbose_npt)
				Logging::printf("VM page 0x%lx in [0x%lx:0x%lx),"
				                " VMM area: [0x%lx:0x%lx)\n",
				                mem_region.page, mem_region.start_page,
				                mem_region.start_page + mem_region.count,
				                (Genode::addr_t)mem_region.ptr >> Vmm::PAGE_SIZE_LOG2,
				                ((Genode::addr_t)mem_region.ptr >> Vmm::PAGE_SIZE_LOG2)
				                + mem_region.count);

			Genode::addr_t vmm_memory_base =
			        reinterpret_cast<Genode::addr_t>(mem_region.ptr);
			Genode::addr_t vmm_memory_fault = vmm_memory_base +
			        (vm_fault_addr - (mem_region.start_page << Vmm::PAGE_SIZE_LOG2));

			/* XXX: Not yet supported by Seoul/Vancouver.

			bool read=true, write=true, execute=true;
			if (mem_region.attr == (DESC_TYPE_MEM | DESC_RIGHT_R)) {
				if (verbose_npt)
					Logging::printf("Mapping readonly to %p (err:%x, attr:%x)\n",
						vm_fault_addr, utcb->qual[0], mem_region.attr);
				write = execute = false;
			}*/

			if (need_unmap)
				Logging::panic("_handle_map_memory: need_unmap not handled, yet\n");

			assert(_state.inj_info.valid());

			/* EPT violation during IDT vectoring? */
			if (_state.inj_info.value() & 0x80000000U) {
				/* convert Genode VM state to Seoul state */
				unsigned mtd = Seoul::read_vm_state(_state, _seoul_state);
				assert(mtd & MTD_INJ);

				Logging::printf("EPT violation during IDT vectoring.\n");

				CpuMessage _win(CpuMessage::TYPE_CALC_IRQWINDOW, &_seoul_state, mtd);
				_win.mtr_out = MTD_INJ;
				if (!_vcpu()->executor.send(_win, true))
					Logging::panic("nobody to execute %s at %x:%x\n",
					               __func__, _seoul_state.cs.sel, _seoul_state.eip);

				/* convert Seoul state to Genode VM state */
				Seoul::write_vm_state(_seoul_state, _win.mtr_out, _state);
//				_state.inj_info.value(_state.inj_info.value() & ~0x80000000U);
			} else
				_state = Genode::Vm_state {}; /* reset */

			_vm_con.with_upgrade([&]() {
				if (_map_small)
					_guest_memory.attach_to_vm(_vm_con,
					                           mem_region.page << Vmm::PAGE_SIZE_LOG2,
					                           1 << Vmm::PAGE_SIZE_LOG2);
				else
					_guest_memory.attach_to_vm(_vm_con,
					                           mem_region.start_page << Vmm::PAGE_SIZE_LOG2,
					                           mem_region.count << Vmm::PAGE_SIZE_LOG2);
			});

			return true;
		}

		void _handle_io(bool is_in, unsigned io_order, unsigned port)
		{
			if (verbose_io)
				Logging::printf("--> I/O is_in=%d, io_order=%d, port=%x\n",
				                is_in, io_order, port);

			/* convert Genode VM state to Seoul state */
			unsigned mtd = Seoul::read_vm_state(_state, _seoul_state);

			Genode::addr_t ax = _state.ax.value();

			CpuMessage msg(is_in, &_seoul_state, io_order,
			               port, &ax, mtd);

			_skip_instruction(msg);

			if (!_vcpu()->executor.send(msg, true))
				Logging::panic("nobody to execute %s at %x:%x\n",
				               __func__, msg.cpu->cs.sel, msg.cpu->eip);

			if (ax != _seoul_state.rax)
				_seoul_state.rax = ax;

			/* convert Seoul state to Genode VM state */
			Seoul::write_vm_state(_seoul_state, msg.mtr_out, _state);
		}

		/* SVM portal functions */
		void _svm_startup()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_CHECK_IRQ);
			_state.ctrl_primary.value(_rdtsc_exit ? (1U << 14) : 0);
		}

		void _svm_npt()
		{
			if (!_handle_map_memory(_state.qual_primary.value() & 1))
				_svm_invalid();
		}

		void _svm_cr()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_SINGLE_STEP);
		}

		void _svm_invalid()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_SINGLE_STEP);
			_state.ctrl_primary.value(1 << 18 /* cpuid */ | (_rdtsc_exit ? (1U << 14) : 0));
			_state.ctrl_secondary.value(1 << 0  /* vmrun */);
		}

		void _svm_ioio()
		{
			if (_state.qual_primary.value() & 0x4) {
				Genode::log("invalid gueststate");
				_state = Genode::Vm_state {}; /* reset */
				_state.ctrl_secondary.value(0);
			} else {
				unsigned order = ((_state.qual_primary.value() >> 4) & 7) - 1;

				if (order > 2)
					order = 2;

				_state.ip_len.value(_state.qual_secondary.value() - _state.ip.value());

				_handle_io(_state.qual_primary.value() & 1, order,
				           _state.qual_primary.value() >> 16);
			}
		}

		void _svm_cpuid()
		{
			_state.ip_len.value(2);
			_handle_vcpu(SKIP, CpuMessage::TYPE_CPUID);
		}

		void _svm_hlt()
		{
			_state.ip_len.value(1);
			_vmx_hlt();
		}

		void _svm_rdtsc()
		{
			_state.ip_len.value(2);
			_handle_vcpu(SKIP, CpuMessage::TYPE_RDTSC);
		}

		void _svm_msr()
		{
			_svm_invalid();
		}

		void _recall()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_CHECK_IRQ);
		}

		void _irqwin()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_CHECK_IRQ);
		}

		void _triple()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_TRIPLE);
		}

		void _vmx_init()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_INIT);
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
			_state = Genode::Vm_state {}; /* reset */
			_state.ip.value(_state.ip.value() + _state.ip_len.value());
		}

		void _vmx_pause()
		{
			/* convert Genode VM state to Seoul state */
			unsigned mtd = Seoul::read_vm_state(_state, _seoul_state);

			CpuMessage msg(CpuMessage::TYPE_SINGLE_STEP, &_seoul_state, mtd);

			_skip_instruction(msg);

			/* convert Seoul state to Genode VM state */
			Seoul::write_vm_state(_seoul_state, msg.mtr_out, _state);
		}

		void _vmx_invalid()
		{
			_state.flags.value(_state.flags.value() | 2);
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_SINGLE_STEP);
		}

		void _vmx_startup()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_HLT);
			_state.ctrl_primary.value(_rdtsc_exit ? (1U << 12) : 0);
			_state.ctrl_secondary.value(0);
		}

		void _vmx_ioio()
		{
			unsigned order = 0U;
			if (_state.qual_primary.value() & 0x10) {
				Logging::printf("invalid gueststate\n");
				assert(_state.flags.valid());
				_state = Genode::Vm_state {}; /* reset */
				_state.flags.value(_state.flags.value() & ~2U);
			} else {
				order = _state.qual_primary.value() & 7;
				if (order > 2) order = 2;

				_handle_io(_state.qual_primary.value() & 8, order,
				           _state.qual_primary.value() >> 16);
			}
		}

		void _vmx_ept()
		{
			if (!_handle_map_memory(_state.qual_primary.value() & 0x38))
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

		void _vmx_mov_crx()
		{
			_handle_vcpu(NO_SKIP, CpuMessage::TYPE_SINGLE_STEP);
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
				return true;
			case 0x80000007U:
				/* Bit 8 of edx indicates whether invariant TSC is supported */
				msg.cpu->eax = msg.cpu->ebx = msg.cpu->ecx = msg.cpu->edx = 0;
				return true;
			default:
				Logging::printf("CpuMessage::TYPE_CPUID index %x ignored\n",
				                msg.cpuid_index);
			}
			return true;
		}
};



class Machine : public StaticReceiver<Machine>
{
	private:

		Genode::Env           &_env;
		Genode::Heap          &_heap;
		Genode::Vm_connection &_vm_con;
		Clock                  _clock;
		Genode::Mutex          _motherboard_mutex { };
		Motherboard            _unsynchronized_motherboard;
		Synced_motherboard     _motherboard;
		Genode::Mutex          _timeouts_mutex { };
		TimeoutList<32, void>  _unsynchronized_timeouts { };
		Synced_timeout_list    _timeouts;
		Seoul::Guest_memory   &_guest_memory;
		Boot_module_provider  &_boot_modules;
		Timeouts               _alarm_thread = { _env, _motherboard, _timeouts };
		unsigned short         _vcpus_up = 0;

		Genode::addr_t         _alloc_fb_size { 0 };
		Genode::addr_t         _alloc_fb_mem  { 0 };
		Genode::addr_t         _vm_phys_fb    { 0 };

		bool                   _map_small    { false   };
		bool                   _rdtsc_exit   { false   };
		bool                   _same_cpu     { false   };
		Seoul::Network        *_nic          { nullptr };
		Rtc::Session          *_rtc          { nullptr };

		enum { MAX_CPUS = 8 };
		Vcpu *                 _vcpus[MAX_CPUS] { nullptr };
		Genode::Bit_array<64>  _vcpus_active { };

		/*
		 * Noncopyable
		 */
		Machine(Machine const &);
		Machine &operator = (Machine const &);

	public:

		/*********************************************
		 ** Callbacks registered at the motherboard **
		 *********************************************/

		bool receive(MessageHostOp &msg)
		{
			switch (msg.type) {

			case MessageHostOp::OP_ALLOC_IOMEM:
			{
				if (msg.len & 0xfff)
					return false;

				Genode::addr_t const guest_addr = msg.value;
				try {
					Genode::Dataspace_capability ds = _env.ram().alloc(msg.len);
					Genode::addr_t local_addr = _env.rm().attach(ds);
					_guest_memory.add_region(_heap, guest_addr, local_addr, ds, msg.len);
					msg.ptr = reinterpret_cast<char *>(local_addr);
					return true;
				} catch (...) {
					return false;
				}
			}
			/**
			 * Request available guest memory starting at specified address
			 */
			case MessageHostOp::OP_GUEST_MEM:

				if (verbose_debug)
					Logging::printf("OP_GUEST_MEM value=0x%lx\n", msg.value);

				if (_alloc_fb_mem) {
					msg.len = _alloc_fb_size;
					msg.ptr = (char *)_alloc_fb_mem - _vm_phys_fb;
					_alloc_fb_mem = _alloc_fb_size = _vm_phys_fb = 0;
					return true;
				}

				if (msg.value >= _guest_memory.remaining_size) {
					msg.value = 0;
				} else {
					msg.len = _guest_memory.remaining_size - msg.value;
					msg.ptr = _guest_memory.backing_store_local_base() + msg.value;
				}

				if (verbose_debug)
					Logging::printf(" -> len=0x%zx, ptr=0x%p\n",
					                msg.len, msg.ptr);
				return true;

			/**
			 * Cut off upper range of guest memory by specified amount
			 */
			case MessageHostOp::OP_ALLOC_FROM_GUEST:

				if (verbose_debug)
					Logging::printf("OP_ALLOC_FROM_GUEST\n");

				if (_alloc_fb_mem) {
					msg.phys = _vm_phys_fb;
					return true;
				}

				if (msg.value > _guest_memory.remaining_size)
					return false;

				_guest_memory.remaining_size -= msg.value;

				msg.phys = _guest_memory.remaining_size;

				if (verbose_debug)
					Logging::printf("-> allocated from guest %08lx+%lx\n",
					                msg.phys, msg.value);
				return true;

			case MessageHostOp::OP_VCPU_CREATE_BACKEND:
				{
					enum { STACK_SIZE = 2*1024*sizeof(Genode::addr_t) };

					if (verbose_debug)
						Logging::printf("OP_VCPU_CREATE_BACKEND\n");

					if (_vcpus_up >= sizeof(_vcpus)/sizeof(_vcpus[0])) {
						Logging::panic("too many vCPUs");
						return false;
					}

					/* detect virtualization extension */
					Attached_rom_dataspace const info(_env, "platform_info");
					Genode::Xml_node const features = info.xml().sub_node("hardware").sub_node("features");

					bool const has_svm = features.attribute_value("svm", false);
					bool const has_vmx = features.attribute_value("vmx", false);

					if (!has_svm && !has_vmx) {
						Logging::panic("no VMX nor SVM virtualization support found");
						return false;
					}

					using Genode::Entrypoint;
					using Genode::String;
					using Genode::Affinity;

					Affinity::Space space = _env.cpu().affinity_space();
					Affinity::Location location(space.location_of_index(_vcpus_up + (_same_cpu ? 0 : 1)));

					String<16> * ep_name = new String<16>("vCPU EP ", _vcpus_up);
					Entrypoint * ep = new Entrypoint(_env, STACK_SIZE,
					                                 ep_name->string(),
					                                 location);

					_vcpus_active.set(_vcpus_up, 1);

					Vcpu * vcpu = new Vcpu(*ep, _vm_con, _heap, _env,
					                       _motherboard_mutex, msg.vcpu,
					                       _guest_memory, _motherboard,
					                       has_vmx, has_svm, _map_small,
					                       _rdtsc_exit);

					_vcpus[_vcpus_up] = vcpu;
					msg.value = _vcpus_up;

					Logging::printf("create vcpu %u affinity %u:%u\n",
					                _vcpus_up, location.xpos(), location.ypos());

					_vcpus_up ++;

					return true;
				}

			case MessageHostOp::OP_VCPU_RELEASE:
			{
				if (verbose_debug)
					Genode::log("- OP_VCPU_RELEASE ", Genode::Thread::myself()->name());

				unsigned vcpu_id = msg.value;
				if ((_vcpus_up >= sizeof(_vcpus)/sizeof(_vcpus[0])))
					return false;

				if (!_vcpus[vcpu_id])
					return false;

				if (msg.len) {
					_vcpus[vcpu_id]->unblock();
					return true;
				}

				_vcpus[vcpu_id]->recall();
				return true;
			}
			case MessageHostOp::OP_VCPU_BLOCK:
				{
					if (verbose_debug)
						Genode::log("- OP_VCPU_BLOCK ", Genode::Thread::myself()->name());

					unsigned vcpu_id = msg.value;
					if ((_vcpus_up >= sizeof(_vcpus)/sizeof(_vcpus[0])))
						return false;

					if (!_vcpus[vcpu_id])
						return false;

					_vcpus_active.clear(vcpu_id, 1);

					if (!_vcpus_active.get(0, 64)) {
						MessageConsole msgcon(MessageConsole::Type::TYPE_KILL);
						_unsynchronized_motherboard.bus_console.send(msgcon);
					}

					_motherboard_mutex.release();

					_vcpus[vcpu_id]->block();

					_motherboard_mutex.acquire();

					if (!_vcpus_active.get(0, 64)) {
						MessageConsole msgcon(MessageConsole::Type::TYPE_RESET);
						_unsynchronized_motherboard.bus_console.send(msgcon);
					}

					_vcpus_active.set(vcpu_id, 1);

					return true;
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
						data_len = _boot_modules.data(_env, index,
						                              data_dst, dst_len);
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
						Genode::align_addr(data_len, Vmm::PAGE_SIZE_LOG2);

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
					if (_nic) {
						Logging::printf("Solely one network connection supported\n");
						return false;
					}

					try {
						_nic = new (_heap) Seoul::Network(_env, _heap,
						                                  _motherboard);
					} catch (...) {
						Logging::printf("Creating network connection failed\n");
						return false;
					}

					Nic::Mac_address mac = _nic->mac_address();

					Logging::printf("Mac address: %2x:%2x:%2x:%2x:%2x:%2x\n",
					                mac.addr[0], mac.addr[1], mac.addr[2],
					                mac.addr[3], mac.addr[4], mac.addr[5]);

					msg.mac = ((Genode::uint64_t)mac.addr[0] & 0xff) << 40 |
					          ((Genode::uint64_t)mac.addr[1] & 0xff) << 32 |
					          ((Genode::uint64_t)mac.addr[2] & 0xff) << 24 |
					          ((Genode::uint64_t)mac.addr[3] & 0xff) << 16 |
					          ((Genode::uint64_t)mac.addr[4] & 0xff) <<  8 |
					          ((Genode::uint64_t)mac.addr[5] & 0xff);

					return true;
				}
			default:

				Logging::printf("HostOp %d not implemented\n", msg.type);
				return false;
			}
		}

		bool receive(MessageTimer &msg)
		{
			switch (msg.type) {
			case MessageTimer::TIMER_NEW:

				if (verbose_debug)
					Logging::printf("TIMER_NEW\n");

				msg.nr = _timeouts()->alloc();

				return true;

			case MessageTimer::TIMER_REQUEST_TIMEOUT:
			{
				int res = _timeouts()->request(msg.nr, msg.abstime);

				if (res == 0)
					_alarm_thread.reprogram(_clock, msg);
				else
				if (res < 0)
					Logging::printf("Could not program timeout.\n");

				return true;
			}
			default:
				return false;
			};
		}

		bool receive(MessageTime &msg)
		{
			if (!_rtc) {
				try {
					_rtc = new Rtc::Connection(_env);
				} catch (...) {
					Logging::printf("No RTC present, returning dummy time.\n");
					msg.wallclocktime = msg.timestamp = 0;
					return true;
				}
			}

			Rtc::Timestamp rtc_ts = _rtc->current_time();
			tm_simple tms(rtc_ts.year, rtc_ts.month, rtc_ts.day, rtc_ts.hour,
			              rtc_ts.minute, rtc_ts.second);

			msg.wallclocktime = mktime(&tms) * MessageTime::FREQUENCY;
			Logging::printf("Got time %llx\n", msg.wallclocktime);
			msg.timestamp = _unsynchronized_motherboard.clock()->clock(MessageTime::FREQUENCY);

			return true;
		}

		bool receive(MessageNetwork &msg)
		{
			if (msg.type != MessageNetwork::PACKET) return false;

			if (!_nic)
				return false;

			/* XXX parallel invocations supported ? */
			return _nic->transmit(msg.buffer, msg.len);
		}

		bool receive(MessagePciConfig &msg)
		{
			if (verbose_debug)
				Logging::printf("MessagePciConfig\n");
			return false;
		}

		bool receive(MessageAcpi &msg)
		{
			if (verbose_debug)
				Logging::printf("MessageAcpi\n");
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
		Machine(Genode::Env &env, Genode::Heap &heap,
		        Genode::Vm_connection &vm_con,
		        Boot_module_provider &boot_modules,
		        Seoul::Guest_memory &guest_memory,
		        size_t const fb_size,
		        bool map_small, bool rdtsc_exit, bool vmm_vcpu_same_cpu)
		:
			_env(env), _heap(heap), _vm_con(vm_con),
			_clock(Attached_rom_dataspace(env, "platform_info").xml().sub_node("hardware").sub_node("tsc").attribute_value("freq_khz", 0ULL) * 1000ULL),
			_unsynchronized_motherboard(&_clock, nullptr),
			_motherboard(_motherboard_mutex, &_unsynchronized_motherboard),
			_timeouts(_timeouts_mutex, &_unsynchronized_timeouts),
			_guest_memory(guest_memory),
			_boot_modules(boot_modules),
			_map_small(map_small),
			_rdtsc_exit(rdtsc_exit),
			_same_cpu(vmm_vcpu_same_cpu)
		{
			_motherboard_mutex.acquire();

			_timeouts()->init();

			/* register host operations, called back by the VMM */
			_unsynchronized_motherboard.bus_hostop.add  (this, receive_static<MessageHostOp>);
			_unsynchronized_motherboard.bus_timer.add   (this, receive_static<MessageTimer>);
			_unsynchronized_motherboard.bus_time.add    (this, receive_static<MessageTime>);
			_unsynchronized_motherboard.bus_network.add (this, receive_static<MessageNetwork>);
			_unsynchronized_motherboard.bus_hwpcicfg.add(this, receive_static<MessageHwPciConfig>);
			_unsynchronized_motherboard.bus_acpi.add    (this, receive_static<MessageAcpi>);
			_unsynchronized_motherboard.bus_legacy.add  (this, receive_static<MessageLegacy>);

			/* tell vga model about available framebuffer memory */
			Device_model_info *dmi = device_model_registry()->lookup("vga_fbsize");
			if (dmi) {
				unsigned long argv[2] = { fb_size >> 10, ~0UL };
				dmi->create(_unsynchronized_motherboard, argv, "", 0);
			}
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
		void setup_devices(Genode::Xml_node machine_node,
		                   Seoul::Console &console)
		{
			using namespace Genode;

			bool const verbose = machine_node.attribute_value("verbose", false);

			Xml_node node = machine_node.sub_node();
			for (;; node = node.next()) {

				typedef String<32> Model_name;

				Model_name const name = node.type();

				if (verbose)
					Genode::log("device: ", name);

				Device_model_info *dmi = device_model_registry()->lookup(name.string());

				if (!dmi) {
					Genode::error("configuration error: device model '",
					              name, "' does not exist");
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
					if (node.has_attribute(dmi->arg_names[i])) {
						argv[i] = node.attribute_value(dmi->arg_names[i], ~0UL);
						if (verbose)
							Genode::log(" arg[", i, "]: ", Genode::Hex(argv[i]));
					}
				}

				/*
				 * Initialize new instance of device model
				 *
				 * We never pass any argument string to a device model because
				 * it is not examined by the existing device models.
				 */
				if (!strcmp(dmi->name, "vga", 4)) {
					_alloc_fb_mem  = console.attached_framebuffer();
					_alloc_fb_size = console.framebuffer_size();
					_vm_phys_fb    = console.vm_phys_framebuffer();
				}

				dmi->create(_unsynchronized_motherboard, argv, "", 0);

				if (_alloc_fb_mem) _alloc_fb_mem = _alloc_fb_size = 0;

				if (node.last())
					break;
			}
		}

		/**
		 * Reset the machine and unblock the VCPUs
		 */
		void boot()
		{
			Genode::log("VM is starting with ", _vcpus_up, " vCPU",
			            _vcpus_up > 1 ? "s" : "");

			/* init VCPUs */
			for (VCpu *vcpu = _unsynchronized_motherboard.last_vcpu; vcpu; vcpu = vcpu->get_last()) {

				/* init CPU strings */
				const char *short_name = "NOVA microHV";
				vcpu->set_cpuid(0, 1, reinterpret_cast<const unsigned *>(short_name)[0]);
				vcpu->set_cpuid(0, 3, reinterpret_cast<const unsigned *>(short_name)[1]);
				vcpu->set_cpuid(0, 2, reinterpret_cast<const unsigned *>(short_name)[2]);
				const char *long_name = "Seoul VMM proudly presents this VirtualCPU. ";
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

			_motherboard_mutex.release();
		}

		Synced_motherboard &motherboard() { return _motherboard; }

		Motherboard &unsynchronized_motherboard() { return _unsynchronized_motherboard; }
};


extern unsigned long _prog_img_beg;  /* begin of program image (link address) */
extern unsigned long _prog_img_end;  /* end of program image */

extern void heap_init_env(Genode::Heap *);

void Component::construct(Genode::Env &env)
{
	static Genode::Heap          heap(env.ram(), env.rm());
	static Genode::Vm_connection vm_con(env, "Seoul vCPUs",
	                                    Genode::Cpu_session::PRIORITY_LIMIT / 16);

	static Attached_rom_dataspace config(env, "config");

	Genode::log("--- Seoul VMM starting ---");

	Genode::Xml_node const node     = config.xml();
	Genode::uint64_t const vmm_size = node.attribute_value("vmm_memory",
	                                                       Genode::Number_of_bytes(12 * 1024 * 1024));

	bool const map_small         = node.attribute_value("map_small", false);
	bool const rdtsc_exit        = node.attribute_value("exit_on_rdtsc", false);
	bool const vmm_vcpu_same_cpu = node.attribute_value("vmm_vcpu_same_cpu",
	                                                    false);

	/* request max available memory */
	Genode::uint64_t vm_size = env.pd().avail_ram().value;
	/* reserve some memory for the VMM */
	vm_size -= vmm_size;
	/* calculate max memory for the VM */
	vm_size = vm_size & ~((1ULL << Vmm::PAGE_SIZE_LOG2) - 1);

	Genode::log(" VMM memory ", Genode::Number_of_bytes(vmm_size));
	Genode::log(" using ", map_small ? "small": "large",
	            " memory attachments for guest VM.");
	if (rdtsc_exit)
		Genode::log(" enabling VM exit on RDTSC.");

	unsigned const width  = node.attribute_value("width", 1024U);
	unsigned const height = node.attribute_value("height", 768U);

	Genode::log(" framebuffer ", width, "x", height);

	/* setup framebuffer memory for guest */
	static Nitpicker::Connection nitpicker { env };
	nitpicker.buffer(Framebuffer::Mode(width, height,
	                                   Framebuffer::Mode::RGB565), false);

	static Framebuffer::Session  &framebuffer { *nitpicker.framebuffer() };
	Framebuffer::Mode           const fb_mode { framebuffer.mode() };

	size_t const fb_size = Genode::align_addr(fb_mode.width() *
	                                          fb_mode.height() *
	                                          fb_mode.bytes_per_pixel(), 12);

	typedef Nitpicker::Session::View_handle View_handle;
	typedef Nitpicker::Session::Command Command;

	View_handle view = nitpicker.create_view();
	Nitpicker::Rect rect(Nitpicker::Point(0, 0),
	                     Nitpicker::Area(fb_mode.width(), fb_mode.height()));

	nitpicker.enqueue<Command::Geometry>(view, rect);
	nitpicker.enqueue<Command::To_front>(view, View_handle());
	nitpicker.execute();

	/* setup guest memory */
	static Seoul::Guest_memory guest_memory(env, heap, vm_con, vm_size);

	typedef Genode::Hex_range<unsigned long> Hex_range;

	/* diagnostic messages */
	if (guest_memory.backing_store_local_base())
		Genode::log(Hex_range((unsigned long)guest_memory.backing_store_local_base(),
		                      guest_memory.remaining_size),
		            " - ", vm_size / 1024 / 1024, " MiB",
		            " - VMM accessible shadow mapping of VM memory"); 

	Genode::log(Hex_range((Genode::addr_t)&_prog_img_beg,
	                      (Genode::addr_t)&_prog_img_end -
	                      (Genode::addr_t)&_prog_img_beg),
	            " - VMM program image");

	if (!guest_memory.backing_store_local_base()) {
		Genode::error("Not enough space left for ",
		              guest_memory.backing_store_local_base() ? "framebuffer"
		                                                      : "VMM");
		env.parent().exit(-1);
		return;
	}

	/* register device models of Seoul, see device_model_registry.cc */
	env.exec_static_constructors();

	Genode::log("\n--- Setup VM ---");

	heap_init_env(&heap);

	static Boot_module_provider boot_modules(node.sub_node("multiboot"));

	/* create the PC machine based on the configuration given */
	static Machine machine(env, heap, vm_con, boot_modules, guest_memory,
	                       fb_size, map_small, rdtsc_exit, vmm_vcpu_same_cpu);

	/* create console thread */
	static Seoul::Console vcon(env, heap, machine.motherboard(),
	                           machine.unsynchronized_motherboard(),
	                           nitpicker, guest_memory);

	vcon.register_host_operations(machine.unsynchronized_motherboard());

	/* create disk thread */
	static Seoul::Disk vdisk(env, machine.motherboard(),
	                         guest_memory.backing_store_local_base(),
	                         guest_memory.backing_store_size());

	vdisk.register_host_operations(machine.unsynchronized_motherboard());

	machine.setup_devices(node.sub_node("machine"), vcon);

	Genode::log("\n--- Booting VM ---");

	machine.boot();
}
