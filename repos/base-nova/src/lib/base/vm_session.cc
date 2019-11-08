/*
 * \brief  Client-side VM session interface
 * \author Alexander Boettcher
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <vm_session/client.h>
#include <base/env.h>
#include <base/allocator.h>
#include <base/registry.h>
#include <base/rpc_server.h>

#include <cpu/vm_state.h>

#include <nova/capability_space.h>
#include <nova/native_thread.h>
#include <nova/syscalls.h>
#include <nova_native_pd/client.h>

using namespace Genode;

struct Vcpu;

static Genode::Registry<Genode::Registered<Vcpu> > vcpus;

struct Vcpu {

	private:

		Signal_dispatcher_base      &_obj;
		Allocator                   &_alloc;
		Vm_session_client::Vcpu_id   _id;
		addr_t                       _state { 0 };
		void                        *_ep_handler { nullptr };
		void                        *_dispatching { nullptr };
		bool                         _block { true };

		enum Remote_state_requested {
			NONE = 0,
			PAUSE = 1,
			RUN = 2
		} _remote { NONE };

		static void _read_nova_state(Nova::Utcb &utcb, Vm_state &state,
		                             unsigned exit_reason)
		{
			typedef Genode::Vm_state::Segment Segment;
			typedef Genode::Vm_state::Range Range;

			state = Vm_state {};
			state.exit_reason = exit_reason;

			if (utcb.mtd & Nova::Mtd::FPU) {
				state.fpu.value([&] (uint8_t *fpu, size_t const) {
					asm volatile ("fxsave %0" : "=m" (*fpu));
				});
			}

			if (utcb.mtd & Nova::Mtd::ACDB) {
				state.ax.value(utcb.ax);
				state.cx.value(utcb.cx);
				state.dx.value(utcb.dx);
				state.bx.value(utcb.bx);
			}

			if (utcb.mtd & Nova::Mtd::EBSD) {
				state.di.value(utcb.di);
				state.si.value(utcb.si);
				state.bp.value(utcb.bp);
			}

			if (utcb.mtd & Nova::Mtd::EFL) state.flags.value(utcb.flags);
			if (utcb.mtd & Nova::Mtd::ESP) state.sp.value(utcb.sp);
			if (utcb.mtd & Nova::Mtd::DR)  state.dr7.value(utcb.dr7);

			if (utcb.mtd & Nova::Mtd::EIP) {
				state.ip.value(utcb.ip);
				state.ip_len.value(utcb.instr_len);
			}

			if (utcb.mtd & Nova::Mtd::R8_R15) {
				state. r8.value(utcb.read_r8());
				state. r9.value(utcb.read_r9());
				state.r10.value(utcb.read_r10());
				state.r11.value(utcb.read_r11());
				state.r12.value(utcb.read_r12());
				state.r13.value(utcb.read_r13());
				state.r14.value(utcb.read_r14());
				state.r15.value(utcb.read_r15());
			}

			if (utcb.mtd & Nova::Mtd::CR) {
				state.cr0.value(utcb.cr0);
				state.cr2.value(utcb.cr2);
				state.cr3.value(utcb.cr3);
				state.cr4.value(utcb.cr4);
			}
			if (utcb.mtd & Nova::Mtd::CSSS) {
				state.cs.value(Segment{utcb.cs.sel, utcb.cs.ar, utcb.cs.limit,
				                       utcb.cs.base});
				state.ss.value(Segment{utcb.ss.sel, utcb.ss.ar, utcb.ss.limit,
				                       utcb.ss.base});
			}

			if (utcb.mtd & Nova::Mtd::ESDS) {
				state.es.value(Segment{utcb.es.sel, utcb.es.ar, utcb.es.limit,
				                       utcb.es.base});
				state.ds.value(Segment{utcb.ds.sel, utcb.ds.ar, utcb.ds.limit,
				                       utcb.ds.base});
			}

			if (utcb.mtd & Nova::Mtd::FSGS) {
				state.fs.value(Segment{utcb.fs.sel, utcb.fs.ar, utcb.fs.limit,
				                       utcb.fs.base});
				state.gs.value(Segment{utcb.gs.sel, utcb.gs.ar, utcb.gs.limit,
				                       utcb.gs.base});
			}

			if (utcb.mtd & Nova::Mtd::TR) {
				state.tr.value(Segment{utcb.tr.sel, utcb.tr.ar, utcb.tr.limit,
				                       utcb.tr.base});
			}

			if (utcb.mtd & Nova::Mtd::LDTR) {
				state.ldtr.value(Segment{utcb.ldtr.sel, utcb.ldtr.ar,
				                         utcb.ldtr.limit, utcb.ldtr.base});
			}

			if (utcb.mtd & Nova::Mtd::GDTR) {
				state.gdtr.value(Range{utcb.gdtr.base, utcb.gdtr.limit});
			}

			if (utcb.mtd & Nova::Mtd::IDTR) {
				state.idtr.value(Range{utcb.idtr.base, utcb.idtr.limit});
			}

			if (utcb.mtd & Nova::Mtd::SYS) {
				state.sysenter_cs.value(utcb.sysenter_cs);
				state.sysenter_sp.value(utcb.sysenter_sp);
				state.sysenter_ip.value(utcb.sysenter_ip);
			}

			if (utcb.mtd & Nova::Mtd::QUAL) {
				state.qual_primary.value(utcb.qual[0]);
				state.qual_secondary.value(utcb.qual[1]);
			}

			if (utcb.mtd & Nova::Mtd::CTRL) {
				state.ctrl_primary.value(utcb.ctrl[0]);
				state.ctrl_secondary.value(utcb.ctrl[1]);
			}

			if (utcb.mtd & Nova::Mtd::INJ) {
				state.inj_info.value(utcb.inj_info);
				state.inj_error.value(utcb.inj_error);
			}

			if (utcb.mtd & Nova::Mtd::STA) {
				state.intr_state.value(utcb.intr_state);
				state.actv_state.value(utcb.actv_state);
			}

			if (utcb.mtd & Nova::Mtd::TSC) {
				state.tsc.value(utcb.tsc_val);
				state.tsc_offset.value(utcb.tsc_off);
			}

			if (utcb.mtd & Nova::Mtd::EFER) {
				state.efer.value(utcb.read_efer());
			}

			if (utcb.mtd & Nova::Mtd::PDPTE) {
				state.pdpte_0.value(utcb.pdpte[0]);
				state.pdpte_1.value(utcb.pdpte[1]);
				state.pdpte_2.value(utcb.pdpte[2]);
				state.pdpte_3.value(utcb.pdpte[3]);
			}

			if (utcb.mtd & Nova::Mtd::SYSCALL_SWAPGS) {
				state.star.value(utcb.read_star());
				state.lstar.value(utcb.read_lstar());
				state.fmask.value(utcb.read_fmask());
				state.kernel_gs_base.value(utcb.read_kernel_gs_base());
			}

			if (utcb.mtd & Nova::Mtd::TPR) {
				state.tpr.value(utcb.read_tpr());
				state.tpr_threshold.value(utcb.read_tpr_threshold());
			}

		}

		static void _write_nova_state(Nova::Utcb &utcb, Vm_state &state)
		{
			utcb.items = 0;
			utcb.mtd = 0;

			if (state.ax.valid() || state.cx.valid() ||
			    state.dx.valid() || state.bx.valid()) {
				utcb.mtd |= Nova::Mtd::ACDB;
				utcb.ax   = state.ax.value();
				utcb.cx   = state.cx.value();
				utcb.dx   = state.dx.value();
				utcb.bx   = state.bx.value();
			}

			if (state.bp.valid() || state.di.valid() || state.si.valid()) {
				utcb.mtd |= Nova::Mtd::EBSD;
				utcb.di   = state.di.value();
				utcb.si   = state.si.value();
				utcb.bp   = state.bp.value();
			}

			if (state.flags.valid()) {
				utcb.mtd |= Nova::Mtd::EFL;
				utcb.flags = state.flags.value();
			}

			if (state.sp.valid()) {
				utcb.mtd |= Nova::Mtd::ESP;
				utcb.sp = state.sp.value();
			}

			if (state.ip.valid()) {
				utcb.mtd |= Nova::Mtd::EIP;
				utcb.ip = state.ip.value();
				utcb.instr_len = state.ip_len.value();
			}

			if (state.dr7.valid()) {
				utcb.mtd |= Nova::Mtd::DR;
				utcb.dr7 = state.dr7.value();
			}

			if (state.r8.valid()  || state.r9.valid() ||
			    state.r10.valid() || state.r11.valid() ||
			    state.r12.valid() || state.r13.valid() ||
			    state.r14.valid() || state.r15.valid()) {

				utcb.mtd |= Nova::Mtd::R8_R15;
				utcb.write_r8 (state.r8.value());
				utcb.write_r9 (state.r9.value());
				utcb.write_r10(state.r10.value());
				utcb.write_r11(state.r11.value());
				utcb.write_r12(state.r12.value());
				utcb.write_r13(state.r13.value());
				utcb.write_r14(state.r14.value());
				utcb.write_r15(state.r15.value());
			}

			if (state.cr0.valid() || state.cr2.valid() || state.cr3.valid() ||
			    state.cr4.valid()) {
				utcb.mtd |= Nova::Mtd::CR;
				utcb.cr0 = state.cr0.value();
				utcb.cr2 = state.cr2.value();
				utcb.cr3 = state.cr3.value();
				utcb.cr4 = state.cr4.value();
			}

			if (state.cs.valid() || state.ss.valid()) {
				utcb.mtd |= Nova::Mtd::CSSS;
				utcb.cs.sel   = state.cs.value().sel;
				utcb.cs.ar    = state.cs.value().ar;
				utcb.cs.limit = state.cs.value().limit;
				utcb.cs.base  = state.cs.value().base;

				utcb.ss.sel   = state.ss.value().sel;
				utcb.ss.ar    = state.ss.value().ar;
				utcb.ss.limit = state.ss.value().limit;
				utcb.ss.base  = state.ss.value().base;
			}

			if (state.es.valid() || state.ds.valid()) {
				utcb.mtd     |= Nova::Mtd::ESDS;
				utcb.es.sel   = state.es.value().sel;
				utcb.es.ar    = state.es.value().ar;
				utcb.es.limit = state.es.value().limit;
				utcb.es.base  = state.es.value().base;

				utcb.ds.sel   = state.ds.value().sel;
				utcb.ds.ar    = state.ds.value().ar;
				utcb.ds.limit = state.ds.value().limit;
				utcb.ds.base  = state.ds.value().base;
			}

			if (state.fs.valid() || state.gs.valid()) {
				utcb.mtd     |= Nova::Mtd::FSGS;
				utcb.fs.sel   = state.fs.value().sel;
				utcb.fs.ar    = state.fs.value().ar;
				utcb.fs.limit = state.fs.value().limit;
				utcb.fs.base  = state.fs.value().base;

				utcb.gs.sel   = state.gs.value().sel;
				utcb.gs.ar    = state.gs.value().ar;
				utcb.gs.limit = state.gs.value().limit;
				utcb.gs.base  = state.gs.value().base;
			}

			if (state.tr.valid()) {
				utcb.mtd     |= Nova::Mtd::TR;
				utcb.tr.sel   = state.tr.value().sel;
				utcb.tr.ar    = state.tr.value().ar;
				utcb.tr.limit = state.tr.value().limit;
				utcb.tr.base  = state.tr.value().base;
			}

			if (state.ldtr.valid()) {
				utcb.mtd       |= Nova::Mtd::LDTR;
				utcb.ldtr.sel   = state.ldtr.value().sel;
				utcb.ldtr.ar    = state.ldtr.value().ar;
				utcb.ldtr.limit = state.ldtr.value().limit;
				utcb.ldtr.base  = state.ldtr.value().base;
			}

			if (state.gdtr.valid()) {
				utcb.mtd       |= Nova::Mtd::GDTR;
				utcb.gdtr.limit = state.gdtr.value().limit;
				utcb.gdtr.base  = state.gdtr.value().base;
			}

			if (state.idtr.valid()) {
				utcb.mtd       |= Nova::Mtd::IDTR;
				utcb.idtr.limit = state.idtr.value().limit;
				utcb.idtr.base  = state.idtr.value().base;
			}

			if (state.sysenter_cs.valid() || state.sysenter_sp.valid() ||
			    state.sysenter_ip.valid()) {
				utcb.mtd        |= Nova::Mtd::SYS;
				utcb.sysenter_cs = state.sysenter_cs.value();
				utcb.sysenter_sp = state.sysenter_sp.value();
				utcb.sysenter_ip = state.sysenter_ip.value();
			}

			if (state.ctrl_primary.valid() || state.ctrl_secondary.valid()) {
				utcb.mtd     |= Nova::Mtd::CTRL;
				utcb.ctrl[0]  = state.ctrl_primary.value();
				utcb.ctrl[1]  = state.ctrl_secondary.value();
			}

			if (state.inj_info.valid() || state.inj_error.valid()) {
				utcb.mtd      |= Nova::Mtd::INJ;
				utcb.inj_info  = state.inj_info.value();
				utcb.inj_error = state.inj_error.value();
			}

			if (state.intr_state.valid() || state.actv_state.valid()) {
				utcb.mtd        |= Nova::Mtd::STA;
				utcb.intr_state  = state.intr_state.value();
				utcb.actv_state  = state.actv_state.value();
			}

			if (state.tsc.valid() || state.tsc_offset.valid()) {
				utcb.mtd     |= Nova::Mtd::TSC;
				utcb.tsc_val  = state.tsc.value();
				utcb.tsc_off  = state.tsc_offset.value();
			}

			if (state.efer.valid()) {
				utcb.mtd |= Nova::Mtd::EFER;
				utcb.write_efer(state.efer.value());
			}

			if (state.pdpte_0.valid() || state.pdpte_1.valid() ||
			    state.pdpte_2.valid() || state.pdpte_3.valid()) {

				utcb.mtd |= Nova::Mtd::PDPTE;
				utcb.pdpte[0] = state.pdpte_0.value();
				utcb.pdpte[1] = state.pdpte_1.value();
				utcb.pdpte[2] = state.pdpte_2.value();
				utcb.pdpte[3] = state.pdpte_3.value();
			}

			if (state.star.valid() || state.lstar.valid() ||
			    state.fmask.valid() || state.kernel_gs_base.valid()) {

				utcb.mtd  |= Nova::Mtd::SYSCALL_SWAPGS;
				utcb.write_star(state.star.value());
				utcb.write_lstar(state.lstar.value());
				utcb.write_fmask(state.fmask.value());
				utcb.write_kernel_gs_base(state.kernel_gs_base.value());
			}

			if (state.tpr.valid() || state.tpr_threshold.valid()) {
				utcb.mtd |= Nova::Mtd::TPR;
				utcb.write_tpr(state.tpr.value());
				utcb.write_tpr_threshold(state.tpr_threshold.value());
			}

			if (state.fpu.valid()) {
				state.fpu.value([&] (uint8_t *fpu, size_t const) {
					asm volatile ("fxrstor %0" : : "m" (*fpu));
				});
			}
		}

		void _dispatch()
		{
			try {
				_dispatching = Thread::myself();
				/* call dispatch handler */
				_obj.dispatch(1);
				_dispatching = nullptr;
			} catch (...) {
				_dispatching = nullptr;
				throw;
			}
		}

		addr_t _sm_sel() const {
			return Nova::NUM_INITIAL_PT_RESERVED + _id.id * 4; }

		addr_t _ec_sel() const { return _sm_sel() + 1; }

		Vcpu(const Vcpu&);
		Vcpu &operator = (Vcpu const &);

	public:

		Vcpu(Vm_handler_base &o, Vm_session::Vcpu_id const id,
		     Allocator &alloc)
		: _obj(o), _alloc(alloc), _id(id) { }

		virtual ~Vcpu() { }

		Allocator &allocator() { return _alloc; }

		addr_t badge(uint16_t exit) const {
			return ((0UL + _id.id) << (sizeof(exit) * 8)) | exit; }

		Vm_session_client::Vcpu_id id() const { return _id; }

		__attribute__((regparm(1))) static void exit_entry(addr_t o)
		{
			Thread     &myself = *Thread::myself();
			Nova::Utcb &utcb   = *reinterpret_cast<Nova::Utcb *>(myself.utcb());

			uint16_t const exit_reason = o;
			unsigned const vcpu_id = o >> (sizeof(exit_reason) * 8);
			Vcpu * vcpu = nullptr;

			vcpus.for_each([&] (Vcpu &vc) {
				if (vcpu_id == vc._id.id)
					vcpu = &vc;
			});

			if (!vcpu) {
				/* somebody called us directly ? ... ignore/deny */
				utcb.items = 0;
				utcb.mtd = 0;
				Nova::reply(myself.stack_top());
			}

			/* reset blocking state */
			bool const previous_blocked = vcpu->_block;
			vcpu->_block = true;

			/* NOVA specific exit reasons */
			enum { VM_EXIT_STARTUP = 0xfe, VM_EXIT_RECALL = 0xff };

			if (exit_reason == VM_EXIT_STARTUP)
				vcpu->_ep_handler = &myself;

			Vm_state &state = *reinterpret_cast<Vm_state *>(vcpu->_state);
			/* transform state from NOVA to Genode */
			if (exit_reason != VM_EXIT_RECALL || !previous_blocked)
				_read_nova_state(utcb, state, exit_reason);

			if (exit_reason == VM_EXIT_RECALL) {
				if (previous_blocked)
					state.exit_reason = exit_reason;

				/* consume potential multiple sem ups */
				Nova::sm_ctrl(vcpu->_sm_sel(), Nova::SEMAPHORE_UP);
				Nova::sm_ctrl(vcpu->_sm_sel(), Nova::SEMAPHORE_DOWNZERO);

				if (vcpu->_remote == PAUSE) {
					vcpu->_remote = NONE;
				} else {
					if (vcpu->_remote == RUN) {
						vcpu->_remote = NONE;
						if (!previous_blocked) {
							/* still running - reply without state transfer */
							vcpu->_block = false;
							utcb.items = 0;
							utcb.mtd   = 0;
							Nova::reply(myself.stack_top());
						}
					}

					if (previous_blocked) {
						/* resume vCPU - with vCPU state update */
						vcpu->_block = false;
						_write_nova_state(utcb, state);
						Nova::reply(myself.stack_top());
					}
				}
			}

			vcpu->_dispatch();

			if (vcpu->_block) {
				/* block vCPU in kernel - no vCPU state update */
				utcb.items = 0;
				utcb.mtd   = 0;
				Nova::reply(myself.stack_top(), vcpu->_sm_sel());
			}

			/* reply to NOVA and transfer vCPU state */
			_write_nova_state(utcb, state);
			Nova::reply(myself.stack_top());
		}

		bool resume()
		{
			if (!_ep_handler) {
				/* not started yet */
				return true;
			}

			Thread * const current = Thread::myself();

			if (_dispatching == current) {
				_block = false;
				return false;
			}

			if ((_ep_handler == current) && !_block)
				return false;

			if (_ep_handler != current)
				_remote = RUN;

			Nova::ec_ctrl(Nova::EC_RECALL, _ec_sel());
			Nova::sm_ctrl(_sm_sel(), Nova::SEMAPHORE_UP);

			return false;
		}

		void pause()
		{
			Thread * const current = Thread::myself();

			if (_dispatching == current) {
				/* current thread is already dispatching */
				if (_block)
					/* issue pause exit next time - fall through */
					_block = false;
				else {
					_block = true;
					return;
				}
			}

			if ((_ep_handler == current) && _block) {
				_remote = PAUSE;
				/* already blocked */
			}

			if (_ep_handler != current)
				_remote = PAUSE;

			if (!_ep_handler) {
				/* not started yet - let startup handler issue the recall */
				return;
			}

			Nova::ec_ctrl(Nova::EC_RECALL, _ec_sel());
			Nova::sm_ctrl(_sm_sel(), Nova::SEMAPHORE_UP);
		}

		void assign_ds_state(Region_map &rm, Dataspace_capability cap) {
			_state = rm.attach(cap); }

		Nova::Mtd portal_mtd(unsigned exit, Vm_handler_base &handler)
		{
			Vm_state &state = *reinterpret_cast<Vm_state *>(_state);
			state = Vm_state {};

			if (!handler.config_vm_event(state, exit))
				return Nova::Mtd(Nova::Mtd::ALL);

			Genode::addr_t mtd = 0;

			if (state.ax.valid() || state.cx.valid() ||
			    state.dx.valid() || state.bx.valid())
				mtd |= Nova::Mtd::ACDB;

			if (state.bp.valid() || state.di.valid() || state.si.valid())
				mtd |= Nova::Mtd::EBSD;

			if (state.flags.valid())
				mtd |= Nova::Mtd::EFL;

			if (state.sp.valid())
				mtd |= Nova::Mtd::ESP;

			if (state.ip.valid())
				mtd |= Nova::Mtd::EIP;

			if (state.dr7.valid())
				mtd |= Nova::Mtd::DR;

			if (state.r8.valid()  || state.r9.valid() || state.r10.valid() ||
			    state.r11.valid() || state.r12.valid() || state.r13.valid() ||
			    state.r14.valid() || state.r15.valid())
				mtd |= Nova::Mtd::R8_R15;

			if (state.cr0.valid() || state.cr2.valid() || state.cr3.valid() ||
			    state.cr4.valid())
				mtd |= Nova::Mtd::CR;

			if (state.cs.valid() || state.ss.valid())
				mtd |= Nova::Mtd::CSSS;

			if (state.es.valid() || state.ds.valid())
				mtd |= Nova::Mtd::ESDS;

			if (state.fs.valid() || state.gs.valid())
				mtd |= Nova::Mtd::FSGS;

			if (state.tr.valid())
				mtd |= Nova::Mtd::TR;

			if (state.ldtr.valid())
				mtd |= Nova::Mtd::LDTR;

			if (state.gdtr.valid())
				mtd |= Nova::Mtd::GDTR;

			if (state.idtr.valid())
				mtd |= Nova::Mtd::IDTR;

			if (state.sysenter_cs.valid() || state.sysenter_sp.valid() ||
			    state.sysenter_ip.valid())
				mtd |= Nova::Mtd::SYS;

			if (state.ctrl_primary.valid() || state.ctrl_secondary.valid())
				mtd |= Nova::Mtd::CTRL;

			if (state.inj_info.valid() || state.inj_error.valid())
				mtd |= Nova::Mtd::INJ;

			if (state.intr_state.valid() || state.actv_state.valid())
				mtd |= Nova::Mtd::STA;

			if (state.tsc.valid() || state.tsc_offset.valid())
				mtd |= Nova::Mtd::TSC;

			if (state.efer.valid())
				mtd |= Nova::Mtd::EFER;

			if (state.pdpte_0.valid() || state.pdpte_1.valid() ||
			    state.pdpte_2.valid() || state.pdpte_3.valid())
				mtd |= Nova::Mtd::PDPTE;

			if (state.star.valid() || state.lstar.valid() ||
			    state.fmask.valid() || state.kernel_gs_base.valid())
				mtd |= Nova::Mtd::SYSCALL_SWAPGS;

			if (state.tpr.valid() || state.tpr_threshold.valid())
				mtd |= Nova::Mtd::TPR;

			if (state.qual_primary.valid() || state.qual_secondary.valid())
				mtd |= Nova::Mtd::QUAL;

			if (state.fpu.valid())
				mtd |= Nova::Mtd::FPU;

			state = Vm_state {};

			return Nova::Mtd(mtd);
		}
};

