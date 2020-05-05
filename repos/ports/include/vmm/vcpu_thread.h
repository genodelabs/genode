/*
 * \brief  Utilities for implementing VMMs on Genode/NOVA
 * \author Norman Feske
 * \date   2013-08-20
 */

/*
 * Copyright (C) 2013-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _INCLUDE__VMM__VCPU_THREAD_H_
#define _INCLUDE__VMM__VCPU_THREAD_H_

/* Genode includes */
#include <base/thread.h>
#include <cpu_session/connection.h>
#include <pd_session/connection.h>
#include <region_map/client.h>
#include <cpu_thread/client.h>
#include <nova_native_cpu/client.h>

/* NOVA includes */
#include <nova/native_thread.h>
#include <nova/cap_map.h>

namespace Vmm {

	using namespace Genode;

	class Vcpu_thread;
	class Vcpu_other_pd;
	class Vcpu_same_pd;
}

class Vmm::Vcpu_thread
{
	public:

		virtual Genode::addr_t exc_base()            = 0;
		virtual void           start(Genode::addr_t) = 0;

		virtual ~Vcpu_thread() { };
};

class Vmm::Vcpu_other_pd : public Vmm::Vcpu_thread
{
	private:

		Genode::Capability<Genode::Pd_session> _pd_cap;
		Genode::Affinity::Location  _location;
		Genode::Cpu_connection     *_cpu_connection;

		Genode::addr_t _exc_pt_sel;

		/*
		 * Noncopyable
		 */
		Vcpu_other_pd(Vcpu_other_pd const &);
		Vcpu_other_pd &operator = (Vcpu_other_pd const &);

	public:

		Vcpu_other_pd(Cpu_connection * cpu_connection,
		              Genode::Affinity::Location location,
		              Genode::Capability<Genode::Pd_session> pd_cap,
		              Genode::size_t = 0 /* stack_size */)
		:
			_pd_cap(pd_cap), _location(location), _cpu_connection(cpu_connection),
			_exc_pt_sel(Genode::cap_map().insert(Nova::NUM_INITIAL_VCPU_PT_LOG2))
		{ }

		void start(Genode::addr_t sel_ec) override
		{
			using namespace Genode;

			Thread_capability vcpu_vm =
				_cpu_connection->retry_with_upgrade(Ram_quota{8*1024}, Cap_quota{2},
				                                    [&] ()
				{
					return _cpu_connection->create_thread(_pd_cap, "vCPU", _location,
					                                      Cpu_session::Weight());
				});

			/* tell parent that this will be a vCPU */
			Nova_native_cpu::Thread_type thread_type { Nova_native_cpu::Thread_type::VCPU };
			Nova_native_cpu::Exception_base exception_base { _exc_pt_sel };
			Nova_native_cpu_client native_cpu(_cpu_connection->native_cpu());
			native_cpu.thread_type(vcpu_vm, thread_type, exception_base);

			Cpu_thread_client cpu_thread(vcpu_vm);

			/*
			 * Translate vcpu_vm thread cap via current executing thread,
			 * which is used to lookup current PD to delegate VM-exit portals.
			 */
			addr_t const current = Thread::myself()->native_thread().exc_pt_sel
			                       + Nova::PT_SEL_PAGE_FAULT;
			translate_remote_pager(current, vcpu_vm.local_name());

			/* start vCPU in separate PD */
			cpu_thread.start(0, 0);

			/*
			 * Request native EC thread cap and put it next to the
			 * SM cap - see Vcpu_dispatcher->sel_sm_ec description
			 */
			addr_t const pager_pt = _exc_pt_sel + Nova::PT_SEL_PAGE_FAULT;
			request_native_ec_cap(pager_pt, sel_ec);

			/* solely needed for vcpu to request native ec cap - drop it */
			Nova::revoke(Nova::Obj_crd(pager_pt, 0));

			/* request creation of SC to let vCPU run */
			cpu_thread.resume();
		}

		Genode::addr_t exc_base() override { return _exc_pt_sel; }
};


class Vmm::Vcpu_same_pd : public Vmm::Vcpu_thread, Genode::Thread
{
	enum { WEIGHT = Genode::Cpu_session::Weight::DEFAULT_WEIGHT };

	public:

		Vcpu_same_pd(Cpu_connection * cpu_connection,
		             Genode::Affinity::Location location,
		             Genode::Capability<Genode::Pd_session>,
		             Genode::size_t stack_size)
		:
			Thread(WEIGHT, "vCPU", stack_size, Type::NORMAL, cpu_connection, location)
		{
			/* release pre-allocated selectors of Thread */
			Genode::cap_map().remove(native_thread().exc_pt_sel, Nova::NUM_INITIAL_PT_LOG2);

			/* allocate correct number of selectors */
			this->native_thread().exc_pt_sel = cap_map().insert(Nova::NUM_INITIAL_VCPU_PT_LOG2);

			/* tell generic thread code that this becomes a vCPU */
			this->native_thread().vcpu = true;
		}

		~Vcpu_same_pd()
		{
			using namespace Nova;

			revoke(Nova::Obj_crd(this->native_thread().exc_pt_sel, NUM_INITIAL_VCPU_PT_LOG2));
			cap_map().remove(this->native_thread().exc_pt_sel, NUM_INITIAL_VCPU_PT_LOG2, false);

			/* allocate selectors for ~Thread */
			this->native_thread().exc_pt_sel = cap_map().insert(Nova::NUM_INITIAL_PT_LOG2);
		}

		addr_t exc_base() override { return this->native_thread().exc_pt_sel; }

		void start(Genode::addr_t sel_ec) override
		{
			this->Thread::start();

			/*
			 * Request native EC thread cap and put it next to the
			 * SM cap - see Vcpu_dispatcher->sel_sm_ec description
			 */
			addr_t const pager_pt = exc_base() + Nova::PT_SEL_PAGE_FAULT;
			request_native_ec_cap(pager_pt, sel_ec);

			/* solely needed for vcpu to request native ec cap - drop it */
			Nova::revoke(Nova::Obj_crd(pager_pt, 0));
		}

		void entry() override { }
};

#endif /* _INCLUDE__VMM__VCPU_THREAD_H_ */
