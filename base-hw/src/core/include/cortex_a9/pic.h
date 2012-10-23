/*
 * \brief  Programmable interrupt controller for core
 * \author Martin stein
 * \date   2011-10-26
 */

/*
 * Copyright (C) 2009-2012 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__CORTEX_A9__PIC_H_
#define _INCLUDE__CORTEX_A9__PIC_H_

/* Genode includes */
#include <util/mmio.h>

namespace Cortex_a9
{
	using namespace Genode;

	/**
	 * Programmable interrupt controller for core
	 */
	class Pic
	{
		public:

			enum { MAX_INTERRUPT_ID = 1023 };

		protected:

			enum {
				MIN_SPI  = 32,
				SPURIOUS_ID = 1023,
			};

			/**
			 * Distributor interface
			 */
			struct Distr : public Mmio
			{
				Distr() : Mmio(Cortex_a9::Cpu::PL390_DISTRIBUTOR_MMIO_BASE) { }

				/**
				 * Distributor control register
				 */
				struct Icddcr : Register<0x000, 32>
				{
					struct Enable : Bitfield<0,1> { };
				};

				/**
				 * Interrupt controller type register
				 */
				struct Icdictr : Register<0x004, 32>
				{
					struct It_lines_number : Bitfield<0,5>  { };
					struct Cpu_number      : Bitfield<5,3>  { };
				};

				/**
				 * Interrupt security registers
				 */
				struct Icdisr : Register_array<0x80, 32, MAX_INTERRUPT_ID+1, 1>
				{
					struct Nonsecure : Bitfield<0, 1> { };
				};

				/**
				 * Interrupt set enable registers
				 */
				struct Icdiser : Register_array<0x100, 32, MAX_INTERRUPT_ID+1, 1, true>
				{
					struct Set_enable : Bitfield<0, 1> { };
				};

				/**
				 * Interrupt clear enable registers
				 */
				struct Icdicer : Register_array<0x180, 32, MAX_INTERRUPT_ID+1, 1, true>
				{
					struct Clear_enable : Bitfield<0, 1> { };
				};

				/**
				 * Interrupt priority level registers
				 */
				struct Icdipr  : Register_array<0x400, 32, MAX_INTERRUPT_ID+1, 8>
				{
					struct Priority : Bitfield<0, 8>
					{
						enum { GET_MIN_PRIORITY = 0xff };
					};
				};

				/**
				 * Interrupt processor target registers
				 */
				struct Icdiptr : Register_array<0x800, 32, MAX_INTERRUPT_ID+1, 8>
				{
					struct Cpu_targets : Bitfield<0, 8>
					{
						enum { ALL = 0xff };
					};
				};

				/**
				 * Interrupt configuration registers
				 */
				struct Icdicr : Register_array<0xc00, 32, MAX_INTERRUPT_ID+1, 2>
				{
					struct Edge_triggered : Bitfield<1, 1> { };
				};

				/**
				 * Minimum supported interrupt priority
				 */
				Icdipr::access_t min_priority()
				{
					write<Icdipr::Priority>(Icdipr::Priority::GET_MIN_PRIORITY, 0);
					return read<Icdipr::Priority>(0);
				}

				/**
				 * Maximum supported interrupt priority
				 */
				Icdipr::access_t max_priority() { return 0; }

				/**
				 * ID of the maximum supported interrupt
				 */
				Icdictr::access_t max_interrupt()
				{
					enum { LINE_WIDTH_LOG2 = 5 };
					Icdictr::access_t lnr = read<Icdictr::It_lines_number>();
					return ((lnr + 1) << LINE_WIDTH_LOG2) - 1;
				}

			} _distr;

			/**
			 * CPU interface
			 */
			struct Cpu : public Mmio
			{
				Cpu() : Mmio(Cortex_a9::Cpu::PL390_CPU_MMIO_BASE) { }

				/**
				 * CPU interface control register
				 */
				struct Iccicr : Register<0x00, 32>
				{
					/* Without security extension */
					struct Enable : Bitfield<0,1> { };

					/* With security extension */
					struct Enable_s  : Bitfield<0,1> { };
					struct Enable_ns : Bitfield<1,1> { };
					struct Ack_ctl   : Bitfield<2,1> { };
					struct Fiq_en    : Bitfield<3,1> { };
					struct Sbpr      : Bitfield<4,1> { };
				};

				/**
				 * Priority mask register
				 */
				struct Iccpmr : Register<0x04, 32>
				{
					struct Priority : Bitfield<0,8> { };
				};

				/**
				 * Binary point register
				 */
				struct Iccbpr : Register<0x08, 32>
				{
					struct Binary_point : Bitfield<0,3>
					{
						enum { NO_PREEMPTION = 7 };
					};
				};

				/**
				 * Interrupt acknowledge register
				 */
				struct Icciar : Register<0x0c, 32, true>
				{
					struct Ack_int_id : Bitfield<0,10> { };
					struct Cpu_id     : Bitfield<10,3> { };
				};

				/**
				 * End of interrupt register
				 */
				struct Icceoir : Register<0x10, 32, true>
				{
					struct Eoi_int_id : Bitfield<0,10> { };
					struct Cpu_id     : Bitfield<10,3> { };
				};

				/**
				 * Non-secure Binary point register
				 */
				struct Iccabpr : Register<0x1c, 32>
				{
					struct Binary_point : Bitfield<0,3>
					{
						enum { NO_PREEMPTION = 7 };
					};
				};

			} _cpu;

			unsigned const _max_interrupt;
			unsigned long _last_taken_request;

		public:

			/**
			 * Constructor, all interrupts get masked
			 */
			Pic() : _max_interrupt(_distr.max_interrupt()),
			        _last_taken_request(SPURIOUS_ID) { }

			/**
			 * Get the ID of the last interrupt request
			 *
			 * \return  True if the request with ID 'i' is treated as accepted
			 *          by the CPU and awaits an subsequently 'finish_request'
			 *          call. Otherwise this returns false and the value of 'i'
			 *          remains useless.
			 */
			bool take_request(unsigned & i)
			{
				_last_taken_request = _cpu.read<Cpu::Icciar::Ack_int_id>();
				i = _last_taken_request;
				return valid(i);
			}

			/**
			 * Complete the last request that was taken via 'take_request'
			 */
			void finish_request()
			{
				if (!valid(_last_taken_request)) return;
				_cpu.write<Cpu::Icceoir>(Cpu::Icceoir::Eoi_int_id::bits(_last_taken_request) |
				                         Cpu::Icceoir::Cpu_id::bits(0) );
				_last_taken_request = SPURIOUS_ID;
			}

			/**
			 * Check if 'i' is a valid interrupt request ID at the device
			 */
			bool valid(unsigned const i) const { return i <= _max_interrupt; }

			/**
			 * Unmask all interrupts
			 */
			void unmask()
			{
				for (unsigned i=0; i <= _max_interrupt; i++)
					_distr.write<Distr::Icdiser::Set_enable>(1, i);
			}

			/**
			 * Unmask interrupt 'i'
			 */
			void unmask(unsigned const i) {
				_distr.write<Distr::Icdiser::Set_enable>(1, i); }

			/**
			 * Mask all interrupts
			 */
			void mask()
			{
				for (unsigned i=0; i <= _max_interrupt; i++)
					_distr.write<Distr::Icdicer::Clear_enable>(1, i);
			}

			/**
			 * Mask interrupt 'i'
			 */
			void mask(unsigned const i) {
				_distr.write<Distr::Icdicer::Clear_enable>(1, i); }
	};
}

#endif /* _INCLUDE__CORTEX_A9__PIC_H_ */

