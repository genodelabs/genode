/*
 * \brief  VMM cpu object
 * \author Stefan Kalkowski
 * \author Benjamin Lamowski
 * \date   2019-07-18
 */

/*
 * Copyright (C) 2019-2023 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SRC__SERVER__VMM__CPU_BASE_H_
#define _SRC__SERVER__VMM__CPU_BASE_H_

#include <exception.h>
#include <generic_timer.h>
#include <state.h>

#include <base/env.h>
#include <base/heap.h>
#include <cpu/vcpu_state_virtualization.h>
#include <util/mmio.h>
#include <vm_session/connection.h>
#include <vm_session/handler.h>

namespace Vmm {
	class Vm;
	class Cpu_base;
}

class Vmm::Cpu_base
{
	public:

		struct Esr : Genode::Register<sizeof(addr_t)*8>
		{
			struct Ec : Bitfield<26, 6>
			{
				enum {
					WFI     = 0x1,
					MRC_MCR = 0x3,
					HVC_32  = 0x12,
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
		         Genode::Entrypoint      & ep,
		         unsigned                  cpu_id);

		unsigned           cpu_id() const;
		bool               active() const;
		Gic::Gicd_banked & gic();
		void               dump(Vcpu_state & state);
		void               handle_exception(Vcpu_state &state);
		void               recall();
		void               initialize_boot(Vcpu_state &state,
		                                   Genode::addr_t ip,
		                                   Genode::addr_t dtb);
		virtual void setup_state(Vcpu_state &) { };

		virtual ~Cpu_base() = default;

		Vcpu_state & state() {
			return _state->ref;
		}

		template<typename FN>
		void with_state(FN const & fn)
		{
			_vm_vcpu.with_state(fn);
		}

		void set_ready() {
			_cpu_ready.up();
		}

		template <typename FUNC>
		void handle_signal(FUNC handler)
		{
			_vm_vcpu.with_state([this, handler](Genode::Vcpu_state &vmstate) {
				Vmm::Vcpu_state & state = static_cast<Vmm::Vcpu_state &>(vmstate);
				_state.construct(state);

				try {
					if (active()) {
						handle_exception(state);
					}

					handler(state);
					_update_state(state);
				} catch(Exception &e) {
					Genode::error(e);
					dump(state);
					return false;
				}

				_state.destruct();
				return active();
			});
		}

		template <typename T>
		struct Signal_handler : Genode::Vcpu_handler<Signal_handler<T>>
		{
			using Base = Genode::Vcpu_handler<Signal_handler<T>>;

			Cpu_base & cpu;
			T        & obj;
			void (T::*member)();

			void handle()
			{
				cpu.handle_signal([this] (Vcpu_state &) { (obj.*member)(); });
			}

			Signal_handler(Cpu_base           & cpu,
			               Genode::Entrypoint & ep,
			               T                  & o,
			               void                 (T::*f)())
			: Base(ep, *this, &Signal_handler::handle),
			  cpu(cpu), obj(o), member(f) {}
		};

	protected:

		class System_register : protected Genode::Avl_node<System_register>
		{
			private:

				const Esr::access_t  _encoding;
				const char         * _name;
				const bool           _writeable;
				uint64_t             _value;

				friend class Avl_node<System_register>;
				friend class Avl_tree<System_register>;

				/*
				 * Noncopyable
				 */
				System_register(System_register const &);
				System_register &operator = (System_register const &);

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

				virtual ~System_register() {}

				const char * name() const { return _name;      }
				bool writeable()    const { return _writeable; }

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

		struct Vcpu_state_container { Vcpu_state &ref; };

		unsigned                               _vcpu_id;
		bool                                   _active { true };
		Vm                                   & _vm;
		Genode::Vm_connection                & _vm_session;
		Genode::Heap                         & _heap;
		Signal_handler<Cpu_base>               _vm_handler;
		Genode::Vm_connection::Exit_config     _exit_config { };
		Genode::Vm_connection::Vcpu            _vm_vcpu;
		Genode::Avl_tree<System_register>      _reg_tree {};
		Genode::Constructible<Vcpu_state_container> _state {};
		Semaphore                              _cpu_ready {};



		/***********************
		 ** Local peripherals **
		 ***********************/

		Gic::Gicd_banked                  _gic;
		Generic_timer                     _timer;

		void _handle_nothing() {}
		void _handle_startup(Vcpu_state &state);
		bool _handle_sys_reg(Vcpu_state &state);
		void _handle_brk(Vcpu_state &state);
		void _handle_wfi(Vcpu_state &state);
		void _handle_sync(Vcpu_state &state);
		void _handle_irq(Vcpu_state &state);
		void _handle_data_abort(Vcpu_state &state);
		void _handle_hyper_call(Vcpu_state &state);
		void _update_state(Vcpu_state &state);

	public:

		Vm & vm() { return _vm; }
};

#endif /* _SRC__SERVER__VMM__CPU_BASE_H_ */
