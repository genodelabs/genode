/*
 * \brief  VM session interface test for x86
 * \author Alexander Boettcher
 * \author Christian Helmuth
 * \author Benjamin Lamowski
 * \date   2018-09-26
 *
 */

/*
 * Copyright (C) 2018-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/* Genode includes */
#include <base/attached_dataspace.h>
#include <base/attached_rom_dataspace.h>
#include <base/component.h>
#include <base/heap.h>
#include <base/signal.h>
#include <timer_session/connection.h>
#include <util/reconstructible.h>
#include <vm_session/connection.h>
#include <vm_session/handler.h>
#include <cpu/vcpu_state.h>

namespace Vmm {
	using namespace Genode;

	class Vm;
	class Vcpu;
	class Main;

	/*
	 * Note, the test implementation requires the exit values to be disjunct
	 * between Intel and AMD due to conditional not checking the used harwdare
	 * platform.
	 */
	enum class Exit : unsigned {
		INTEL_CPUID         = 0x0a,
		INTEL_HLT           = 0x0c,
		INTEL_INVALID_STATE = 0x21,
		INTEL_EPT           = 0x30,
		AMD_PF              = 0x4e,
		AMD_HLT             = 0x78,
		AMD_TRIPLE_FAULT    = 0x7f,
		AMD_NPT             = 0xfc,

		/* synthetic exits */
		STARTUP = 0xfe,
		PAUSED  = 0xff
	};

	Vm_connection::Exit_config const exit_config {
		/* ... */
	};

	static uint32_t rdtscp()
	{
		uint32_t lo = 0, hi = 0, tsc_aux = 0;
		asm volatile ("rdtscp" : "=a" (lo), "=d" (hi), "=c" (tsc_aux) :: "memory");
		return tsc_aux;
	}
}

class Vmm::Vcpu
{
	private:

		unsigned const _id;
		bool const     _svm;
		bool const     _vmx;

		Vm                  &_vm;
		Vm_connection       &_vm_con;
		Vcpu_handler<Vcpu>   _handler;
		Vm_connection::Vcpu  _vcpu;

		/* some status information to steer the test */
		enum class State {
			INITIAL,
			HALTED,
			PAUSED,
			UNKNOWN,
			RUNNING
		} _test_state { State::INITIAL };

		/* test state end */
		unsigned _exit_count     { 0 };
		unsigned _pause_count    { 0 };
		unsigned _hlt_count      { 0 };
		unsigned _timer_count    { 0 };
		unsigned _pause_at_timer { 0 };

		void _handle_vcpu_exit();
		void _cpu_init(Vcpu_state & state);

	public:

		Vcpu(unsigned const id, Entrypoint &ep, Vm_connection &vm_con,
		     Allocator &alloc, Vm &vm, bool svm, bool vmx)
		:
			_id(id), _svm(svm), _vmx(vmx),
			_vm(vm), _vm_con(vm_con),
			_handler(ep, *this, &Vcpu::_handle_vcpu_exit),
			_vcpu(_vm_con, alloc, _handler, exit_config)
		{
			log("vcpu ", _id, " : created");
		}

		unsigned id() const { return _id; }

		void skip_instruction(Vcpu_state & state, unsigned bytes)
		{
			state.ip.charge(state.ip.value() + bytes);
		}

		void force_fpu_state_transfer(Vcpu_state & state)
		{
			/* force FPU-state transfer on next entry */
			state.fpu.charge([] (Vcpu_state::Fpu::State &state) {
				/* don't change state */
				return sizeof(state);
			});
		}

		/*
		 * state information and state requests to steer the test
		 */
		bool halted() const {
			 return _test_state == State::HALTED; }
		bool paused_1st() const {
			return _test_state == State::PAUSED && _pause_count == 1; }
		bool paused_2nd() const {
			return _test_state == State::PAUSED && _pause_count == 2; }
		bool paused_3rd() const {
			return _test_state == State::PAUSED && _pause_count == 3; }
		bool paused_4th() const {
			return _test_state == State::PAUSED && _pause_count == 4; }

		void break_endless_loop()
		{
			_pause_at_timer = _timer_count + 3;
		}
		bool pause_endless_loop()
		{
			if (_pause_at_timer == 0)
				return false;

			if (_timer_count < _pause_at_timer)
				return false;

			_pause_at_timer = 0;
			return true;
		}

		template<typename FN>
		void with_state(FN const & fn)
		{
			_vcpu.with_state(fn);
		}

		void request_intercept()
		{
			_handler.local_submit();
		}

		void claim_state_unknown() { _test_state = State::UNKNOWN; }

		void timer_triggered() { _timer_count++; }
};


