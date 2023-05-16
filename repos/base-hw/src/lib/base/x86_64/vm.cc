/*
 * \brief  Client-side VM session interface
 * \author Alexander Boettcher
 * \author Benjamin Lamowski
 * \date   2018-08-27
 */

/*
 * Copyright (C) 2018-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#include <base/allocator.h>
#include <base/attached_dataspace.h>
#include <base/env.h>
#include <base/registry.h>
#include <base/signal.h>
#include <base/internal/capability_space.h>
#include <virtualization/extended_vcpu_state.h>
#include <kernel/interface.h>

#include <vm_session/connection.h>
#include <vm_session/handler.h>

#include <hw_native_vcpu/hw_native_vcpu.h>

using namespace Genode;

using Exit_config = Vm_connection::Exit_config;


/****************************
 ** hw vCPU implementation **
 ****************************/

struct Hw_vcpu : Rpc_client<Vm_session::Native_vcpu>, Noncopyable
{
	private:

		Attached_dataspace      _state;
		Native_capability       _kernel_vcpu { };
		Vcpu_handler_base &     _vcpu_handler;
		Thread *                _ep_handler  { nullptr };
		unsigned                _id { 0 };
		Vcpu_state              _stashed_state { };
		bool                    _need_state_update { false };
		bool                    _extra_pause { false };
		Vcpu_handler<Hw_vcpu>   _wrapper;

		void _wrapper_dispatch();
		void _prepare_pause_exit();
		void _update_charged_state(Vcpu_state & old_state, Vcpu_state & new_state);
		Capability<Native_vcpu> _create_vcpu(Vm_connection &, Vcpu_handler_base &);

	public:

		const Hw_vcpu& operator=(const Hw_vcpu &) = delete;
		Hw_vcpu(const Hw_vcpu&) = delete;

		Hw_vcpu(Env &, Vm_connection &, Vcpu_handler_base &);

		void run();
		void pause();

		Vm_state & state() { return *_state.local_addr<Vm_state>(); }
};


Hw_vcpu::Hw_vcpu(Env &env, Vm_connection &vm, Vcpu_handler_base &handler)
:
	Rpc_client<Native_vcpu>(_create_vcpu(vm, handler)),
	_state(env.rm(), vm.with_upgrade([&] () { return call<Rpc_state>(); })),
	_vcpu_handler(handler),
	_wrapper(handler.ep(), *this, &Hw_vcpu::_wrapper_dispatch)
{
	static unsigned counter = 0;
	call<Rpc_exception_handler>(_wrapper.signal_cap());
	_kernel_vcpu = call<Rpc_native_vcpu>();
	_id = counter++;
}


void Hw_vcpu::_wrapper_dispatch()
{
	/*
	 * If this is running, the VM is not. Either it hasn't, or it has been
	 * forced out and has written any state back.
	 */

	/*
	 * We run from the same EP as the orignal dispatch handler that
	 * will call run() from its dispatch loop, set _ep_handler.
	 */
	if (!_ep_handler)
			_ep_handler = Thread::myself();

	int run_state = state().run_state.value();

	/*
	 * In case the VMM dispatch method waits for a pause signal,
	 * we need a different state to make the pause() method
	 * send another signal.
	 */
	if (run_state == Vcpu_run_state::DISPATCHING)
		state().run_state.set(Vcpu_run_state::DISPATCHED);

	if (run_state == Vcpu_run_state::DISPATCHING_PAUSED)
		state().run_state.set(Vcpu_run_state::PAUSING);

	/*
	 * Dispatch the exit originating from the vCPU
	 */
	if (run_state == Vcpu_run_state::DISPATCHING        ||
	    run_state == Vcpu_run_state::DISPATCHING_PAUSED ||
	    run_state == Vcpu_run_state::PAUSED) {
			/* Call the VMM's dispatch method. */
			_vcpu_handler.dispatch(1);
			/*
			 * Dispatch will probably have called run(), but if the state is set
			 * to PAUSING it won't.
			 */
	}

	/*
	 * Dispatch a possible folded in pause signal.
	 * Note that we only the local run_state against pausing.
	 * If the DISPATCHED state was changed to PAUSING in between, pause()
	 * has sent a new signal.
	 */
	if (run_state == Vcpu_run_state::PAUSING            ||
		run_state == Vcpu_run_state::DISPATCHING_PAUSED ||
		_extra_pause) {
		Kernel::pause_vm(Capability_space::capid(_kernel_vcpu));
		_update_charged_state(_stashed_state, state());
		/*
		 * Tell run() to get any stashed state from the original dispatch.
		 * Necessary because that state is discharged when the VMM
		 * dispatches and would be lost otherwise.
		 */
		_need_state_update = true;
		_extra_pause = false;
		_prepare_pause_exit();
		state().run_state.set(Vcpu_run_state::PAUSED);
		_vcpu_handler.dispatch(1);
	}
}


