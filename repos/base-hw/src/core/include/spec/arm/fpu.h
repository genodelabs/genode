/*
 * \brief  ARM-specific FPU driver for core
 * \author Stefan Kalkowski
 * \author Martin stein
 * \date   2016-01-19
 */

/*
 * Copyright (C) 2016-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

#ifndef _SPEC__ARM__FPU_H_
#define _SPEC__ARM__FPU_H_

#include <util/register.h>

namespace Genode { class Fpu; }

/**
 * FPU driver for the ARM VFPv3-D16 architecture
 */
class Genode::Fpu
{
	private:

		/**
		 * Floating-point Status and Control Register
		 */
		struct Fpscr : Register<32>
		{
			/**
			 * Read register value
			 */
			static access_t read()
			{
				/* FIXME: See annotation 1. */
				access_t v;
				asm volatile ("mrc p10, 7, %[v], cr1, cr0, 0" : [v] "=r" (v) ::);
				return v;
			}

			/**
			 * Override register value
			 *
			 * \param v  write value
			 */
			static void write(access_t const v)
			{
				/* FIXME: See annotation 1. */
				asm volatile ("mcr p10, 7, %[v], cr1, cr0, 0" :: [v] "r" (v) :);
			}
		};

		/**
		 * Floating-Point Exception Control register
		 */
		struct Fpexc : Register<32>
		{
			struct En  : Bitfield<30, 1> { };

			/**
			 * Read register value
			 */
			static access_t read()
			{
				/* FIXME: See annotation 1. */
				access_t v;
				asm volatile ("mrc p10, 7, %[v], cr8, cr0, 0" : [v] "=r" (v) ::);
				return v;
			}

			/**
			 * Override register value
			 *
			 * \param v  write value
			 */
			static void write(access_t const v)
			{
				/* FIXME: See annotation 1. */
				asm volatile ("mcr p10, 7, %[v], cr8, cr0, 0" :: [v] "r" (v) :);
			}
		};

	public:

		class Context
		{
			private:

				friend class Fpu;

				/* advanced FP/SIMD - system registers */
				uint32_t fpscr;
				uint32_t fpexc;

				/* advanced FP/SIMD - general purpose registers d0-d15 */
				uint64_t d0, d1, d2,  d3,  d4,  d5,  d6,  d7;
				uint64_t d8, d9, d10, d11, d12, d13, d14, d15;

				Fpu * _fpu = nullptr;

			public:

				Context() : fpexc(Fpexc::En::bits(1)) { }

				~Context() { if (_fpu) _fpu->unset(*this); }
		};

	private:

		Context * _context = nullptr;

		/**
		 * Enable FPU
		 */
		void _enable()
		{
			Fpexc::access_t fpexc = Fpexc::read();
			Fpexc::En::set(fpexc, 1);
			Fpexc::write(fpexc);
		}

		/**
		 * Disable FPU
		 */
		void _disable()
		{
			Fpexc::access_t fpexc = Fpexc::read();
			Fpexc::En::set(fpexc, 0);
			Fpexc::write(fpexc);
		}

		/**
		 * Save FPU context
		 */
		void _save()
		{
			/* save system registers */
			_context->fpexc = Fpexc::read();
			_context->fpscr = Fpscr::read();

			/*
			 * Save D0 - D15
			 *
			 * FIXME: See annotation 2.
			 */
			void * const d0_d15_base = &_context->d0;
			asm volatile (
				"stc p11, cr0, [%[d0_d15_base]], #128"
				:: [d0_d15_base] "r" (d0_d15_base) : );
		}

		/**
		 * Load context to FPU
		 */
		void _load()
		{
			/* load system registers */
			Fpexc::write(_context->fpexc);
			Fpscr::write(_context->fpscr);

			/*
			 * Load D0 - D15
			 *
			 * FIXME: See annotation 2.
			 */
			void * const d0_d15_base = &_context->d0;
			asm volatile (
				"ldc p11, cr0, [%[d0_d15_base]], #128"
				:: [d0_d15_base] "r" (d0_d15_base) : );
		}

		/**
		 * Return wether the FPU is enabled
		 */
		bool _enabled()
		{
			Fpexc::access_t fpexc = Fpexc::read();
			return Fpexc::En::get(fpexc);
		}

	public:

		/**
		 * Initialize FPU
		 */
		void init();

		void switch_to(Context & context)
		{
			if (_context == &context) return;
			_disable();
		}

		/**
		 * Return wether the FPU fault can be solved
		 *
		 * \param state  CPU state of the user
		 */
		bool fault(Context & context)
		{
			if (_enabled()) { return false; }
			_enable();
			if (_context != &context) {
				if (_context) {
					_save();
					_context->_fpu = nullptr;
				}
				_context = &context;
				_context->_fpu = this;
				_load();
			}
			return true;
		}

		/**
		 * Unset FPU context
		 */
		void unset(Context &context) {
			if (_context == &context) _context = nullptr; }
};

/*
 * Annotation 1
 *
 *  According to the ARMv7 manual this should be done via vmsr/vmrs instruction
 *  but it seems that binutils 2.22 doesn't fully support this yet. Hence, we
 *  use a co-processor instruction instead. The parameters to target the
 *  register this way can be determined via 'sys/arm/include/vfp.h' and
 *  'sys/arm/arm/vfp.c' of the FreeBSD head branch as from 2014.04.17.
 */

/*
 * Annotation 2
 *
 *  According to the ARMv7 manual this should be done via vldm/vstm instruction
 *  but it seems that binutils 2.22 doesn't fully support this yet. Hence, we
 *  use a co-processor instruction instead. The parameters to target the
 *  register this way can be determined via 'sys/arm/arm/vfp.c' of the FreeBSD
 *  head branch as from 2014.04.17.
 */

#endif /* _SPEC__ARM__FPU_H_ */
