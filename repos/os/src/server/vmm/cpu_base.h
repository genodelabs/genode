/*
 * \brief  VMM cpu object
 * \author Stefan Kalkowski
 * \date   2019-07-18
 */

/*
 * Copyright (C) 2019 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__CPU_BASE_H_
#define _SRC__SERVER__VMM__CPU_BASE_H_

#include <exception.h>
#include <generic_timer.h>

#include <base/env.h>
#include <base/heap.h>
#include <cpu/vm_state_virtualization.h>
#include <util/mmio.h>
#include <vm_session/connection.h>

namespace Vmm {
	class Vm;
	class Cpu_base;
	Genode::Lock & lock();
}

class Vmm::Cpu_base
{
	public:

		struct State : Genode::Vm_state
		{
			Genode::uint64_t reg(unsigned idx) const;
			void reg(unsigned idx, Genode::uint64_t v);
		};

		struct Esr : Genode::Register<32>
		{
			struct Ec : Bitfield<26, 6>
			{
				enum {
					WFI     = 0x1,
					MRC_MCR = 0x3,
					HVC     = 0x16,
					MRS_MSR = 0x18,
					DA      = 0x24,
					BRK     = 0x3c
				};
			};
		};

		Cpu_base(Vm                      & vm,
		         Genode::Vm_connection   & vm_session,
		         Mmio_bus                & bus,
		         Gic                     & gic,
		         Genode::Env             & env,
		         Genode::Heap            & heap,
		         Genode::Entrypoint      & ep);

		unsigned           cpu_id() const;
		void               run();
		void               pause();
		bool               active() const;
		State &            state()  const;
		Gic::Gicd_banked & gic();
		void               dump();
		void               handle_exception();
		void               recall();
		void               initialize_boot(Genode::addr_t ip,
		                                   Genode::addr_t dtb);

		template <typename FUNC>
		void handle_signal(FUNC handler)
		{
			if (active()) {
				pause();
				handle_exception();
			}

			handler();
			_update_state();
			if (active()) run();
		}

		template <typename T>
		struct Signal_handler : Genode::Vm_handler<Signal_handler<T>>
		{
			using Base = Genode::Vm_handler<Signal_handler<T>>;

			Cpu_base & cpu;
			T        & obj;
			void (T::*member)();

			void handle()
			{
				try {
					cpu.handle_signal([this] () { (obj.*member)(); });
				} catch(Exception &e) {
					Genode::error(e);
					cpu.dump();
				}
			}

			Signal_handler(Cpu_base           & cpu,
			               Genode::Entrypoint & ep,
			               T                  & o,
			               void                 (T::*f)())
			: Base(ep, *this, &Signal_handler::handle),
			  cpu(cpu), obj(o), member(f) {}
		};

	protected:

		class System_register : public Genode::Avl_node<System_register>
		{
			private:

				const Esr::access_t       _encoding;
				const char               *_name;
				const bool                _writeable;
				Genode::uint64_t          _value;

			public:

				struct Iss : Esr
				{
					struct Direction : Bitfield<0,  1> {};
					struct Crm       : Bitfield<1,  4> {};
					struct Register  : Bitfield<5,  5> {};
					struct Crn       : Bitfield<10, 4> {};
					struct Opcode1   : Bitfield<14, 3> {};
					struct Opcode2   : Bitfield<17, 3> {};
					struct Opcode0   : Bitfield<20, 2> {};

					static access_t value(unsigned op0,
					                      unsigned crn,
					                      unsigned op1,
					                      unsigned crm,
					                      unsigned op2);

					static access_t mask_encoding(access_t v);
				};

				System_register(unsigned         op0,
				                unsigned         crn,
				                unsigned         op1,
				                unsigned         crm,
				                unsigned         op2,
				                const char     * name,
				                bool             writeable,
				                Genode::addr_t   v,
				                Genode::Avl_tree<System_register> & tree);

				System_register(unsigned         crn,
				                unsigned         op1,
				                unsigned         crm,
				                unsigned         op2,
				                const char     * name,
				                bool             writeable,
				                Genode::addr_t   v,
				                Genode::Avl_tree<System_register> & tree)
				: System_register(0, crn, op1, crm, op2,
				                  name, writeable, v, tree) {}

				const char * name()       const { return _name;      }
				const bool   writeable()  const { return _writeable; }

				System_register * find_by_encoding(Iss::access_t e)
				{
					if (e == _encoding) return this;

					System_register * r =
						Avl_node<System_register>::child(e > _encoding);
					return r ? r->find_by_encoding(e) : nullptr;
				}

				virtual void write(Genode::addr_t v) {
					_value = (Genode::addr_t)v; }

				virtual Genode::addr_t read() const {
					return (Genode::addr_t)(_value); }


				/************************
				 ** Avl node interface **
				 ************************/

				bool higher(System_register *r) {
					return (r->_encoding > _encoding); }
		};

		bool                              _active { true };
		Vm                              & _vm;
		Genode::Vm_connection           & _vm_session;
		Genode::Heap                    & _heap;
		Signal_handler<Cpu_base>          _vm_handler;
		Genode::Vm_session::Vcpu_id       _vcpu_id;
		State                           & _state;
		Genode::Avl_tree<System_register> _reg_tree;


		/***********************
		 ** Local peripherals **
		 ***********************/

		Gic::Gicd_banked                  _gic;
		Generic_timer                     _timer;

		void _handle_nothing() {}
		bool _handle_sys_reg();
		void _handle_brk();
		void _handle_wfi();
		void _handle_sync();
		void _handle_irq();
		void _handle_data_abort();
		void _handle_hyper_call();
		void _update_state();
};

#endif /* _SRC__SERVER__VMM__CPU_BASE_H_ */