void Hw_vcpu::run()
{
	if (_need_state_update) {
		_update_charged_state(state(), _stashed_state);
		_stashed_state.discharge();
		_need_state_update = false;
	}

	switch (state().run_state.value()) {
		case Vcpu_run_state::STARTUP:
			break;
		case Vcpu_run_state::DISPATCHED:
			if (_ep_handler != Thread::myself()) {
				Genode::error("Vcpu (", _id, ") run: setting run from remote CPU unsupported");
				return;
			}

			if (!state().run_state.cas(Vcpu_run_state::DISPATCHED,
				                       Vcpu_run_state::RUNNABLE))
				return; /* state changed to PAUSING */
			break;
		case Vcpu_run_state::PAUSED:
			state().run_state.set(Vcpu_run_state::RUNNABLE);
			/*
			 * It is the VMM's responsibility to be reasonable here.
			 * If Vcpu::run() is called assynchronously while the vCPU handler
			 * is still dispatching a request before pause this breaks.
			 */
			if (_ep_handler != Thread::myself())
				Genode::warning("Vcpu (", _id, ") run: asynchronous call of run()");
			break;
		case Vcpu_run_state::PAUSING:
			return;
		default:
			Genode::error("Vcpu (", _id, ") run: ignoring state ",
			              Genode::Hex(state().run_state.value()));
			return;
	}

	Kernel::run_vm(Capability_space::capid(_kernel_vcpu));
}


void Hw_vcpu::pause()
{
	switch (state().run_state.value()) {
		/*
		 * Ignore pause requests before the vCPU has started up.
		 */
		case Vcpu_run_state::STARTUP:
			return;

		/*
		 * When a pause is requested while starting or dispatching, the original
		 * exit needs to be handled before a pause exit can be injected.
		 * In these two cases it may happen be that the pause signal would be
		 * folded in with the signal from the kernel, therefore we need to make
		 * sure that the wrapper will prepare the pause exit anyway.
		 */
		case Vcpu_run_state::DISPATCHING:
			if (!state().run_state.cas(Vcpu_run_state::DISPATCHING,
			                           Vcpu_run_state::DISPATCHING_PAUSED))
				pause(); /* moved on to DISPATCHED, retry */
			return;

		/*
		 * The vCPU could run anytime. Switch to RUN_ONCE to make the kernel
		 * exit and send a signal after running.
		 * If the state has changed, it must be to running, in that case retry
		 * the pause.
		 */
		case Vcpu_run_state::RUNNABLE:
			if (!state().run_state.cas(Vcpu_run_state::RUNNABLE,
			                           Vcpu_run_state::RUN_ONCE))
			{
				pause();
				return;
			}

			_extra_pause = true;
			return;

		/*
		 * The vCPU may be running, signal that any interrupt exit is because it
		 * is forced out.
		 *
		 * If the CPU is already at the beginning of the exception handling,
		 * the handler will get two signals: whatever the normal exit would have
		 * been and the pause exit straight after, which is ok.
		 *
		 * If the state is written after it was already switched to
		 * INTERRUPTIBLE in the exit handler, we simply retry.
		 */
		case Vcpu_run_state::RUNNING:
			if (_ep_handler == Thread::myself()) {
				Genode::error("Vcpu (", _id, " ) pause: illegal state in line ", __LINE__ );
				return;
			};

			if (!state().run_state.cas(Vcpu_run_state::RUNNING,
			                           Vcpu_run_state::EXITING)) {
				pause();
				return;
			}
			break;

		/*
		 * A pause request is received when the CPU was already forced out.
		 * In this case we need to write the state back first and send the
		 * signal later. If this comes from another thread then it may be
		 * interrupted after reading the state, while the vCPU thread starts
		 * RUNNING. Therefore if the swap fails, retry the pause().
		 */
		case Vcpu_run_state::INTERRUPTIBLE:
			if (!state().run_state.cas(Vcpu_run_state::INTERRUPTIBLE,
			                           Vcpu_run_state::SYNC_FROM_VCPU))
				pause();
			return;

		/*
		 * A pause is requested while the VM has been dispatched.
		 * Send a new signal in case the VMM waits for a pause() exit
		 * before doing another run.
		 */
		case Vcpu_run_state::DISPATCHED:
			if (!state().run_state.cas(Vcpu_run_state::DISPATCHED,
			                           Vcpu_run_state::PAUSING)) {
				pause();
				return;
			}
			break;

		/*
		 * We're already pausing or paused, ignore it.
		 */
		default:
			return;
	}

	_wrapper.local_submit();
}