extern char _binary_guest_bin_start[];
extern char _binary_guest_bin_end[];

class Vmm::Vm
{
	private:

		enum { STACK_SIZE = 2*1024*sizeof(long) };

		Heap                 _heap;
		Vm_connection        _vm_con;
		bool const           _svm;
		bool const           _vmx;
		Entrypoint          &_ep_first;  /* running on first CPU */
		Entrypoint           _ep_second; /* running on second CPU */
		Vcpu                 _vcpu0;
		Vcpu                 _vcpu1;
		Vcpu                 _vcpu2;
		Vcpu                 _vcpu3;
		Dataspace_capability _memory; /* guest memory */

		/* just to trigger some events after some time */
		Timer::Connection    _timer;
		Signal_handler<Vm>   _timer_handler;
		Semaphore            _vmm_ready { 0 };

		/* trigger destruction of _vm session to test this case also */
		Signal_context_capability _signal_destruction;

		void _handle_timer();

		static bool _cpu_name(char const * name)
		{
			uint32_t cpuid = 0, edx = 0, ebx = 0, ecx = 0;
			asm volatile ("cpuid" : "+a" (cpuid), "=d" (edx), "=b"(ebx), "=c"(ecx));

			return ebx == *reinterpret_cast<uint32_t const *>(name) &&
			       edx == *reinterpret_cast<uint32_t const *>(name + 4) &&
			       ecx == *reinterpret_cast<uint32_t const *>(name + 8);
		}

		static bool _amd() { return _cpu_name("AuthenticAMD"); }
		static bool _intel() { return _cpu_name("GenuineIntel"); }

		/* lookup which hardware assisted feature the CPU supports */
		static bool _vm_feature(Env &env, char const *name)
		{
			try {
				Attached_rom_dataspace info { env, "platform_info"};

				return info.xml().sub_node("hardware").sub_node("features").attribute_value(name, false);
			} catch (...) { }

			return false;
		}

	public:

		Vm(Env &env, Signal_context_capability destruct_cap)
		:
			_heap(env.ram(), env.rm()),
			_vm_con(env),
			_svm(_amd() && _vm_feature(env, "svm")),
			_vmx(_intel() && _vm_feature(env, "vmx")),
			_ep_first(env.ep()),
			_ep_second(env, STACK_SIZE, "second ep",
			           env.cpu().affinity_space().location_of_index(1)),
			_vcpu0(0, _ep_first,  _vm_con, _heap, *this, _svm, _vmx),
			_vcpu1(1, _ep_first,  _vm_con, _heap, *this, _svm, _vmx),
			_vcpu2(2, _ep_second, _vm_con, _heap, *this, _svm, _vmx),
			_vcpu3(3, _ep_second, _vm_con, _heap, *this, _svm, _vmx),
			_memory(env.ram().alloc(4096)),
			_timer(env),
			_timer_handler(_ep_first, *this, &Vm::_handle_timer),
			_signal_destruction(destruct_cap)
		{
			if (!_svm && !_vmx) {
				error("no SVM nor VMX support detected");
				throw 0;
			}

			/* prepare guest memory with some instructions for testing */
			{
				Attached_dataspace guest { env.rm(), _memory };
				memcpy(guest.local_addr<void>(), &_binary_guest_bin_start, 4096);
			}

			/* VMM ready for all the vCPUs */
			_vmm_ready.up();
			_vmm_ready.up();
			_vmm_ready.up();
			_vmm_ready.up();

			_timer.sigh(_timer_handler);
			_timer.trigger_periodic(1000 * 1000 /* in us */);
		}

		Dataspace_capability handle_guest_memory_exit()
		{
			/*
			 * A real VMM would now have to lookup the right dataspace for
			 * the given guest physical region. This simple test has just one
			 * supported region ...
			 */
			return _memory;
		}

		void wait_until_ready() {
			_vmm_ready.down();
		}
};


/**
 * Handle timer events - used to trigger some pause/resume on vCPU for testing
 */