Signal_context_capability
static create_exit_handler(Pd_session &pd, Rpc_entrypoint &ep,
                           Vcpu &vcpu, unsigned exit_reason,
                           Nova::Mtd &mtd)
{
	Thread * tep = reinterpret_cast<Thread *>(&ep);

	Native_capability thread_cap = Capability_space::import(tep->native_thread().ec_sel);

	Nova_native_pd_client native_pd { pd.native_pd() };
	Native_capability vm_exit_cap = native_pd.alloc_rpc_cap(thread_cap,
	                                                        (addr_t)Vcpu::exit_entry,
	                                                        mtd.value());
	native_pd.imprint_rpc_cap(vm_exit_cap, vcpu.badge(exit_reason));

	return reinterpret_cap_cast<Signal_context>(vm_exit_cap);
}

Vm_session_client::Vcpu_id
Vm_session_client::create_vcpu(Allocator &alloc, Env &env,
                               Vm_handler_base &handler)
{
	Thread * ep = reinterpret_cast<Thread *>(&handler._rpc_ep);

	Vcpu * vcpu = new (alloc) Registered<Vcpu> (vcpus, handler,
	                                            call<Rpc_create_vcpu>(ep->cap()),
	                                            alloc);
	vcpu->assign_ds_state(env.rm(), call<Rpc_cpu_state>(vcpu->id()));

	Signal_context_capability dontcare_exit;

	enum { MAX_VM_EXITS = (1U << Nova::NUM_INITIAL_VCPU_PT_LOG2) };
	for (unsigned i = 0; i < MAX_VM_EXITS; i++) {
		Signal_context_capability signal_exit;

		Nova::Mtd mtd = vcpu->portal_mtd(i, handler);
		if (mtd.value()) {
			signal_exit = create_exit_handler(env.pd(), handler._rpc_ep,
			                                  *vcpu, i, mtd);
		} else {
			if (!dontcare_exit.valid()) {
				Nova::Mtd mtd_ip(Nova::Mtd::EIP);
				dontcare_exit = create_exit_handler(env.pd(), handler._rpc_ep,
		                                            *vcpu, 0x100, mtd_ip);
			}
			signal_exit = dontcare_exit;
		}

		call<Rpc_exception_handler>(signal_exit, vcpu->id());
	}

	return vcpu->id();
}

void Vm_session_client::run(Vm_session_client::Vcpu_id vcpu_id)
{
	vcpus.for_each([&] (Vcpu &vcpu) {
		if (vcpu.id().id != vcpu_id.id)
			return;

		if (vcpu.resume())
			call<Rpc_run>(vcpu.id());
	});
}

void Vm_session_client::pause(Vm_session_client::Vcpu_id vcpu_id)
{
	vcpus.for_each([&] (Vcpu &vcpu) {
		if (vcpu.id().id != vcpu_id.id)
			return;

		vcpu.pause();
	});
}

Dataspace_capability Vm_session_client::cpu_state(Vcpu_id vcpu_id)
{
	Dataspace_capability cap;

	vcpus.for_each([&] (Vcpu &vcpu) {
		if (vcpu.id().id == vcpu_id.id)
			cap = call<Rpc_cpu_state>(vcpu_id);
	});

	return cap;
}

Vm_session::~Vm_session()
{
	vcpus.for_each([&] (Vcpu &vc) {
		Allocator &alloc = vc.allocator();
		destroy(alloc, &vc);
	});
}