/*
 * Prepare a pause exit to dispatch to the VMM.
 * Because we don't do a round trip to the kernel we charge some state to keep
 * seoul happy.
 */
void Hw_vcpu::_prepare_pause_exit()
{
	state().exit_reason = 0xFF;
	state().ax.set_charged();
	state().bx.set_charged();
	state().cx.set_charged();
	state().dx.set_charged();

	state().bp.set_charged();
	state().di.set_charged();
	state().si.set_charged();

	state().flags.set_charged();

	state().sp.set_charged();

	state().ip.set_charged();
	state().ip_len.set_charged();

	state().qual_primary.set_charged();
	state().qual_secondary.set_charged();

	state().intr_state.set_charged();
	state().actv_state.set_charged();

	state().inj_info.set_charged();
	state().inj_error.set_charged();
}


/*
 * Update fields not already charged from one Vcpu_state to the other.
 */
void Hw_vcpu::_update_charged_state(Vcpu_state & old_state, Vcpu_state & new_state)
{
	if (new_state.ax.charged() || new_state.cx.charged() ||
		new_state.dx.charged() || new_state.bx.charged()) {
		old_state.ax.update(new_state.ax.value());
		old_state.cx.update(new_state.cx.value());
		old_state.dx.update(new_state.dx.value());
		old_state.bx.update(new_state.bx.value());
	}
	if (new_state.bp.charged() || new_state.di.charged() ||
		new_state.si.charged()) {
		old_state.bp.update(new_state.bp.value());
		old_state.si.update(new_state.si.value());
		old_state.di.update(new_state.di.value());
	}
	if (new_state.sp.charged()) {
		old_state.sp.update(new_state.sp.value());
	}
	if (new_state.ip.charged()) {
		old_state.ip.update(new_state.ip.value());
		old_state.ip_len.update(new_state.ip_len.value());
	}
	if (new_state.flags.charged()) {
		old_state.flags.update(new_state.flags.value());
	}
	if (new_state.es.charged() || new_state.ds.charged()) {
		old_state.es.update(new_state.es.value());
		old_state.ds.update(new_state.ds.value());
	}
	if (new_state.fs.charged() || new_state.gs.charged()) {
		old_state.fs.update(new_state.fs.value());
		old_state.gs.update(new_state.gs.value());
	}
	if (new_state.cs.charged() || new_state.ss.charged()) {
		old_state.cs.update(new_state.cs.value());
		old_state.ss.update(new_state.ss.value());
	}
	if (new_state.tr.charged()) {
		old_state.tr.update(new_state.tr.value());
	}
	if (new_state.ldtr.charged()) {
		old_state.ldtr.update(new_state.ldtr.value());
	}
	if (new_state.gdtr.charged()) {
		old_state.gdtr.update(new_state.gdtr.value());
	}
	if (new_state.idtr.charged()) {
		old_state.idtr.update(new_state.idtr.value());
	}
	if (new_state.cr0.charged() || new_state.cr2.charged() ||
		new_state.cr3.charged() || new_state.cr4.charged()) {
		old_state.cr0.update(new_state.cr0.value());
		old_state.cr2.update(new_state.cr2.value());
		old_state.cr3.update(new_state.cr3.value());
		old_state.cr4.update(new_state.cr4.value());
	}
	if (new_state.dr7.charged()) {
		old_state.dr7.update(new_state.dr7.value());
	}
	if (new_state.sysenter_cs.charged() || new_state.sysenter_sp.charged() ||
		new_state.sysenter_ip.charged()) {
		old_state.sysenter_ip.update(new_state.sysenter_ip.value());
		old_state.sysenter_sp.update(new_state.sysenter_sp.value());
		old_state.sysenter_cs.update(new_state.sysenter_cs.value());
	}
	if (new_state.ctrl_primary.charged() ||
		new_state.ctrl_secondary.charged()) {
		old_state.ctrl_primary.update(new_state.ctrl_primary.value());
		old_state.ctrl_secondary.update(new_state.ctrl_secondary.value());
	}
	if (new_state.inj_info.charged() || new_state.inj_error.charged()) {
		old_state.inj_info.update(new_state.inj_info.value());
		old_state.inj_error.update(new_state.inj_error.value());
	}
	if (new_state.intr_state.charged() || new_state.actv_state.charged()) {
		old_state.intr_state.update(new_state.intr_state.value());
		old_state.actv_state.update(new_state.actv_state.value());
	}
	if (new_state.tsc_offset.charged()) {
		old_state.tsc.update(new_state.tsc.value());
		old_state.tsc_offset.update(new_state.tsc_offset.value());
		old_state.tsc_aux.update(new_state.tsc_aux.value());
	}
	if (new_state.efer.charged()) {
		old_state.efer.update(new_state.efer.value());
	}
	if (new_state.pdpte_0.charged() || new_state.pdpte_1.charged() ||
		new_state.pdpte_2.charged() || new_state.pdpte_3.charged()) {
		old_state.pdpte_0.update(new_state.pdpte_0.value());
		old_state.pdpte_1.update(new_state.pdpte_1.value());
		old_state.pdpte_2.update(new_state.pdpte_2.value());
		old_state.pdpte_3.update(new_state.pdpte_3.value());
	}

	if (new_state.r8 .charged() || new_state.r9 .charged() ||
		new_state.r10.charged() || new_state.r11.charged() ||
		new_state.r12.charged() || new_state.r13.charged() ||
		new_state.r14.charged() || new_state.r15.charged()) {
		old_state.r8.update(new_state.r8.value());
		old_state.r9.update(new_state.r9.value());
		old_state.r10.update(new_state.r10.value());
		old_state.r11.update(new_state.r11.value());
		old_state.r12.update(new_state.r12.value());
		old_state.r13.update(new_state.r13.value());
		old_state.r14.update(new_state.r14.value());
		old_state.r15.update(new_state.r15.value());
	}
	if (new_state.star .charged() || new_state.lstar.charged() ||
		new_state.cstar.charged() || new_state.fmask.charged() ||
		new_state.kernel_gs_base.charged()) {
		old_state.star.update(new_state.star.value());
		old_state.lstar.update(new_state.lstar.value());
		old_state.cstar.update(new_state.cstar.value());
		old_state.fmask.update(new_state.fmask.value());
		old_state.kernel_gs_base.update(new_state.kernel_gs_base.value());
	}
		if (new_state.tpr.charged() || new_state.tpr_threshold.charged()) {
		old_state.tpr.update(new_state.tpr.value());
		old_state.tpr_threshold.update(new_state.tpr_threshold.value());
	}
}


Capability<Vm_session::Native_vcpu> Hw_vcpu::_create_vcpu(Vm_connection     &vm,
                                                          Vcpu_handler_base &handler)
{
	Thread &tep { *reinterpret_cast<Thread *>(&handler.rpc_ep()) };

	return vm.with_upgrade([&] () {
		return vm.call<Vm_session::Rpc_create_vcpu>(tep.cap()); });
}


/**************
 ** vCPU API **
 **************/

void         Vm_connection::Vcpu::run()   {        static_cast<Hw_vcpu &>(_native_vcpu).run(); }
void         Vm_connection::Vcpu::pause() {        static_cast<Hw_vcpu &>(_native_vcpu).pause(); }
Vcpu_state & Vm_connection::Vcpu::state() { return static_cast<Hw_vcpu &>(_native_vcpu).state(); }


Vm_connection::Vcpu::Vcpu(Vm_connection &vm, Allocator &alloc,
                          Vcpu_handler_base &handler, Exit_config const &)
:
	_native_vcpu(*new (alloc) Hw_vcpu(vm._env, vm, handler))
{ }