void Vmm::Vm::_handle_timer()
{
	_vcpu0.timer_triggered();
	_vcpu1.timer_triggered();
	_vcpu2.timer_triggered();
	_vcpu3.timer_triggered();

	/*
	 * We're running in context of _ep_first. Try to trigger remotely
	 * for vCPU2 (handled by _ep_second actually) pause/run. Remotely means
	 * that vCPU2 is not on the same physical CPU as _ep_first.
	 */
	if (_vcpu2.halted()) {
		/* test to trigger a Genode signal even if we're already blocked */
		_vcpu2.request_intercept();
	}

	if (_vcpu2.paused_1st())
		_vcpu2.request_intercept();

	if (_vcpu2.paused_2nd())
		_vcpu2.request_intercept();

	/*
	 * pause/run for vCPU0 in context of right _ep_first - meaning both
	 * are on same physical CPU.
	 */
	if (_vcpu1.pause_endless_loop()) {
		log("pause endless loop");
		/* guest in endless jmp loop - request to stop it asap */
		_vcpu1.request_intercept();
		return;
	}

	if (_vcpu1.halted()) {
		log(Thread::myself()->name(), "     : request pause of vcpu ", _vcpu1.id());
		/* test to trigger a Genode signal even if we're already blocked */
		_vcpu1.request_intercept();
	}

	if (_vcpu1.paused_1st()) {
		log(Thread::myself()->name(), "     : request resume (A) of vcpu ", _vcpu1.id());

		_vcpu1.with_state([this](Vcpu_state & state) {
			state.discharge();
			_vcpu1.force_fpu_state_transfer(state);

			/* continue after first paused state */
			return true;
		});
	} else if (_vcpu1.paused_2nd()) {
		log(Thread::myself()->name(), "     : request resume (B) of vcpu ", _vcpu1.id());

		_vcpu1.with_state([this](Vcpu_state & state) {
			state.discharge();
			/* skip over next 2 hlt instructions after second paused state */
			_vcpu1.skip_instruction(state, 2*1 /* 2x hlt instruction size */);

			/* reset state to unknown, otherwise we may enter this a second time */
			_vcpu1.claim_state_unknown();

			/* the next instruction is actually a jmp endless loop */
			return true;
		});

		/* request on the next timeout to stop the jmp endless loop */
		_vcpu1.break_endless_loop();
	} else if (_vcpu1.paused_3rd()) {
		log(Thread::myself()->name(), "     : request resume (C) of vcpu ", _vcpu1.id());

		_vcpu1.with_state([this](Vcpu_state & state) {
			state.discharge();
			_vcpu1.skip_instruction(state, 1*2 /* 1x jmp endless loop size */);
			return true;
		});
	} else if (_vcpu1.paused_4th()) {
		log("vcpu test finished - de-arm timer");
		_timer.trigger_periodic(0);

		/* trigger destruction of VM session */
		Signal_transmitter(_signal_destruction).submit();
	}
}


/**
 * Handle VM exits ...
 */
void Vmm::Vcpu::_handle_vcpu_exit()
{
	using namespace Genode;

	_vcpu.with_state([this](Vcpu_state & state) {

	Exit const exit { state.exit_reason };

	state.discharge();

	_exit_count++;

	/*
	 * Needed so that the "vcpu X: created" output
	 * comes first on foc.
	 */
	if (exit == Exit::STARTUP)
		_vm.wait_until_ready();

	log("vcpu ", _id, " : ", _exit_count, ". vm exit - ",
	    "reason ", Hex((unsigned)exit), " handled by '",
	    Thread::myself()->name(), "'");

	switch (exit) {

	case Exit::STARTUP:
		_cpu_init(state);
		break;

	case Exit::INTEL_INVALID_STATE:
		error("vcpu ", _id, " : ", _exit_count, ". vm exit - "
		      " halting vCPU - invalid guest state");
		_test_state = State::UNKNOWN;
		return false;

	case Exit::AMD_TRIPLE_FAULT:
		error("vcpu ", _id, " : ", _exit_count, ". vm exit - "
		      " halting vCPU - triple fault");
		_test_state = State::UNKNOWN;
		return false;

	case Exit::PAUSED:
		/* FIXME handle remote resume */
		if (id() == 2) {
			if (paused_1st()) {
				log(Thread::myself()->name(), "     : request resume of vcpu ", id());
				return true;
			}
			if (paused_2nd()) {
				log(Thread::myself()->name(), "     : request resume of vcpu ", id());

					/* skip over next hlt instructions after second paused state */
					skip_instruction(state, 1*1 /* 1x hlt instruction size */);

					/* reset state to unknown, otherwise we may enter this a second time */
					claim_state_unknown();

					/* the next instruction is again a hlt */
					return true;
			}
		}

		log("vcpu ", _id, " : ", _exit_count, ". vm exit - "
		    " due to pause() request - ip=", Hex(state.ip.value()));
		_pause_count++;
		_test_state = State::PAUSED;
		return false;

	case Exit::INTEL_HLT:
	case Exit::AMD_HLT:
		log("vcpu ", _id, " : ", _exit_count, ". vm exit - "
		    " halting vCPU - guest called HLT - ip=", Hex(state.ip.value()));

		if (_hlt_count == 0) {
			uint32_t const tsc_aux_host = rdtscp();

			log("vcpu ", _id, " : ", _exit_count, ". vm exit -  rdtscp cx"
			    " guest=", Hex(state.cx.value()), " host=", Hex(tsc_aux_host));
		}

		_test_state = State::HALTED;
		_hlt_count ++;
		return false;

	case Exit::INTEL_EPT:
	case Exit::AMD_NPT:
	case Exit::AMD_PF: {
			addr_t const guest_fault_addr = state.qual_secondary.value();
			addr_t const guest_map_addr   = state.qual_secondary.value() & ~0xFFFUL;

			log("vcpu ", _id, " : ", _exit_count, ". vm exit - "
			    " guest fault address: ", Hex(guest_fault_addr));

			Dataspace_capability cap = _vm.handle_guest_memory_exit();
			if (!cap.valid()) {
				error("vcpu ", _id, " : ", _exit_count, ". vm exit - "
				      " halting vCPU - guest memory lookup failed");
				_test_state = State::UNKNOWN;
				/* no memory - we halt the vcpu */
				return false;
			}
			if (guest_fault_addr != 0xfffffff0UL) {
				error("vcpu ", _id, " : ", _exit_count, ". vm exit - "
				      " unknown guest fault address");
				return false;
			}

			_vm_con.attach(cap, guest_map_addr, { 0, 0, true, true });
		}

	default: break;
	}

	if (_exit_count >= 5) {
		error("vcpu ", _id, " : ", _exit_count, ". vm exit - "
		      " halting vCPU - unknown state");
		_test_state = State::UNKNOWN;
		return false;
	}

	log("vcpu ", _id, " : ", _exit_count, ". vm exit - resume vcpu");

	_test_state = State::RUNNING;
	return true;
	});
}

void Vmm::Vcpu::_cpu_init(Vcpu_state & state)
{
	enum { INTEL_CTRL_PRIMARY_HLT = 1 << 7 };
	enum {
		INTEL_CTRL_SECOND_UG = 1 << 7,
		INTEL_CTRL_SECOND_RDTSCP_ENABLE = 1 << 3,
	};
	enum {
		AMD_CTRL_PRIMARY_HLT  = 1 << 24,
		AMD_CTRL_SECOND_VMRUN = 1 << 0
	};

	/* http://www.sandpile.org/x86/initial.htm */

	using Segment = Vcpu_state::Segment;
	using Range   = Vcpu_state::Range;

	state.flags.charge(2);
	state.ip.   charge(0xfff0);
	state.cr0.  charge(0x10);
	state.cs.   charge(Segment{0xf000, 0x93, 0xffff, 0xffff0000});
	state.ss.   charge(Segment{0, 0x93, state.cs.value().limit, 0});
	state.dx.   charge(0x600);
	state.es.   charge(Segment{0, state.ss.value().ar,
			                    state.cs.value().limit, 0});
	state.ds.   charge(Segment{0, state.ss.value().ar,
			                    state.cs.value().limit, 0});
	state.fs.   charge(Segment{0, state.ss.value().ar,
			                    state.cs.value().limit, 0});
	state.gs.   charge(Segment{0, state.ss.value().ar,
			                    state.cs.value().limit, 0});
	state.tr.   charge(Segment{0, 0x8b, 0xffff, 0});
	state.ldtr. charge(Segment{0, 0x1000, state.tr.value().limit, 0});
	state.gdtr. charge(Range  {0, 0xffff});
	state.idtr. charge(Range  {0, state.gdtr.value().limit});
	state.dr7.  charge(0x400);

	if (_vmx) {
		state.ctrl_primary.charge(INTEL_CTRL_PRIMARY_HLT);
		state.ctrl_secondary.charge(INTEL_CTRL_SECOND_UG | /* required for seL4 */
				                    INTEL_CTRL_SECOND_RDTSCP_ENABLE);
	}
	if (_svm) {
		state.ctrl_primary.charge(AMD_CTRL_PRIMARY_HLT);
		state.ctrl_secondary.charge(AMD_CTRL_SECOND_VMRUN);
	}

	/* Store id of CPU for rdtscp, similar as some OS do and some
	 * magic number to check for testing purpuse
	 */
	state.tsc_aux.charge((0xaffeU << 16) | _id);
}



class Vmm::Main
{
	private:

		Signal_handler<Main> _destruct_handler;
		Reconstructible<Vm>  _vm;

		void _destruct()
		{
			log("destruct vm session");

			_vm.destruct();

			log("vmm test finished");
		}

	public:

		Main(Env &env)
		:
			_destruct_handler(env.ep(), *this, &Main::_destruct),
			_vm(env, _destruct_handler)
		{
		}
};

void Component::construct(Genode::Env & env) { static Vmm::Main main(env); }
